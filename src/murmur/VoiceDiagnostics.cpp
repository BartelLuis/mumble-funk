// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file at the root of the Mumble source tree.

#include "VoiceDiagnostics.h"

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QUuid>
#include <QtCore/QtGlobal>

#include <algorithm>
#include <utility>

namespace {
QJsonObject context(const VoiceDiagnosticRecord &record) {
	QJsonObject object;
	object.insert(QStringLiteral("connection"), QString::number(record.connection));
	object.insert(QStringLiteral("session"), static_cast< double >(record.session));
	object.insert(QStringLiteral("user"), record.userName);
	object.insert(QStringLiteral("channel"), record.channel);
	object.insert(QStringLiteral("channel_name_at_join"), record.channelName);
	object.insert(QStringLiteral("target"), static_cast< double >(record.target));
	return object;
}
} // namespace

VoiceDiagnosticTracker::VoiceDiagnosticTracker(std::int64_t timeoutUs, Sink sink)
	: m_timeoutUs(timeoutUs), m_sink(std::move(sink)) {
}

void VoiceDiagnosticTracker::emitEvent(const QString &event, const Stream &stream, std::int64_t timeUs,
									   const QString &reason) {
	QJsonObject object = context(stream.last);
	object.insert(QStringLiteral("event"), event);
	object.insert(QStringLiteral("time_us"), QString::number(timeUs));
	object.insert(QStringLiteral("stream"), QString::number(stream.id));
	if (!reason.isEmpty()) {
		object.insert(QStringLiteral("reason"), reason);
	}
	m_sink(std::move(object));
}

void VoiceDiagnosticTracker::expire(std::int64_t nowUs) {
	for (auto it = m_streams.begin(); it != m_streams.end();) {
		Stream &stream = it->second;
		if (nowUs - stream.last.timeUs >= m_timeoutUs) {
			if (stream.active) {
				emitEvent(QStringLiteral("stream_end_timeout"), stream, stream.last.timeUs + m_timeoutUs,
						  QStringLiteral("suspected_only"));
				stream.active = false;
			}
			// Retain recent packet gaps; discard abandoned/disconnected history eventually.
			if (nowUs - stream.last.timeUs >= std::max< std::int64_t >(60000000, m_timeoutUs * 2)) {
				it = m_streams.erase(it);
				continue;
			}
		}
		++it;
	}
}

void VoiceDiagnosticTracker::interrupt(std::int64_t nowUs, const QString &reason) {
	for (const auto &entry : m_streams) {
		if (entry.second.active) {
			emitEvent(QStringLiteral("stream_interrupted"), entry.second, nowUs, reason);
		}
	}
	m_streams.clear();
}

