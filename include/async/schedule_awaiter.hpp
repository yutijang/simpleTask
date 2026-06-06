#pragma once

#include <coroutine>

// NOLINTBEGIN (readability-identifier-naming)
namespace Async {

class ThreadPool;

class ScheduleAwaiter {
  public:
    explicit ScheduleAwaiter(ThreadPool& pool) noexcept : m_pool(pool) {}

    ~ScheduleAwaiter() noexcept = default;

    ScheduleAwaiter(ScheduleAwaiter const&) = delete;
    ScheduleAwaiter& operator=(ScheduleAwaiter const&) = delete;

    ScheduleAwaiter(ScheduleAwaiter&&) noexcept = delete;
    ScheduleAwaiter& operator=(ScheduleAwaiter&&) noexcept = delete;

    [[nodiscard]] bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> const awaiting_handle) const noexcept {
        executeOnPool(awaiting_handle);
    }

    void await_resume() const noexcept {}

  private:
    void executeOnPool(std::coroutine_handle<> handle) const;

    ThreadPool& m_pool;
};

inline auto operator co_await(ThreadPool& pool) noexcept {
    return ScheduleAwaiter{pool};
}

} // namespace Async

// NOLINTEND (readability-identifier-naming)
