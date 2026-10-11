#ifndef BLREDIRECT_LOGGING_H
#define BLREDIRECT_LOGGING_H

#include <filesystem>
#include <format>
#include <string>
#include <string_view>

namespace blredirect {

namespace fs = std::filesystem;

void setup_logging(const fs::path& in_file);
void shutdown_logging();
void log(std::string_view msg);

std::string narrow(std::wstring_view str);

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BLREDIRECT_LOG(...) (::blredirect::log(std::format(__VA_ARGS__)))

}  // namespace blredirect

#endif  // BLREDIRECT_LOGGING_H
