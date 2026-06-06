#pragma once

#include "async/coroutine_task.hpp"
#include "async/thread_pool.hpp"

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

// NOLINTBEGIN (readability-identifier-naming)
namespace Async {

// ─────────────────────────────────────────────────────────────
// Overload 1: vector<std::function<void()>>  — plain void tasks
// ─────────────────────────────────────────────────────────────
class WhenAllFnAwaiter {
  public:
    WhenAllFnAwaiter(ThreadPool& pool, std::vector<std::function<void()>> tasks)
        : m_pool(pool), m_tasks(std::move(tasks)) {}

    [[nodiscard]] bool await_ready() const noexcept {
        return m_tasks.empty();
    }

    void await_suspend(std::coroutine_handle<> handle) {
        auto const count = m_tasks.size();
        auto counter = std::make_shared<std::atomic<std::size_t>>(count);

        for (auto& task : m_tasks) {
            [[maybe_unused]] auto fu =
                m_pool.submit([t = std::move(task), counter, handle]() mutable {
                    t();
                    if (counter->fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        handle.resume();
                    }
                });
        }
    }

    void await_resume() const noexcept {}

  private:
    ThreadPool& m_pool;
    std::vector<std::function<void()>> m_tasks;
};

[[nodiscard]] inline WhenAllFnAwaiter when_all(ThreadPool& pool,
                                               std::vector<std::function<void()>> tasks) {
    return WhenAllFnAwaiter{pool, std::move(tasks)};
}

// ─────────────────────────────────────────────────────────────
// Overload 2: vector<TaskVoid>
//   Mỗi TaskVoid được resume trên thread pool.
//   Khi tất cả done → resume caller coroutine.
// ─────────────────────────────────────────────────────────────
class WhenAllVoidAwaiter {
  public:
    WhenAllVoidAwaiter(ThreadPool& pool, std::vector<TaskVoid> tasks)
        : m_pool(pool), m_tasks(std::move(tasks)) {}

    [[nodiscard]] bool await_ready() const noexcept {
        return m_tasks.empty();
    }

    void await_suspend(std::coroutine_handle<> handle) {
        auto const count = m_tasks.size();
        auto counter = std::make_shared<std::atomic<std::size_t>>(count);
        auto sharedTasks = std::make_shared<std::vector<TaskVoid>>(std::move(m_tasks));

        for (std::size_t i = 0; i < count; ++i) {
            [[maybe_unused]] auto fu = m_pool.submit([sharedTasks, i, counter, handle]() {
                (*sharedTasks)[i].resume();
                if (counter->fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    handle.resume();
                }
            });
        }
    }

    void await_resume() const noexcept {}

  private:
    ThreadPool& m_pool;
    std::vector<TaskVoid> m_tasks;
};

[[nodiscard]] inline WhenAllVoidAwaiter when_all(ThreadPool& pool, std::vector<TaskVoid> tasks) {
    return WhenAllVoidAwaiter{pool, std::move(tasks)};
}

// ─────────────────────────────────────────────────────────────
// Overload 3: vector<TaskReturn<T>>
//   Mỗi task được resume trên thread pool.
//   Kết quả được collect theo đúng thứ tự index.
//   co_await → std::vector<T>
// ─────────────────────────────────────────────────────────────
template<typename T> class WhenAllReturnAwaiter {
  public:
    WhenAllReturnAwaiter(ThreadPool& pool, std::vector<TaskReturn<T>> tasks)
        : m_pool(pool), m_tasks(std::move(tasks)) {}

    [[nodiscard]] bool await_ready() const noexcept {
        return m_tasks.empty();
    }

    void await_suspend(std::coroutine_handle<> handle) {
        auto const count = m_tasks.size();
        auto counter = std::make_shared<std::atomic<std::size_t>>(count);
        auto sharedTasks = std::make_shared<std::vector<TaskReturn<T>>>(std::move(m_tasks));
        auto results = std::make_shared<std::vector<T>>(count);

        m_results = results;

        for (std::size_t i = 0; i < count; ++i) {
            [[maybe_unused]] auto fu = m_pool.submit([sharedTasks, results, i, counter, handle]() {
                (*sharedTasks)[i].resume();
                (*results)[i] = (*sharedTasks)[i].get_result();
                if (counter->fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    handle.resume();
                }
            });
        }
    }

    [[nodiscard]] std::vector<T> await_resume() {
        if (m_results) {
            return std::move(*m_results);
        }
        return {};
    }

  private:
    ThreadPool& m_pool;
    std::vector<TaskReturn<T>> m_tasks;
    std::shared_ptr<std::vector<T>> m_results{};
};

template<typename T>
[[nodiscard]] WhenAllReturnAwaiter<T> when_all(ThreadPool& pool, std::vector<TaskReturn<T>> tasks) {
    return WhenAllReturnAwaiter<T>{pool, std::move(tasks)};
}

} // namespace Async

// NOLINTEND (readability-identifier-naming)
