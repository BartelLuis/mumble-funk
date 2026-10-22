// Copyright The Mumble-Funk Contributors. BSD-style license: see LICENSE.
#include "AudioFeedback.h"
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtEndian>
#include <array>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <opus.h>
#include <stdexcept>
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
using Plan = SquelchPlan;
static QString asset(const char *name) { return QStringLiteral(AUDIO_ASSET_DIR) + '/' + name; }
struct Heard { AudioFeedback::Recipient recipient; QByteArray bytes; std::uint64_t frame; bool last; };
struct Rig {
    std::int64_t now = 0;
    Plan plan{true, [this] { return now; }};
    AudioFeedback feedback{50};
    QTemporaryDir dir;
    Plan::Identity source{1, 11, 0};
    std::vector<Plan::Identity> receivers{{2, 12, 0}, {3, 13, 0}};
    std::vector<Heard> heard;
    bool open;
    Rig(bool squelch = true, int count = 2) : open(squelch) {
        QByteArray wav(44 + 1007 * 2, 0);
        auto w16 = [&](int at, quint16 x) { qToLittleEndian(x, wav.data() + at); };
        auto w32 = [&](int at, quint32 x) { qToLittleEndian(x, wav.data() + at); };
        wav.replace(0, 4, "RIFF"); wav.replace(8, 8, "WAVEfmt "); wav.replace(36, 4, "data");
        w32(4, quint32(wav.size() - 8)); w32(16, 16); w16(20, 1); w16(22, 1);
        w32(24, 48000); w32(28, 96000); w16(32, 2); w16(34, 16); w32(40, 2014);
        for (int i = 0; i < 1007; ++i) w16(44 + 2 * i, quint16(i * 19 % 5000));
        QFile f(dir.path() + "/noise.wav"); CHECK(f.open(QIODevice::WriteOnly)); CHECK(f.write(wav) == wav.size()); f.close();
        CHECK(feedback.configureNoise(f.fileName(), 20));
        if (open) feedback.configureSquelch(asset("squelch-open.wav"), asset("squelch-close.wav"), 50, 50);
        receivers.clear(); for (int i = 0; i < count; ++i) receivers.push_back({std::uint64_t(i + 2), unsigned(i + 12), 0});
    }
    void queue(const std::vector<Plan::Cue> &cues) {
        for (const auto &cue : cues) {
            if (cue.kind == Plan::Kind::Open) CHECK(feedback.requestNoise(cue));
            if (open) CHECK(feedback.requestSquelch(cue));
            else if (cue.kind == Plan::Kind::Open) CHECK(plan.consume(cue, cue.sender, cue.recipient, true));
        }
    }
    void start(bool last = false) { queue(plan.observe(source, true, 0, true, last, true, receivers)); }
    void end() { queue(plan.observe(source, true, 2, true, true, true, receivers)); }
    void tick(int count = 1) {
        for (int i = 0; i < count; ++i) {
            const auto before = heard.size();
            feedback.tick(now, [&](const AudioFeedback::Recipient &r, const QByteArray &p, std::uint64_t frame, bool last) {
                for (auto it = heard.rbegin(); it != heard.rend(); ++it) if (it->recipient.connection == r.connection) {
                    CHECK(frame > it->frame); break;
                }
                heard.push_back({r, p, frame, last}); return true;
            }, [&](const Plan::Cue &cue, bool starting) {
                return starting ? plan.consume(cue, cue.sender, cue.recipient, true) : plan.current(cue);
            });
            int noisePackets = 0;
            for (std::size_t j = before; j < heard.size(); ++j) {
                bool clip = false;
                for (auto signal : {AudioFeedback::Signal::Busy, AudioFeedback::Signal::SquelchOpen, AudioFeedback::Signal::SquelchClose}) {
                    const auto &packets = feedback.clip(signal);
                    clip |= std::find(packets.begin(), packets.end(), heard[j].bytes) != packets.end();
                }
                if (!clip) ++noisePackets;
            }
            CHECK(noisePackets <= RadioNoise::MaxInstances);
            now += 20000; std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    std::vector<Heard> forUser(std::uint64_t c) {
        std::vector<Heard> out; for (const auto &h : heard) if (h.recipient.connection == c) out.push_back(h); return out;
    }
    void decode() {
        std::map<std::uint64_t, OpusDecoder *> decoders;
        for (const auto &h : heard) {
            auto &decoder = decoders[h.recipient.connection]; int error = 0;
            if (!decoder) decoder = opus_decoder_create(48000, 1, &error);
            CHECK(decoder && !error); std::array<float, 960> pcm;
            CHECK(opus_decode_float(decoder, reinterpret_cast<const unsigned char *>(h.bytes.data()), h.bytes.size(), pcm.data(), 960, 0) == 960);
            if (h.last) { opus_decoder_destroy(decoder); decoder = nullptr; }
        }
        for (auto &entry : decoders) if (entry.second) opus_decoder_destroy(entry.second);
    }
};
int main() {
    try {
        {
            Rig r; CHECK(r.plan.observe(r.source, false, 0, true, false, true, r.receivers).empty());
            r.tick(5); CHECK(r.heard.empty()); r.start(); r.tick(20);
            const auto a = r.forUser(2), b = r.forUser(3); CHECK(a.size() > 5 && b.size() > 5);
            CHECK(a[2].bytes == b[2].bytes); // Shared ORIGINAL random start, separate encoders.
            const auto open = r.feedback.clip(AudioFeedback::Signal::SquelchOpen);
            CHECK(a[0].bytes == open[0] && a[1].bytes == open[1] && a[1].last);
            r.end(); r.tick(15); const auto ended = r.forUser(2);
            const auto close = r.feedback.clip(AudioFeedback::Signal::SquelchClose);
            CHECK(ended.size() >= close.size());
            for (std::size_t i = 0; i < close.size(); ++i) CHECK(ended[ended.size() - close.size() + i].bytes == close[i]);
            CHECK(ended.back().last); const auto size = r.heard.size(); r.tick(10); CHECK(r.heard.size() == size); r.decode();
        }
        {
            Rig r(false); r.start(); r.tick(10); auto before = r.forUser(2); CHECK(before.size() > 2);
            CHECK(r.feedback.request(AudioFeedback::Signal::Busy, {2, 12, 0}, r.now)); r.tick(24);
            auto after = r.forUser(2); const auto busy = r.feedback.clip(AudioFeedback::Signal::Busy);
            auto first = std::find_if(after.begin(), after.end(), [&](const Heard &h) { return h.bytes == busy[0]; });
            CHECK(first != after.end() && first != after.begin() && (first - 1)->last);
            for (std::size_t i = 0; i < busy.size(); ++i) CHECK((first + std::ptrdiff_t(i))->bytes == busy[i]);
            CHECK(std::size_t(after.end() - first) > busy.size()); // Noise resumed, no new start event.
            CHECK(r.forUser(3).size() > before.size()); r.plan.invalidate(1); r.tick(3);
            const auto stopped = r.heard.size(); r.tick(5); CHECK(r.heard.size() == stopped); r.decode();
        }
        {
            Rig r; r.start(); r.tick(10); r.queue(r.plan.timeout(r.source, 0));
            CHECK(r.plan.timeout(r.source, 0).empty()); r.tick(15);
            auto a = r.forUser(2); CHECK(a.back().last);
            const auto close = r.feedback.clip(AudioFeedback::Signal::SquelchClose);
            CHECK(std::count_if(a.begin(), a.end(), [&](const Heard &h) { return h.bytes == close[0]; }) == 1);
        }
        {
            Rig r; r.start(true); r.tick(20); const auto a = r.forUser(2);
            CHECK(a.size() == 6 && a.back().last); // Open + Close only, no noise for ended short burst.
        }
        {
            Rig r(false); r.start(); r.tick(7); r.plan.invalidate(2); r.feedback.cancel(2); r.tick(3);
            const auto count = r.forUser(2).size(); r.tick(5); CHECK(r.forUser(2).size() == count);
            r.plan.invalidate(1); r.tick(5); r.source.connection = 8; r.start(); r.tick(8);
            CHECK(r.forUser(3).size() > 8); // New generation, same channel/session is not old work.
        }
        {
            Rig r(false); r.start(); r.tick(10);
            CHECK(r.feedback.request(AudioFeedback::Signal::Busy, {2, 12, 0}, r.now)); r.tick(5);
            r.plan.invalidate(1); r.tick(25);
            auto packets = r.forUser(2); auto busy = r.feedback.clip(AudioFeedback::Signal::Busy);
            CHECK(packets.back().bytes == busy.back() && packets.back().last);
            auto size = packets.size(); r.tick(8); CHECK(r.forUser(2).size() == size);
        }
        {
            Rig r(true, 17); r.start(); r.tick(20);
            int noisy = 0; for (const auto &who : r.receivers) if (r.forUser(who.connection).size() > 2) ++noisy;
            CHECK(noisy > 0); // Optional underruns may rotate slots; per-tick cap is asserted above.
            r.end(); r.tick(18);
            for (const auto &who : r.receivers) { auto h = r.forUser(who.connection); CHECK(h.size() >= 6 && h.back().last); }
        }
        std::cout << "Noise scheduler: accepted/denied, shared start, Open/Close, Busy/resume, timeout, short bursts, lifecycle, 16-slot cap: OK\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