void VoiceDiagnosticTracker::process(const VoiceDiagnosticRecord &record) {
	auto it = m_streams.find(record.connection);
	if (it != m_streams.end() && it->second.active && record.timeUs - it->second.last.timeUs >= m_timeoutUs) {
		emitEvent(QStringLiteral("stream_end_timeout"), it->second, it->second.last.timeUs + m_timeoutUs,
				  QStringLiteral("suspected_only"));
		it->second.active = false;
	}
	if (record.kind != VoiceDiagnosticRecord::Kind::Packet) {
		const QString reason = record.kind == VoiceDiagnosticRecord::Kind::Disconnected
								   ? QStringLiteral("disconnected")
								   : QStringLiteral("channel_changed");
		if (it != m_streams.end()) {
			if (it->second.active) {
				emitEvent(QStringLiteral("stream_interrupted"), it->second, record.timeUs, reason);
			}
			m_streams.erase(it);
		}
		Stream event;
		event.last = record;
		emitEvent(reason, event, record.timeUs);
		return;
	}

	QJsonValue gap;
	if (it != m_streams.end()) {
		gap = QString::number(record.timeUs - it->second.last.timeUs);
		if (it->second.last.channel != record.channel || it->second.last.target != record.target) {
			if (it->second.active) {
				emitEvent(QStringLiteral("stream_interrupted"), it->second, record.timeUs,
						  QStringLiteral("context_changed"));
			}
			m_streams.erase(it);
			it = m_streams.end();
		}
	}
	if (it == m_streams.end()) {
		if (m_streams.size() >= 16384) {
			interrupt(record.timeUs, QStringLiteral("state_capacity"));
		}
		it = m_streams.emplace(record.connection, Stream{}).first;
	}
	Stream &stream        = it->second;
	const bool olderFrame = stream.active && record.frame < stream.highestFrame;
	if (!stream.active && record.payloadBytes != 0) {
		stream.active       = true;
		stream.id           = ++m_nextStream;
		stream.highestFrame = record.frame;
		stream.last         = record;
		emitEvent(QStringLiteral("stream_start"), stream, record.timeUs);
	}
	stream.highestFrame = std::max(stream.highestFrame, record.frame);
	stream.last         = record;
	QJsonObject packet  = context(record);
	packet.insert(QStringLiteral("event"), QStringLiteral("packet"));
	packet.insert(QStringLiteral("time_us"), QString::number(record.timeUs));
	packet.insert(QStringLiteral("stream"), QString::number(stream.id));
	packet.insert(QStringLiteral("gap_us"), gap);
	packet.insert(QStringLiteral("transport"), record.transport == VoiceDiagnosticRecord::Transport::UDP
												   ? QStringLiteral("udp")
												   : QStringLiteral("tcp"));
	packet.insert(QStringLiteral("frame"), QString::number(record.frame));
	packet.insert(QStringLiteral("codec"), record.codec);
	packet.insert(QStringLiteral("payload_bytes"), static_cast< double >(record.payloadBytes));
	packet.insert(QStringLiteral("is_last_frame"), record.last);
	packet.insert(QStringLiteral("older_frame"), olderFrame);
	m_sink(std::move(packet));
	if (record.last) {
		emitEvent(QStringLiteral("end_marker"), stream, record.timeUs,
				  olderFrame ? QStringLiteral("older_frame") : QStringLiteral("received"));
		// Frame positions cannot identify PTT presses. Be conservative with late markers.
		if (stream.active && !olderFrame) {
			emitEvent(QStringLiteral("stream_end_marker"), stream, record.timeUs, QStringLiteral("marker_confirmed"));
			stream.active = false;
		}
	}
}

VoiceDiagnostics::VoiceDiagnostics(Config config) : m_config(std::move(config)), m_started(Clock::now()) {
	m_config.timeoutMs     = std::max(50, std::min(m_config.timeoutMs, 60000));
	m_config.maxBytes      = std::max< std::int64_t >(4096, m_config.maxBytes);
	m_config.queueCapacity = std::max< std::size_t >(1, m_config.queueCapacity);
	m_queue.resize(m_config.queueCapacity);
	m_writer = std::thread(&VoiceDiagnostics::run, this);
}

VoiceDiagnostics::~VoiceDiagnostics() {
	m_accepting.store(false);
	{
		std::lock_guard< std::mutex > lock(m_mutex);
		m_stopping = true;
	}
	m_ready.notify_one();
	m_writer.join();
}

std::int64_t VoiceDiagnostics::nowUs() const {
	return std::chrono::duration_cast< std::chrono::microseconds >(Clock::now() - m_started).count();
}

bool VoiceDiagnostics::submit(VoiceDiagnosticRecord record) {
	if (!m_accepting.load(std::memory_order_relaxed)) {
		return false;
	}
	std::unique_lock< std::mutex > lock(m_mutex, std::try_to_lock);
	if (!lock.owns_lock() || m_size == m_queue.size()) {
		m_dropped.fetch_add(1, std::memory_order_relaxed);
		return false;
	}
	// Timestamp and enqueue share a lock, so the two producers have a common order.
	record.timeUs                               = nowUs();
	m_queue[(m_head + m_size) % m_queue.size()] = std::move(record);
	++m_size;
	lock.unlock();
	m_ready.notify_one();
	return true;
}

