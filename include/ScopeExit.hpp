#pragma once

#include <utility>

template<typename F> class ScopeExit {
  public:
    explicit ScopeExit(F&& fn) : m_fn(std::forward<F>(fn)) {}

    ScopeExit(ScopeExit&& other) noexcept : m_fn(std::move(other.m_fn)), m_active(other.m_active) {
        other.m_active = false;
    }

    ~ScopeExit() noexcept {
        if (m_active) {
            m_fn();
        }
    }

    ScopeExit(ScopeExit const&) = delete;
    ScopeExit& operator=(ScopeExit const&) = delete;
    ScopeExit& operator=(ScopeExit&&) = delete;

  private:
    F m_fn;
    bool m_active{true};
};

template<typename F> ScopeExit(F) -> ScopeExit<F>;
