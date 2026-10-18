// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#ifndef MUMBLE_MURMUR_AUDIOSIGNAL_H_
#define MUMBLE_MURMUR_AUDIOSIGNAL_H_
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <vector>
// Small, bounded signal assets. Decode/resample/encode only at startup or CLI validation.
// Future signals share this loader/encoder and supply their own fallback waveform.
class AudioSignal {
public:
	static std::vector< float > loadWav(const QString &path);
	static std::vector< float > syntheticBusy();
	static std::vector< QByteArray > encode(const std::vector< float > &pcm, int volume);
};
#endif
