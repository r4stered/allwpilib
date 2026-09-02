// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wpi::filterdesigner {

/**
 * How well a signal's samples fit the uniform grid implied by its inferred
 * sample rate.
 *
 * Real FRC data is never exactly uniform. Timestamps jitter by single-digit
 * percentages of the period, samples drop out during heavy CPU load, and
 * whole minutes go missing while the robot is disabled — a strict uniformity
 * check rejects essentially every entry in a real log. The loaders therefore
 * resample onto the inferred grid (see @ref Signal::ResampleToGrid) so the
 * fixed-`dt` assumption downstream is actually true, and record here how
 * much of the result is reconstruction rather than measurement.
 *
 * Metrics are measured against the grid whether or not the signal was
 * actually resampled onto it, so an @ref onGrid == false signal still reports
 * what the reconstruction would have cost. @ref filled is approximate: it
 * counts slots against original samples rather than tracking which slots an
 * interpolant actually reached, so clustered samples read as better covered
 * than they are.
 */
struct GridQuality {
  /** True once the signal's timestamps sit exactly on the uniform grid. */
  bool onGrid = false;
  /**
   * Median error of an original sampling interval against the grid period, as
   * a fraction of that period. Ranges 0 to 0.5; near 0.5 means the inferred
   * rate does not describe the data. Intervals long enough to be dropouts are
   * left out — @ref longestGap and @ref filled report those.
   *
   * Intervals rather than each timestamp's distance from its slot: those
   * distances are the running sum of the interval errors, so they random-walk
   * away from the grid and saturate near 0.5 within a few dozen samples.
   */
  double jitter = 0.0;
  /**
   * Fraction of grid slots with no original sample behind them, so their
   * value comes from the interpolant rather than from a measurement. Ranges
   * 0 to 1; high values mean the signal is mostly reconstruction.
   */
  double filled = 0.0;
  /**
   * Longest interval between consecutive original samples, in seconds. Large
   * relative to the period means a logging pause, across which a spectral
   * estimate is meaningless no matter how the hole is filled.
   */
  double longestGap = 0.0;
  /**
   * Samples at the ends of the record that sit too far from the rest to be
   * part of it, and that @ref Signal::ResampleToGrid left outside the grid.
   * A topic published once as NetworkTables connects and then not again until
   * the robot is enabled leaves exactly one, minutes ahead of the data it
   * belongs to.
   *
   * Every other metric here describes the window that remains, so the fill
   * fraction and the longest gap are those of the data actually kept. On the
   * refusal path there is no grid and nothing is dropped; the count still
   * says which window was measured.
   */
  std::size_t trimmed = 0;

  /**
   * Metrics for a signal generated exactly on a uniform grid — what the
   * step/impulse generators produce. Returns an off-grid quality when
   * @p sampleRate is non-positive.
   */
  static GridQuality Exact(double sampleRate);
};

/**
 * A time series loaded from a WPILOG file or NT4 source, resampled onto a
 * uniform grid by its loader.
 *
 * timestamps and values are parallel arrays in chronological order;
 * timestamps is in seconds.
 */
struct Signal {
  std::string name;
  std::vector<double> timestamps;
  std::vector<double> values;
  /** Grid rate the signal was resampled onto, from @ref InferSampleRate.
   * Set by @ref ResampleToGrid; 0 when no grid could be inferred. */
  double sampleRate = 0.0;
  /** How well the samples fit the grid @ref sampleRate describes. */
  GridQuality quality;
  /**
   * True when the signal only ever takes discrete values, so there is nothing
   * meaningful between two of them. @ref ResampleToGrid holds such a signal
   * across a slot rather than interpolating a value — 0.5 for a boolean —
   * that the source can never have taken.
   *
   * Set by the loaders from the source type. Booleans are discrete; integers
   * are not, being more often a count than a state enum.
   */
  bool discrete = false;
  /** Monotonically increasing whenever values/timestamps change in place.
   * Lets downstream caches detect content churn on a pointer-stable signal
   * (e.g. NT4's sliding-window buffer, where size stays constant once the
   * window saturates). Loaders are responsible for bumping it. */
  std::uint64_t revision = 0;
  /** True for streaming sources whose buffer is a sliding window of recent
   * samples (NT4); false for whole-record sources (WPILOG, generators). Lets
   * sinks like TimePlotNode follow the latest data instead of pinning the
   * x-axis to the initial range. Filter nodes propagate this from input. */
  bool live = false;

