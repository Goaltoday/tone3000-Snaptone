#include "common.hpp"

#include <fstream>
#include <system_error>

namespace ntc {

std::string toUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const auto encoded = fs::path(value).u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

std::wstring fromUtf8(const std::string& value) {
    if (value.empty()) return {};
    return std::wstring(value.begin(), value.end());
}

std::string pathToUtf8(const fs::path& path) {
    const auto encoded = path.u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

fs::path executablePath() {
    // The plugin passes an explicit embedded-stimulus path through
    // NativeConverterConfig. This fallback is retained for standalone callers.
    std::error_code ec;
    const auto current = fs::current_path(ec);
    return ec ? fs::path{} : current / "TONE3000";
}

bool readFileBytes(const fs::path& path, std::vector<std::uint8_t>& data, std::string& error) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) { error = "Cannot open file: " + pathToUtf8(path); return false; }
    const auto end = in.tellg();
    if (end < 0) { error = "Cannot determine file size: " + pathToUtf8(path); return false; }
    data.resize(static_cast<std::size_t>(end));
    in.seekg(0, std::ios::beg);
    if (!data.empty()) {
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (static_cast<std::size_t>(in.gcount()) != data.size()) {
            error = "Short read: " + pathToUtf8(path); return false;
        }
    }
    return true;
}

bool writeFileBytes(const fs::path& path, const std::uint8_t* data, std::size_t size, std::string& error) {
    std::error_code ec;
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) { error = "Cannot create output directory: " + ec.message(); return false; }
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) { error = "Cannot create file: " + pathToUtf8(path); return false; }
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    if (!out) { error = "Failed writing file: " + pathToUtf8(path); return false; }
    return true;
}

} // namespace ntc
