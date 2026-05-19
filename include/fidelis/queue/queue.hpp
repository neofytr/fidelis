// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FIDELIS_QUEUE_QUEUE_HPP
#define FIDELIS_QUEUE_QUEUE_HPP

#include <fidelis/engine/engine.hpp>

#include <filesystem>
#include <mutex>
#include <vector>

namespace fidelis::queue {

// Given the queue's current index, the track list, and the path the engine
// just reported loaded, return the index the queue should now point at. On a
// gapless swap the engine advances into the staged next track without the
// queue knowing; detect that (loaded == tracks[current+1]) and catch up, else
// the next preload restages the same track and it repeats forever. Pure and
// side-effect free so it can be unit-tested without an Engine.
int resolve_loaded_index(int current_index,
                         const std::vector<std::filesystem::path>& tracks,
                         const std::filesystem::path& loaded);

// Owns the playback queue and drives the engine's preload() API for gapless
// continuation. Thread-safe: all public methods may be called from any thread.
//
// Usage: construct after Engine::create(), pass on_event() as the engine event
// callback, then call append/jump/etc. to manipulate the queue. The queue
// advances automatically on TrackEnded and preloads the next track on
// TrackLoaded so the engine's gapless decoder-swap fires at EOF.
class Queue {
public:
    explicit Queue(engine::Engine& engine);
    ~Queue() = default;

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    // Engine event sink. Wire as: engine.set_event_callback([&q](auto& e){ q.on_event(e); });
    void on_event(const engine::Event& ev);

    // ── Mutation ──────────────────────────────────────────────────────────────

    void append(std::filesystem::path path);
    // Insert before index (clamped to valid range).
    void insert(int index, std::filesystem::path path);
    void remove(int index);
    // Move track at `from` to position `to`.
    void move(int from, int to);
    void clear();

    // Load and play the track at index. Clamps to [0, size-1].
    void jump(int index);

    // Repopulate the queue from a persisted session without auto-playing.
    // Replaces tracks_ with `paths`, sets current_index_ to `index` clamped
    // to a valid range, loads that track into the engine, seeks to
    // `position_frames`, and stays paused. Per the persistence contract, the
    // daemon never auto-resumes — the user explicitly hits play.
    void restore(std::vector<std::filesystem::path> paths, int index,
                 std::uint64_t position_frames);

    // ── Query ─────────────────────────────────────────────────────────────────

    std::vector<std::filesystem::path> tracks() const;
    int current_index() const;
    int size() const;

private:
    engine::Engine& engine_;
    mutable std::mutex mtx_;
    std::vector<std::filesystem::path> tracks_;
    int current_index_ = -1;

    // Must be called with mtx_ held.
    void preload_next_locked_();
};

} // namespace fidelis::queue

#endif
