#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ntc {

namespace fs = std::filesystem;

inline constexpr std::uint64_t kExpectedCloSize = 0x2288;
inline constexpr std::uint32_t kExpectedApiReturn = 0x2288;
inline constexpr wchar_t kVersion[] = L"2.10.1";

std::string toUtf8(const std::wstring& value);
std::wstring fromUtf8(const std::string& value);
std::string pathToUtf8(const fs::path& path);
fs::path executablePath();
bool readFileBytes(const fs::path& path, std::vector<std::uint8_t>& data, std::string& error);
bool writeFileBytes(const fs::path& path, const std::uint8_t* data, std::size_t size, std::string& error);

} // namespace ntc
