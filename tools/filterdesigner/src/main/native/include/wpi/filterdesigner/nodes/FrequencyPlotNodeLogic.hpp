// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "wpi/filterdesigner/model/Signal.hpp"
#include "wpi/filterdesigner/model/Spectrum.hpp"

namespace wpi::filterdesigner {

/**
 * Persisted display options for a FrequencyPlotNode, plus one cached
 * spectrum per input pin so a static WPILOG signal is transformed once, not
 * once per frame.
 */
struct FrequencyPlotNodeLogic {
  static constexpr int kInputCount = 4;

  /** If true (default) the y-axis rescales to the data each frame. */
  bool autoscale = true;

  /** If true (default) the legend overlay is shown. */
  bool showLegend = true;

  /** If true, x-axis is log-scaled; default is linear. */
  bool logFrequency = false;

  /** Canvas size in pixels. The node auto-sizes to it. */
  float plotWidth = 600.0f;
  float plotHeight = 320.0f;

  static constexpr float kMinPlotWidth = 240.0f;
  static constexpr float kMinPlotHeight = 140.0f;

  /**
   * Spectrum of @p sig for input pin @p slot, or nullptr when @p sig is null
   * or too short or rateless to transform. A transient (see
   * @ref Signal::transient) is transformed without a window, so a filtered
   * impulse plots the filter's response. Recomputed only when the pointer,
   * revision, sample rate or transient flag differ from the previous call
   * for that slot; a
   * live source bumps its revision every frame it drains samples, a log
   * source only when its window changes. The pointer is valid until the next
   * call for the same slot.
   */
  const Spectrum* SpectrumFor(int slot, const Signal* sig);

  /** Number of FFTs run so far; lets a test see a cache hit. */
  std::uint64_t SpectrumComputeCount() const { return m_computeCount; }

 private:
  struct CachedSpectrum {
    const Signal* input = nullptr;
    std::uint64_t revision = 0;
    double sampleRate = 0.0;
    bool transient = false;
    std::optional<Spectrum> spectrum;
  };
  std::array<CachedSpectrum, kInputCount> m_cache;
  std::uint64_t m_computeCount = 0;
};

}  // namespace wpi::filterdesigner
