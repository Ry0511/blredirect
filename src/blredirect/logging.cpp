#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <fstream>
#include <mutex>

#include "blredirect/logging.h"

namespace blredirect {

namespace {

std::mutex log_mutex{};

// NOLINTNEXTLINE(*-throwing-static-initialization)
std::ofstream log_file{};

}  // namespace

void setup_logging(const fs::path& in_file) {
    const std::scoped_lock lock{log_mutex};
    log_file.open(in_file, std::ios::trunc);
}

void shutdown_logging() {
    const std::scoped_lock lock{log_mutex};
    log_file.close();
}

void log(std::string_view msg) {
    const std::scoped_lock lock{log_mutex};
    if (log_file.is_open()) {
        log_file << msg << '\n'
                 << std::flush;
    }
}

std::string narrow(std::wstring_view str) {
    if (str.empty()) {
        return {};
    }

    const int len = WideCharToMultiByte(
        CP_UTF8,
        0,
        str.data(),
        static_cast<int>(str.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        str.data(),
        static_cast<int>(str.size()),
        out.data(),
        len,
        nullptr,
        nullptr
    );

    return out;
}

}  // namespace blredirect
