#include "ConversionManager.h"

#include "common.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>

std::mutex ConversionManager::globalConversionMutex;

namespace {
juce::var objectVar(std::initializer_list<std::pair<const char*, juce::var>> values) {
  auto object = new juce::DynamicObject();
  for (const auto& [key, value] : values)
    object->setProperty(key, value);
  return juce::var(object);
}

juce::String pathString(const ntc::fs::path& path) {
  return juce::String(ntc::pathToUtf8(path));
}
}  // namespace

class ConversionManager::Job final : public juce::ThreadPoolJob {
public:
  Job(ConversionManager& owner, std::shared_ptr<State> state, Request request)
      : juce::ThreadPoolJob("NAM to CLO conversion"), owner(owner), state(std::move(state)),
        request(std::move(request)) {}

  JobStatus runJob() override {
    std::lock_guard<std::mutex> conversionLock(ConversionManager::globalConversionMutex);
    setPhase("preparing");

    std::error_code ec;
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("TONE3000-NamToClo")
                          .getChildFile(state->jobId);
    const auto rootPath = ConversionManager::toPath(root.getFullPathName());
    const auto inputPath = rootPath / (ConversionManager::sanitiseStem(request.modelName) + ".nam");
    const auto outputPath = ConversionManager::toPath(request.outputDirectory);
    std::filesystem::create_directories(rootPath, ec);
    if (ec) return fail("Cannot create conversion work directory: " + ec.message());

    std::string error;
    if (!ntc::writeFileBytes(inputPath, request.namBytes.data(), request.namBytes.size(), error))
      return fail(error);

    if (outputPath.empty())
      return fail("Choose an output folder before starting the conversion.");
    if (!std::filesystem::exists(outputPath, ec)) {
      std::filesystem::create_directories(outputPath, ec);
      if (ec) return fail("Cannot create output folder: " + ec.message());
    }

    ntc::StimulusConfig stimulus;
    stimulus.tailMode = request.tailMode;
    stimulus.recordedAudio = ConversionManager::toPath(request.recordedAudio);

    ntc::CorrectiveIrConfig correction;
    correction.enabled = request.correctiveIrEnabled;
    correction.wav = ConversionManager::toPath(request.correctiveIr);

    ntc::CloRefineConfig refine;
    refine.destination = request.destination;
    refine.referenceWav = ConversionManager::toPath(request.referenceWav);

    ntc::NativeConverterConfig converter;
    converter.blockSize = 1024;
    converter.originalStimulus = ConversionManager::toPath(request.originalStimulus);

    const auto result = ntc::convertNamToClo(
        inputPath, outputPath, stimulus, correction, refine, converter,
        [this](const std::wstring& message) {
          setPhase(juce::String(ntc::toUtf8(message)));
        });

    if (!result.ok) {
      std::filesystem::remove_all(rootPath, ec);
      return fail(result.error.empty() ? "NAM to CLO conversion failed." : result.error);
    }

    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->outputPath = pathString(result.outputClo);
      state->finalRmseDb = result.toneMatch.finalRmseDb;
      state->phase = "complete";
      state->running = false;
      state->done = true;
      state->ok = true;
    }
    std::filesystem::remove_all(rootPath, ec);
    return jobHasFinished;
  }

private:
  void setPhase(const juce::String& phase) {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->running) state->phase = phase;
  }

  JobStatus fail(const std::string& message) {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->phase = "failed";
    state->error = juce::String(message);
    state->running = false;
    state->done = true;
    state->ok = false;
    return jobHasFinished;
  }

  ConversionManager& owner;
  std::shared_ptr<State> state;
  Request request;
};

ConversionManager::ConversionManager() : pool(1) {}

ConversionManager::~ConversionManager() {
  pool.removeAllJobs(true);
}

std::string ConversionManager::sanitiseStem(const juce::String& name) {
  auto stem = name.isNotEmpty() ? name : juce::String("TONE3000-model");
  stem = stem.upToLastOccurrenceOf(".", false, false);
  std::string result = stem.toStdString();
  for (char& c : result) {
    const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                      || (c >= '0' && c <= '9') || c == '-' || c == '_';
    if (!allowed) c = '_';
  }
  if (result.empty()) result = "TONE3000-model";
  return result;
}

ntc::fs::path ConversionManager::toPath(const juce::String& value) {
  if (value.isEmpty()) return {};
  return ntc::fs::u8path(std::string(value.toRawUTF8()));
}

juce::String ConversionManager::fromPath(const ntc::fs::path& value) {
  return value.empty() ? juce::String() : juce::String(ntc::pathToUtf8(value));
}

juce::var ConversionManager::statusToVar(const State& state) {
  std::lock_guard<std::mutex> lock(state.mutex);
  return objectVar({
      {"jobId", state.jobId},
      {"phase", state.phase},
      {"modelName", state.modelName},
      {"outputPath", state.outputPath},
      {"error", state.error},
      {"finalRmseDb", state.finalRmseDb},
      {"running", state.running},
      {"done", state.done},
      {"ok", state.ok},
  });
}

juce::var ConversionManager::start(Request request) {
  if (request.namBytes.empty())
    return objectVar({{"error", "The selected NAM is not loaded in the plugin."}});

  std::shared_ptr<State> state;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (current != nullptr) {
      std::lock_guard<std::mutex> stateLock(current->mutex);
      if (current->running)
        return objectVar({{"error", "A conversion is already running."}, {"jobId", current->jobId}});
    }

    static std::uint64_t nextId = 1;
    state = std::make_shared<State>();
    state->jobId = "ntc-" + juce::String(juce::Time::currentTimeMillis()) + "-" + juce::String(nextId++);
    state->modelName = request.modelName;
    state->phase = "queued";
    state->running = true;
    current = state;
  }

  auto job = std::make_unique<Job>(*this, state, std::move(request));
  if (!pool.addJob(job.get(), true)) {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->phase = "failed";
    state->error = "Could not start the conversion worker.";
    state->running = false;
    state->done = true;
    state->ok = false;
    return objectVar({{"error", "Could not start the conversion worker."}, {"jobId", state->jobId}});
  }
  job.release();
  return objectVar({{"jobId", state->jobId}});
}

juce::var ConversionManager::getStatus(const juce::String& jobId) const {
  std::shared_ptr<State> state;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    state = current;
  }
  if (state == nullptr || state->jobId != jobId)
    return objectVar({{"jobId", jobId}, {"phase", "unknown"}, {"error", "Unknown conversion job."},
                      {"running", false}, {"done", true}, {"ok", false}});
  return statusToVar(*state);
}
