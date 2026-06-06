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
#include <cstdlib>
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

[[maybe_unused]] TaskReturn<std::size_t>
    simpleTask(Async::ThreadPool& pool, fs::path const& dir, std::promise<void>& done) {
    auto guard = std::shared_ptr<void>(nullptr, [&done](void*) {
        done.set_value();
    });

    /** ĐIỂM CHIA TAY **
     * Đây là điểm cuối cùng coroutine còn chạy trên main thread.
     *
     * Sau co_await pool:
     * - coroutine được đưa vào thread pool
     * - main thread quay về main() và chờ tại future.get()
     * - một worker thread sẽ tiếp quản phần việc còn lại
     */
    co_await pool;

    /** BẮT ĐẦU THU THẬP VÀ CHUẨN BỊ THUỐC NỔ **
     * Coroutine giờ đã chạy trên một worker thread.
     *
     * Worker thread này sẽ:
     * - thu thập danh sách file cần xử lý
     * - tạo danh sách các task xử lý độc lập
     * - chuẩn bị cho đợt thực thi song song sắp tới
     */
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

    /** ĐIỂM KÍCH NỔ **
     * Toàn bộ thuốc nổ được châm ngòi tại đây.
     *
     * Các task được đẩy vào thread pool và bắt đầu chạy đồng thời.
     * Worker thread nào rảnh sẽ giành lấy task tiếp theo để xử lý.
     *
     * Coroutine đứng ngoài quan sát và chờ mọi vụ nổ kết thúc
     * trước khi quay lại thu thập kết quả.
     */
    co_await Async::when_all(pool, std::move(tasks));

    /** Thu thập kết quả và trả về */
    co_return totalWords.load(std::memory_order_relaxed);
}

} // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    if (argc < 2) {
        std::println("USAGE: ./<binary_name> <directory_path>");

        return EXIT_FAILURE;
    }

    fs::path const dir{argv[1]};

    ScopedTimer timer{};

    Async::ThreadPool pool;
    std::promise<void> done;
    auto future = done.get_future();

    auto task = simpleTask(pool, dir, done);
    task.resume();

    future.get();

    if (task.done()) {
        try {
            auto const total = task.get_result();
            std::println("Total words: {}", total);
        } catch (std::exception const& e) {
            std::println("{}", e.what());
        }
    }

    return EXIT_SUCCESS;
}
