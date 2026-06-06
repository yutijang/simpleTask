// clang-format off
#include "async/thread_pool.hpp"
#include "async/coroutine_task.hpp"
#include "async/schedule_awaiter.hpp"
#include "async/when_all.hpp"
#include "ScopedTimer.hpp"
#include "MemoryMappedFile.hpp"
// clang-format on

#include <atomic>
#include <cctype>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <print>
#include <string_view>
#include <utility>
#include <vector>

namespace Async {

void ScheduleAwaiter::executeOnPool(std::coroutine_handle<> const handle) const {
    [[maybe_unused]] auto future = m_pool.submit([handle]() {
        if (handle && !handle.done()) {
            handle.resume();
        }
    });
}

} // namespace Async

namespace {

namespace fs = std::filesystem;

std::size_t countWords(fs::path const& path) {
    std::size_t count{};
    bool inWord{false};

    MemoryMappedFile const file{path};
    std::string_view const content{reinterpret_cast<char const*>(file.data()), file.size()};

    for (char const& ch : content) {
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            ++count;
        }
    }

    return count;
}

[[maybe_unused]] TaskReturn<std::size_t> simpleTask(Async::ThreadPool& pool,
                                                    std::promise<void>& done) {
    auto guard = std::shared_ptr<void>(nullptr, [&done](void*) {
        done.set_value();
    });

    co_await pool;

    fs::path const dir{"../content-generated"};
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        co_return 0;
    }

    std::vector<fs::path> files;

    for (fs::directory_entry const& entry : fs::directory_iterator(dir)) {
        if (!fs::is_regular_file(entry)) {
            continue;
        }

        fs::path const& p = entry.path();
        if (p.extension() == ".txt") {
            files.push_back(p);
        }
    }

    std::atomic<std::size_t> totalWords{0};

    std::vector<std::function<void()>> tasks;
    tasks.reserve(files.size());

    for (auto const& f : files) {
        tasks.emplace_back([f, &totalWords]() {
            auto const localCount = countWords(f);
            totalWords.fetch_add(localCount, std::memory_order_relaxed);
        });
    }

    co_await Async::when_all(pool, std::move(tasks));

    co_return totalWords.load(std::memory_order_relaxed);
}

} // namespace

int main() {
    ScopedTimer timer{};

    Async::ThreadPool pool;
    std::promise<void> done;
    auto fut = done.get_future();

    auto task = simpleTask(pool, done);
    task.resume();

    fut.get();

    if (task.done()) {
        try {
            auto const total = task.get_result();
            std::println("Total words: {}", total);
        } catch (std::exception const& e) {
            std::println("{}", e.what());
        }
    }

    return 0;
}
