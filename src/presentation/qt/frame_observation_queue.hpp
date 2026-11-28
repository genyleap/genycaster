#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>

// Per-window bounded handoff from the render thread to the GUI thread.
// The mutex protects only fixed-size storage; never invoke Qt while holding it.
class FrameObservationQueue final {
public:
  using Time = std::chrono::steady_clock::time_point;
  static constexpr std::size_t capacity = 256;

  struct Batch final {
    std::array<Time, capacity> timestamps{};
    std::size_t count{};
    std::size_t dropped{};
    std::uint64_t epoch{};
    Time firstQueued{};
  };

  // True means the caller owns posting the single pending GUI notification.
  [[nodiscard]] bool push(Time timestamp, std::uint64_t epoch) {
    const std::lock_guard lock{m_mutex};
    if (m_count == 0 || m_epoch != epoch) {
      m_head = 0;
      m_count = 0;
      m_dropped = 0;
      m_epoch = epoch;
      m_firstQueued = timestamp;
    }
    if (m_count == capacity) {
      m_head = (m_head + 1) % capacity;
      --m_count;
      ++m_dropped;
    }
    m_timestamps[(m_head + m_count) % capacity] = timestamp;
    ++m_count;
    if (m_pending)
      return false;
    m_pending = true;
    return true;
  }

  [[nodiscard]] Batch take() {
    const std::lock_guard lock{m_mutex};
    Batch batch;
    batch.count = m_count;
    batch.dropped = m_dropped;
    batch.epoch = m_epoch;
    batch.firstQueued = m_firstQueued;
    for (std::size_t i = 0; i < m_count; ++i)
      batch.timestamps[i] = m_timestamps[(m_head + i) % capacity];
    m_head = 0;
    m_count = 0;
    m_dropped = 0;
    m_pending = false;
    return batch;
  }

private:
  std::mutex m_mutex;
  std::array<Time, capacity> m_timestamps{};
  std::size_t m_head{};
  std::size_t m_count{};
  std::size_t m_dropped{};
  std::uint64_t m_epoch{};
  Time m_firstQueued{};
  bool m_pending{};
};
