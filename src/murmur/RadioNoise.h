// Copyright The Mumble-Funk Contributors. BSD-style license: see LICENSE.
#ifndef MUMBLE_MURMUR_RADIONOISE_H_
#define MUMBLE_MURMUR_RADIONOISE_H_
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

// Rausch producer for AudioFeedback's startup-created, reusable worker pool.
// One encoder per independently interruptible recipient. Share immutable PCM and
// choose ONE start per transmission, passing it to all recipient producers.
// Construction, pause/resume, error reporting and destruction are CONTROL-thread
// operations. take()/tryPause()/tryRestart() never wait for a lock.
// Never construct/join/encode in speech.
class RadioNoise {
public:
    using PCM = std::shared_ptr<const std::vector<float>>;
    // frame is producer-local (10-ms units), NOT AudioFeedback's output sequence.
    // epoch marks a new codec segment; it does not reset a remote decoder.
    // AudioFeedback gates delivery and retires the old segment before re-entry.
    struct Packet { QByteArray opus; std::uint64_t epoch, frame; std::size_t firstSample; };
    enum { FrameSamples = 960, QueueLimit = 3, MaxInstances = 16 };
    static PCM load(const QString &path);
    static std::size_t chooseStart(std::size_t samples, std::mt19937 &random);
    RadioNoise(PCM pcm, int volume, std::size_t start, bool paused = false);
    ~RadioNoise();
    RadioNoise(const RadioNoise &) = delete;
    RadioNoise &operator=(const RadioNoise &) = delete;
    bool take(Packet &packet); // Empty/paused/contended => omit effect; never wait.
    bool tryPause();
    bool tryRestart(std::size_t start); // Reusable pool: no threads created or joined.
    void pause();              // Invalidates queued and in-flight packets.
    void resume();             // Same next PCM sample, NEW Opus decoder segment.
    bool current(std::uint64_t epoch) const;
    std::size_t buffered() const;
    QString error() const;
private:
    void run() noexcept;
    PCM m_pcm;
    float m_gain;
    mutable std::mutex m_mutex;
    std::condition_variable m_ready;
    std::deque<Packet> m_packets;
    bool m_active = false, m_stop = false;
    std::uint64_t m_epoch = 1, m_frame = 0;
    std::size_t m_cursor, m_taken;
    QString m_error;
    std::thread m_worker;
    static std::atomic<unsigned> s_instances;
};
#endif
