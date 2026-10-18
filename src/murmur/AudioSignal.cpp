// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#include "AudioSignal.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <opus.h>
#include <stdexcept>

std::vector< float > AudioSignal::loadWav(const QString &path) {
	const auto fail = [] {
		throw std::runtime_error(
			"Expected PCM WAV: mono, signed 16-bit LE, 8000/16000/24000/48000 Hz, 20-1000 ms, at most 128 KiB");
	};
	QFile file(path);
	if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly) || file.size() > 131072)
		fail();
	const QByteArray bytes = file.read(131073);
	if (bytes.size() < 44 || bytes.size() > 131072 || bytes.mid(0, 4) != "RIFF" || bytes.mid(8, 4) != "WAVE")
		fail();
	auto u32 = [&](int pos) { return qFromLittleEndian< quint32 >(bytes.constData() + pos); };
	auto u16 = [&](int pos) { return qFromLittleEndian< quint16 >(bytes.constData() + pos); };
	if (u32(4) != static_cast< quint32 >(bytes.size() - 8))
		fail();
	int fmt = -1, data = -1;
	quint32 size = 0;
	for (int pos = 12; pos < bytes.size();) {
		if (bytes.size() - pos < 8)
			fail();
		const auto length = u32(pos + 4);
		if (length > static_cast< quint32 >(bytes.size() - pos - 8))
			fail();
		const auto name = bytes.mid(pos, 4);
		if (name == "fmt ") {
			if (fmt >= 0 || length != 16)
				fail();
			fmt = pos + 8;
		}
		if (name == "data") {
			if (data >= 0)
				fail();
			data = pos + 8;
			size = length;
		}
		const auto step = 8 + length + (length & 1);
		if (step > static_cast< quint32 >(bytes.size() - pos))
			fail();
		pos += static_cast< int >(step);
	}
	if (fmt < 0 || data < 0 || u16(fmt) != 1 || u16(fmt + 2) != 1 || u16(fmt + 14) != 16 || u16(fmt + 12) != 2
		|| size % 2)
		fail();
	const auto rate = u32(fmt + 4);
	if ((rate != 8000 && rate != 16000 && rate != 24000 && rate != 48000) || u32(fmt + 8) != rate * 2
		|| size < rate / 50 * 2 || size > rate * 2)
		fail();
	const auto frames = size / 2;
	const auto output = static_cast< std::size_t >(frames) * 48000 / rate;
	std::vector< float > pcm(output + 960, 0); // One silent 20 ms tail for the Opus lookahead.
	for (std::size_t i = 0; i < output; ++i) {
		const double source = static_cast< double >(i) * rate / 48000.0;
		const auto first    = static_cast< quint32 >(source);
		const auto second   = std::min(first + 1, frames - 1);
		auto sample         = [&](quint32 index) {
            return static_cast< float >(
                       qFromLittleEndian< qint16 >(bytes.constData() + data + static_cast< int >(index * 2)))
                   / 32768.0f;
		};
		const float fraction = static_cast< float >(source - first);
		pcm[i]               = sample(first) * (1 - fraction) + sample(second) * fraction;
	}
	return pcm;
}

std::vector< float > AudioSignal::syntheticBusy() {
	std::vector< float > pcm(12480, 0); // 100 ms tone, 40 ms gap, 100 ms tone, 20 ms tail.
	for (std::size_t sample = 0; sample < pcm.size(); ++sample) {
		const int at    = static_cast< int >(sample);
		const int local = at < 4800 ? at : at - 6720;
		if ((at < 4800 || at >= 6720) && local >= 0 && local < 4800) {
			const double ramp = std::min(1.0, std::min(local, 4799 - local) / 240.0);
			pcm[sample]       = static_cast< float >(0.2 * ramp * std::sin(2.0 * 3.141592653589793 * 600 * at / 48000));
		}
	}
	return pcm;
}

std::vector< QByteArray > AudioSignal::encode(const std::vector< float > &pcm, int volume) {
	if (pcm.empty() || pcm.size() > 48960)
		throw std::runtime_error("Feedback PCM length outside limits");
	int error = OPUS_OK;
	std::unique_ptr< OpusEncoder, decltype(&opus_encoder_destroy) > encoder(
		opus_encoder_create(48000, 1, OPUS_APPLICATION_AUDIO, &error), opus_encoder_destroy);
	if (!encoder || error != OPUS_OK || opus_encoder_ctl(encoder.get(), OPUS_SET_BITRATE(24000)) != OPUS_OK)
		throw std::runtime_error("Cannot initialize feedback Opus encoder");
	float peak = 0;
	for (float sample : pcm) {
		if (!std::isfinite(sample) || std::abs(sample) > 1)
			throw std::runtime_error("Feedback PCM level outside limits");
		peak = std::max(peak, std::abs(sample));
	}
	// Only attenuate: cap the PCM peak at 0.2 (-14 dBFS), then apply 0..100% volume.
	const float gain =
		(peak > 0.2f ? 0.2f / peak : 1.0f) * static_cast< float >(std::max(0, std::min(100, volume))) / 100;
	std::vector< QByteArray > result;
	for (std::size_t offset = 0; offset < pcm.size(); offset += 960) {
		std::array< float, 960 > frame{};
		for (std::size_t i = 0; i < 960 && offset + i < pcm.size(); ++i)
			frame[i] = pcm[offset + i] * gain;
		std::array< unsigned char, 512 > bytes{};
		const int size =
			opus_encode_float(encoder.get(), frame.data(), 960, bytes.data(), static_cast< opus_int32 >(bytes.size()));
		if (size < 0)
			throw std::runtime_error("Cannot encode feedback Opus audio");
		result.emplace_back(reinterpret_cast< const char * >(bytes.data()), size);
	}
	return result;
}
