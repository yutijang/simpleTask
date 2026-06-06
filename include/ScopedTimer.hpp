#pragma once

#include <chrono>
#include <print>

class ScopedTimer {
  public:
    ScopedTimer() : m_start(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        auto const end = std::chrono::steady_clock::now();
        auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
        std::println("Execution time: {}", elapsed);
    }

  private:
    std::chrono::steady_clock::time_point m_start;
};
