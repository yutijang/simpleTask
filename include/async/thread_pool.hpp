#pragma once

#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace Async {

class ThreadPool {
  public:
    explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency()) {
        {
            constexpr std::size_t threadCountDefault{5};
            threadCount = (threadCount == 0) ? threadCountDefault : threadCount;
            threadCount = (threadCount >= threadCountDefault) ? (threadCount - 1) : threadCount;
        }

        m_workers.reserve(threadCount);

        for (std::size_t i{0}; i < threadCount; ++i) {
            m_workers.emplace_back([this]() {
                workerLoop();
            });
        }
    }

    ThreadPool(ThreadPool const&) = delete;
    ThreadPool& operator=(ThreadPool const&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool() noexcept {
        {
            std::scoped_lock const lock{m_mutex};
            m_stopping = true;
        }

        m_condition_variable.notify_all();
        // std::jthread tự động request_stop + join
    }

    template<typename Function, typename... Args>
        requires std::invocable<Function, Args...>
    [[nodiscard]]
    auto submit(Function&& function, Args&&... args)
        -> std::future<std::invoke_result_t<Function, Args...>> {
        using resultType = std::invoke_result_t<Function, Args...>;

        auto task = std::make_shared<std::packaged_task<resultType()>>(
            [function = std::forward<Function>(function),
             ... args = std::forward<Args>(args)]() mutable {
                return std::invoke(std::move(function), std::move(args)...);
            });

        auto future = task->get_future();

        {
            std::scoped_lock const lock{m_mutex};

            if (m_stopping) {
                throw std::runtime_error("ThreadPool is stopping");
            }

            m_tasks.emplace([task]() {
                (*task)();
            });
        }

        m_condition_variable.notify_one();

        return future;
    }

    template<typename Function, typename Callback, typename... Args>
        requires std::invocable<Function, Args...> &&
                 ((std::is_void_v<std::invoke_result_t<Function, Args...>> &&
                   std::invocable<Callback>) ||
                  (!std::is_void_v<std::invoke_result_t<Function, Args...>> &&
                   std::invocable<Callback, std::invoke_result_t<Function, Args...>>) )
    auto submitWithCallback(Function&& function, Callback&& callback, Args&&... args) -> void {
        using resultType = std::invoke_result_t<Function, Args...>;

        auto task = [function = std::forward<Function>(function),

                     callback = std::forward<Callback>(callback),

                     ... args = std::forward<Args>(args)]() mutable {
            if constexpr (std::is_void_v<resultType>) {
                std::invoke(std::move(function), std::move(args)...);

                std::invoke(std::move(callback));
            } else {
                auto result = std::invoke(std::move(function), std::move(args)...);

                std::invoke(std::move(callback), std::move(result));
            }
        };

        {
            std::scoped_lock const lock{m_mutex};

            if (m_stopping) {
                throw std::runtime_error{"ThreadPool is stopping"};
            }

            m_tasks.emplace(std::move(task));
        }

        m_condition_variable.notify_one();
    }

  private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock{m_mutex};

                m_condition_variable.wait(lock, [this]() {
                    return m_stopping || !m_tasks.empty();
                });

                if (m_stopping && m_tasks.empty()) {
                    return;
                }

                task = std::move(m_tasks.front());
                m_tasks.pop();
            }

            task();
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    std::queue<std::function<void()>> m_tasks;
    std::vector<std::jthread> m_workers;
    bool m_stopping{};
};

} // namespace Async