  /** Hard ceiling on the grid ResampleToGrid will build, in slots. At two
   * doubles per slot that caps the resampled arrays at 64 MiB. */
  static constexpr std::size_t kMaxGridSlots = std::size_t{1} << 22;
  /** Ceiling on grid slots per original sample. Bounds memory for short
   * signals separated by huge gaps, where @ref kMaxGridSlots alone would
   * still let a handful of samples inflate into millions. */
  static constexpr std::size_t kMaxGridExpansion = 64;
  /** Gap, in periods, past which a sample at either end of the record is
   * treated as belonging to a different stretch of logging rather than to the
   * body, and left off the grid. Matches the threshold segmentation uses:
   * beyond a handful of periods a gap is a pause, not a dropout. */
  static constexpr double kTrimPeriods = 10.0;

  /**
   * Infers sample rate (Hz) from the median of consecutive timestamp
   * differences. Median rather than mean makes it robust to one-off gaps
   * (dropped packets, logging pauses). Returns 0 if fewer than two samples
   * or if the inferred period is non-positive.
   *
   * @param timestamps Monotonic timestamps in seconds.
   */
  static double InferSampleRate(std::span<const double> timestamps);

  /**
   * Resamples this signal onto the uniform grid implied by
   * @ref InferSampleRate, so that the fixed-`dt` assumption the FFT, the
   * biquad stages and the generated robot code all make is actually true.
   *
   * The grid spans the record's body: leading and trailing samples separated
   * from their neighbour by more than @ref kTrimPeriods periods are left off
   * it, since a gap that size is a topic that went quiet rather than a packet
   * that dropped, and anchoring on one would spend most of the grid
   * interpolating a value nothing measured. @ref GridQuality::trimmed counts
   * them. Each slot's value is linearly interpolated between the two original
   * samples bracketing it, or, when @ref discrete is set, held from the last
   * sample at or before it. Interpolating is what makes use of the sub-slot
   * part of a timestamp: snapping to the nearest slot discards it and
   * substitutes a quantization error of up to half a period. Sets
   * @ref sampleRate and @ref quality; leaves @ref revision and @ref discrete
   * alone so the loader controls both.
   *
   * Leaves the data untouched when no grid can be inferred (fewer than two
   * samples, or a non-positive median period) and when the grid would exceed
   * @ref kMaxGridSlots or @ref kMaxGridExpansion — a signal whose gaps dwarf
   * its period, which no amount of interpolation makes analyzable.
   * @ref GridQuality::onGrid distinguishes those cases; the rest of
   * @ref quality is populated either way.
   *
   * Metrics describe the samples handed in, so resample from the raw source
   * samples — as both loaders do — rather than from an already-resampled
   * signal. A second call leaves the data alone but re-measures it against
   * itself, which reports a flawless grid no matter how much of the first
   * pass was invention.
   */
  void ResampleToGrid();
};

// Sampling is reported from the model rather than from the nodes because the
// node draw() bodies are compiled out of the test build; keeping the wording
// and the thresholds here is what makes them testable. See
// nodes/SamplingReadout.hpp for the ImGui side.

/** How alarming a signal's grid quality is, for UI emphasis. */
enum class SamplingSeverity { Ok, Warn, Bad };

/**
 * Grades a signal's grid quality so the source nodes can color their
 * sampling readout. Bad means the numbers downstream should not be trusted:
 * no inferred rate, a grid we refused to build, or a signal that is mostly
 * held-over values.
 */
SamplingSeverity ClassifySampling(const Signal& signal);

/**
 * One-line human-readable summary of a signal's sampling, e.g.
 * "248 Hz, 0.2% filled, longest gap 24.0 ms". Reads "sample rate unknown"
 * when no rate could be inferred.
 */
std::string DescribeSampling(const Signal& signal);

}  // namespace wpi::filterdesigner
