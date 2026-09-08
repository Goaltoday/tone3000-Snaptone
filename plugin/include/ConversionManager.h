#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "native_converter.hpp"

/**
    Owns the CPU/file-heavy NAM -> CLO conversion jobs used by the editor.

    The processor snapshots model bytes before creating a Request.  The worker
    therefore never touches ChainBlock, the audio thread, or the processor
    after the request has been submitted.  A process-wide mutex serializes the
    converter because NAM rendering and the 2048-tap fit are deliberately
    memory hungry and several plugin instances may be open in one DAW.
*/
class ConversionManager {
public:
  struct Request {
    std::vector<std::uint8_t> namBytes;
    juce::String modelName;
    juce::String originalStimulus;
    juce::String recordedAudio;
    juce::String correctiveIr;
    juce::String referenceWav;
    juce::String outputDirectory;
    ntc::CloDestination destination = ntc::CloDestination::Gp200;
    ntc::TailMode tailMode = ntc::TailMode::PresetAudio;
    bool correctiveIrEnabled = false;
  };

  ConversionManager();
  ~ConversionManager();

  /** Returns { jobId } or { error }. */
  juce::var start(Request request);
  /** Returns a stable JSON-compatible status object. */
  juce::var getStatus(const juce::String& jobId) const;

private:
  struct State {
    mutable std::mutex mutex;
    juce::String jobId;
    juce::String phase = "idle";
    juce::String modelName;
    juce::String outputPath;
    juce::String error;
    double finalRmseDb = 0.0;
    bool running = false;
    bool done = false;
    bool ok = false;
  };

  class Job;

  static std::mutex globalConversionMutex;
  juce::ThreadPool pool;
  mutable std::mutex stateMutex;
  std::shared_ptr<State> current;

  static juce::var statusToVar(const State& state);
  static std::string sanitiseStem(const juce::String& name);
  static ntc::fs::path toPath(const juce::String& value);
  static juce::String fromPath(const ntc::fs::path& value);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConversionManager)
};
