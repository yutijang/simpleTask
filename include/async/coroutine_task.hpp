#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

// NOLINTBEGIN (readability-identifier-naming)
// ─────────────────────────────────────────────
// TaskVoid
// ─────────────────────────────────────────────
class [[nodiscard]] TaskVoid {
  public:
    struct promise_type;
    using HandleType = std::coroutine_handle<promise_type>;

    explicit TaskVoid(HandleType handle) noexcept : m_handle(handle) {}

    ~TaskVoid() noexcept {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    TaskVoid(TaskVoid const&) = delete;
    TaskVoid& operator=(TaskVoid const&) = delete;

    TaskVoid(TaskVoid&& other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}

    TaskVoid& operator=(TaskVoid&& other) noexcept {
        if (this != &other) {
            if (m_handle) {
                m_handle.destroy();
            }
            m_handle = std::exchange(other.m_handle, nullptr);
        }
        return *this;
    }

    void resume() const {
        if (m_handle && !m_handle.done()) {
            m_handle.resume();
        }
    }

    void destroy() noexcept {
        if (m_handle) {
            m_handle.destroy();
            m_handle = nullptr;
        }
    }

    [[nodiscard]] bool done() const noexcept {
        return !m_handle || m_handle.done();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_handle != nullptr;
    }

    [[nodiscard]] HandleType get_handle() const noexcept {
        return m_handle;
    }

    // ── FinalAwaiter: symmetric transfer sang continuation ──
    struct FinalAwaiter {
        [[nodiscard]] bool await_ready() const noexcept {
            return false;
        }

        std::coroutine_handle<> await_suspend(HandleType handle) noexcept {
            if (auto cont = handle.promise().m_continuation) {
                return cont;
            }
            return std::noop_coroutine();
        }

        void await_resume() const noexcept {}
    };

    struct promise_type {
        std::coroutine_handle<> m_continuation{};

        TaskVoid get_return_object() {
            return TaskVoid{HandleType::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        FinalAwaiter final_suspend() noexcept {
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            std::terminate();
        }
    };

  private:
    HandleType m_handle{nullptr};
};

// ─────────────────────────────────────────────
// TaskReturn<T>
// ─────────────────────────────────────────────
template<typename T> class [[nodiscard]] TaskReturn {
  public:
    struct promise_type;
    using HandleType = std::coroutine_handle<promise_type>;

    explicit TaskReturn(HandleType handle) noexcept : m_handle(handle) {}

    ~TaskReturn() noexcept {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    TaskReturn(TaskReturn const&) = delete;
    TaskReturn& operator=(TaskReturn const&) = delete;

    TaskReturn(TaskReturn&& other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}

    TaskReturn& operator=(TaskReturn&& other) noexcept {
        if (this != &other) {
            if (m_handle) {
                m_handle.destroy();
            }
            m_handle = std::exchange(other.m_handle, nullptr);
        }
        return *this;
    }

    void resume() const {
        if (m_handle && !m_handle.done()) {
            m_handle.resume();
        }
    }

    void destroy() noexcept {
        if (m_handle) {
            m_handle.destroy();
            m_handle = nullptr;
        }
    }

    [[nodiscard]] bool done() const noexcept {
        return !m_handle || m_handle.done();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_handle != nullptr;
    }

    [[nodiscard]] HandleType get_handle() const noexcept {
        return m_handle;
    }

    [[nodiscard]] T const& get_result() const& {
        return checkResultReady();
    }

    [[nodiscard]] T& get_result() & {
        return checkResultReady();
    }

    [[nodiscard]] T&& get_result() && {
        return std::move(checkResultReady());
    }

    // ── FinalAwaiter ──
    struct FinalAwaiter {
        [[nodiscard]] bool await_ready() const noexcept {
            return false;
        }

        std::coroutine_handle<> await_suspend(HandleType handle) noexcept {
            if (auto cont = handle.promise().m_continuation) {
                return cont;
            }
            return std::noop_coroutine();
        }

        void await_resume() const noexcept {}
    };

    struct promise_type {
        std::optional<T> m_value{};
        std::exception_ptr m_exception{};
        std::coroutine_handle<> m_continuation{};

        TaskReturn<T> get_return_object() noexcept {
            return TaskReturn<T>{HandleType::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        FinalAwaiter final_suspend() noexcept {
            return {};
        }

        template<typename Value> void return_value(Value&& value) {
            m_value.emplace(std::forward<Value>(value));
        }

        void unhandled_exception() noexcept {
            m_exception = std::current_exception();
        }
    };

  private:
    T& checkResultReady() const {
        if (!m_handle) {
            throw std::runtime_error{"Invalid coroutine handle"};
        }
        auto& promise = m_handle.promise();
        if (promise.m_exception) {
            std::rethrow_exception(promise.m_exception);
        }
        if (!promise.m_value.has_value()) {
            throw std::runtime_error{"Result not available"};
        }
        return *promise.m_value;
    }

    HandleType m_handle{};
};

// NOLINTEND (readability-identifier-naming)
