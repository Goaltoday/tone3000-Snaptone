// Portable regression test for the final v2.10.1 Tone Match/export stage.
// It deliberately includes the implementation so the test can exercise the
// private analysis helpers without exposing them as production API.
#include "../../plugin/conversion/src/clo_refiner.cpp"

#include <iostream>
#include <random>
#include <stdexcept>

namespace ntc {
std::string pathToUtf8(const fs::path& path) { return path.string(); }
bool readFileBytes(const fs::path& path, std::vector<std::uint8_t>& out, std::string& error) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) { error = "read failed"; return false; }
  const auto size = input.tellg();
  if (size < 0) return false;
  out.resize(static_cast<std::size_t>(size));
  input.seekg(0);
  input.read(reinterpret_cast<char*>(out.data()), size);
  return static_cast<bool>(input);
}
bool writeFileBytes(const fs::path& path, const std::uint8_t* bytes,
                    std::size_t size, std::string& error) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(size));
  if (!output) { error = "write failed"; return false; }
  return true;
}
}  // namespace ntc

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
void put32(std::vector<std::uint8_t>& bytes, std::size_t pos, std::uint32_t value) {
  for (int i = 0; i < 4; ++i) bytes[pos + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> (8 * i));
}
void putFloat(std::vector<std::uint8_t>& bytes, std::size_t pos, float value) {
  std::uint32_t raw{}; std::memcpy(&raw, &value, sizeof(raw)); put32(bytes, pos, raw);
}
void putDouble(std::vector<std::uint8_t>& bytes, std::size_t pos, double value) {
  std::uint64_t raw{}; std::memcpy(&raw, &value, sizeof(raw));
  for (int i = 0; i < 8; ++i) bytes[pos + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(raw >> (8 * i));
}
std::uint16_t crc(const std::vector<std::uint8_t>& bytes, std::size_t end) {
  std::uint16_t value = 0xffff;
  for (std::size_t i = 12; i < end; ++i) {
    value ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) value = (value & 1) ? (value >> 1) ^ 0xa001 : value >> 1;
  }
  return value;
}
void writeWav(const std::filesystem::path& path, const std::vector<float>& samples) {
  std::vector<std::uint8_t> bytes(44 + 4 * samples.size());
  std::memcpy(bytes.data(), "RIFF", 4); put32(bytes, 4, static_cast<std::uint32_t>(bytes.size() - 8));
  std::memcpy(bytes.data() + 8, "WAVEfmt ", 8); put32(bytes, 16, 16); bytes[20] = 3; bytes[22] = 1;
  put32(bytes, 24, 44100); put32(bytes, 28, 176400); bytes[32] = 4; bytes[34] = 32;
  std::memcpy(bytes.data() + 36, "data", 4); put32(bytes, 40, static_cast<std::uint32_t>(samples.size() * 4));
  for (std::size_t i = 0; i < samples.size(); ++i) putFloat(bytes, 44 + 4 * i, samples[i]);
  std::string error;
  require(ntc::writeFileBytes(path, bytes.data(), bytes.size(), error), "write WAV");
}
}  // namespace