void VoiceDiagnostics::run() {
	QFile file(m_config.filePath);
	const QString runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
	bool failed         = false;
	const auto open     = [&]() {
        return file.open(QIODevice::WriteOnly | QIODevice::Append)
               && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
	};
	if (!open()) {
		m_accepting.store(false);
		qWarning("Voice diagnostics: cannot open %s: %s", qPrintable(file.fileName()), qPrintable(file.errorString()));
		return;
	}
	qint64 bytesWritten = file.size();
	const auto write    = [&](QJsonObject object) {
        if (failed) {
            return;
        }
        object.insert(QStringLiteral("run"), runId);
        object.insert(QStringLiteral("server"), m_config.server);
        QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
        if (bytesWritten + line.size() > m_config.maxBytes) {
            file.close();
            const QString backup = file.fileName() + QStringLiteral(".1");
            if ((QFile::exists(backup) && !QFile::remove(backup)) || !file.rename(backup)) {
                failed = true;
            } else {
                file.setFileName(m_config.filePath);
                failed       = !open();
                bytesWritten = 0;
            }
        }
        if (!failed) {
            const qint64 written = file.write(line);
            failed               = written != line.size();
            bytesWritten += written;
        }
        if (failed) {
            m_accepting.store(false);
            qWarning("Voice diagnostics: output failed for %s; diagnostics stopped", qPrintable(m_config.filePath));
        }
	};
	QJsonObject started;
	started.insert(QStringLiteral("event"), QStringLiteral("diagnostics_started"));
	started.insert(QStringLiteral("started_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
	started.insert(QStringLiteral("time_us"), QString::number(nowUs()));
	started.insert(QStringLiteral("timeout_ms"), m_config.timeoutMs);
	write(std::move(started));
	VoiceDiagnosticTracker tracker(static_cast< std::int64_t >(m_config.timeoutMs) * 1000, write);
	std::vector< VoiceDiagnosticRecord > batch;
	batch.reserve(m_queue.size());
	std::uint64_t droppedTotal  = 0;
	Clock::time_point lastFlush = Clock::now();
	while (!failed) {
		bool stopping;
		{
			std::unique_lock< std::mutex > lock(m_mutex);
			m_ready.wait_for(lock, std::chrono::milliseconds(25), [&]() { return m_size != 0 || m_stopping; });
			while (m_size != 0) {
				batch.push_back(std::move(m_queue[m_head]));
				m_head = (m_head + 1) % m_queue.size();
				--m_size;
			}
			stopping = m_stopping;
		}
		const std::uint64_t dropped = m_dropped.exchange(0);
		if (dropped != 0) {
			droppedTotal += dropped;
			const std::int64_t lossTime = batch.empty() ? nowUs() : batch.front().timeUs;
			tracker.interrupt(lossTime, QStringLiteral("diagnostic_loss"));
			QJsonObject loss;
			loss.insert(QStringLiteral("event"), QStringLiteral("diagnostic_loss"));
			loss.insert(QStringLiteral("time_us"), QString::number(lossTime));
			loss.insert(QStringLiteral("dropped_records"), QString::number(dropped));
			write(std::move(loss));
		}
		for (const VoiceDiagnosticRecord &record : batch) {
			tracker.process(record);
		}
		batch.clear();
		std::int64_t watermark;
		{
			std::lock_guard< std::mutex > lock(m_mutex);
			// Do not expire a stream past an already queued packet's observation time.
			watermark = m_size == 0 ? nowUs() : m_queue[m_head].timeUs;
		}
		tracker.expire(watermark);
		if (stopping || Clock::now() - lastFlush >= std::chrono::milliseconds(100)) {
			if (!file.flush()) {
				m_accepting.store(false);
				qWarning("Voice diagnostics: flush failed for %s; diagnostics stopped", qPrintable(m_config.filePath));
				failed = true;
			}
			lastFlush = Clock::now();
		}
		if (stopping) {
			break;
		}
	}
	if (!failed) {
		tracker.interrupt(nowUs(), QStringLiteral("server_stopped"));
		QJsonObject stopped;
		stopped.insert(QStringLiteral("event"), QStringLiteral("diagnostics_stopped"));
		stopped.insert(QStringLiteral("time_us"), QString::number(nowUs()));
		stopped.insert(QStringLiteral("dropped_records"), QString::number(droppedTotal));
		write(std::move(stopped));
		if (!file.flush()) {
			qWarning("Voice diagnostics: final flush failed for %s", qPrintable(m_config.filePath));
		}
	}
}