int main(int argc, char** argv) try {
  namespace fs = std::filesystem;
  const fs::path dir = argc > 1 ? argv[1] : "conversion-test-output";
  const bool longTail = argc > 2;
  std::error_code ignored;
  fs::remove_all(dir, ignored);
  fs::create_directories(dir);
  std::string error;

  ntc::V26Comp comp; comp.f = {40, 18000}; comp.raw = {6, 6};
  const auto raw = ntc::v26minPhaseIr(comp);
  require(std::abs(raw[0] - std::pow(10., 6. / 20.)) < 1e-4, "direct curve changed");
  std::vector<float> base(512, 0), candidate; base[0] = 1;
  require(ntc::correctedFinalB(base, {2.f}, candidate), "gain correction invalid");
  require(candidate == base, "historical B normalization changed");
  require(!ntc::correctedFinalB(base, {std::numeric_limits<float>::infinity()}, candidate), "non-finite accepted");

  std::vector<std::uint8_t> inputClo(0x2288); std::memcpy(inputClo.data(), "VTSI", 4);
  put32(inputClo, 4, 0x2288); put32(inputClo, 0x14, 0x2200); put32(inputClo, 0x7c, 128);
  put32(inputClo, 0x80, 128); put32(inputClo, 0x84, 2048);
  putDouble(inputClo, 0x18, 1); putDouble(inputClo, 0x40, 1);
  for (const auto pos : {0x68, 0x6c, 0x70, 0x74, 0x88, 0x288}) putFloat(inputClo, pos, 1);
  putFloat(inputClo, 0x288 + 1500 * 4, .7f);
  const auto inputCrc = crc(inputClo, inputClo.size()); inputClo[8] = inputCrc >> 8; inputClo[9] = inputCrc & 255;
  require(ntc::writeFileBytes(dir / "source2048.clo", inputClo.data(), inputClo.size(), error), "source write");

  ntc::Model model; require(ntc::parseModel(inputClo, model, error), "parse model");
  constexpr std::size_t sampleCount = 70u * 44100u;
  std::vector<float> input(sampleCount); std::mt19937 rng(341);
  for (auto& sample : input) sample = static_cast<float>((static_cast<double>(rng()) / rng.max() - .5) * .25);
  const auto aOutput = ntc::precomputeA(model, input, sampleCount, ntc::cloPlayerGainControlToLinear(50));
  std::vector<float> preB, target; ntc::renderPreB(model, aOutput, 1, 1, 1, 1, preB);
  std::vector<float> targetB(1600, 0);
  if (longTail) { targetB[0] = .2f; targetB[900] = std::sqrt(.96f); }
  else { targetB[0] = std::sqrt(.96f); targetB[5] = .2f; }
  ntc::renderWithB(preB, targetB, target, ntc::cloPlayerVolumeControlToLinear(50));
  input.resize(sampleCount + 600, 123.f); target.resize(sampleCount + 600, -123.f);
  writeWav(dir / "stimulus.wav", input); writeWav(dir / "target.wav", target);

  for (const auto destination : {ntc::CloDestination::Gp200, ntc::CloDestination::Gp5}) {
    ntc::CloRefineConfig config; config.destination = destination; ntc::CloRefineStats stats;
    const auto taps = ntc::destinationBTaps(destination); const auto output = dir / ("final" + std::to_string(taps) + ".clo");
    require(ntc::refineCloBOnly(dir / "source2048.clo", dir / "stimulus.wav", dir / "target.wav",
                               output, config, error, {}, &stats), error.c_str());
    require(std::isfinite(stats.finalRmseDb), "final measurement invalid");
    std::vector<std::uint8_t> bytes; require(ntc::readFileBytes(output, bytes, error), "read output");
    const auto declared = 0x88u + 4u * (128u + taps);
    require(bytes.size() == (taps == 1024 ? 0x2288u : declared), "physical size");
    require(ntc::le32(bytes.data() + 4) == declared && ntc::le32(bytes.data() + 0x84) == taps, "header size");
    require(std::equal(bytes.begin() + 0x18, bytes.begin() + 0x78, inputClo.begin() + 0x18), "P/K or biquad changed");
    require(std::equal(bytes.begin() + 0x88, bytes.begin() + 0x288, inputClo.begin() + 0x88), "A changed");
    require(std::all_of(bytes.begin() + declared, bytes.end(), [](auto value) { return value == 0; }), "padding not zero");
    const auto outputCrc = crc(bytes, declared);
    require(bytes[8] == outputCrc >> 8 && bytes[9] == (outputCrc & 255), "CRC byte order");
    std::cout << "B" << taps << " RMSE=" << stats.finalRmseDb << " frames=" << stats.analysisFrames << '\n';
  }

  ntc::CloRefineConfig config; ntc::CloRefineStats stats;
  writeWav(dir / "short.wav", std::vector<float>(100, 0));
  require(!ntc::refineCloBOnly(dir / "source2048.clo", dir / "short.wav", dir / "target.wav",
                              dir / "must_not_exist.clo", config, error, {}, &stats), "short input accepted");
  require(!fs::exists(dir / "must_not_exist.clo"), "failed job wrote output");
  std::cout << "PASS: B1024/B512 direct Tone Match, sizes, CRC, preserved A/P/K/biquads and invalid input.\n";
} catch (const std::exception& exception) {
  std::cerr << "FAIL: " << exception.what() << '\n';
  return 1;
}
