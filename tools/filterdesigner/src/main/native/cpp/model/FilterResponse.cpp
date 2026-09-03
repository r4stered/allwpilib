// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/filterdesigner/model/FilterResponse.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <optional>

#include "wpi/filterdesigner/model/PoleZero.hpp"

namespace wpi::filterdesigner {

namespace {

// Corner frequency of the cascade's slowest pole, in Hz. A z-plane pole p
// corresponds to an s-plane pole at ln(p)·fs, and |ln p| is that pole's
// distance from the origin, i.e. its corner in rad/sample. Poles at the
// origin (pure-FIR sections) have no corner and are skipped.
std::optional<double> LowestPoleCorner(const Sections& sections, double fs) {
  std::optional<double> lowest;
  for (const auto& p : ComputePolesZeros(sections).poles) {
    if (std::abs(p) == 0.0) {
      continue;
    }
    double corner = std::abs(std::log(p)) * fs / (2.0 * std::numbers::pi);
    if (corner > 0.0 && (!lowest || corner < *lowest)) {
      lowest = corner;
    }
  }
  return lowest;
}

}  // namespace

std::optional<FrequencyResponse> FrequencyResponse::Compute(
    const Sections& sections, double fs, int numPoints) {
  if (sections.empty() || fs <= 0.0 || numPoints < 2) {
    return std::nullopt;
  }

  const double nyquist = 0.5 * fs;
  // The grid has to reach below the filter's own corner or the passband and
  // cutoff of a low-frequency design never appear: fs/numPoints alone puts a
  // 1 Hz low-pass at 1 kHz and 512 points off the left edge. Start a decade
  // under the slowest pole, and never above the old fs/numPoints bound.
  double fLow = fs / static_cast<double>(numPoints);
  if (auto corner = LowestPoleCorner(sections, fs)) {
    fLow = std::min(fLow, *corner / 10.0);
  }
  fLow = std::max(fLow, 1e-6);
  const double logLow = std::log10(fLow);
  const double logHigh = std::log10(nyquist);

  FrequencyResponse out;
  out.frequencies.reserve(numPoints);
  out.magnitudesDb.reserve(numPoints);
  out.phasesDegrees.reserve(numPoints);

  double prevPhase = 0.0;
  double offset = 0.0;
  for (int i = 0; i < numPoints; ++i) {
    double alpha = static_cast<double>(i) / (numPoints - 1);
    double f = std::pow(10.0, logLow + alpha * (logHigh - logLow));
    double w = 2.0 * std::numbers::pi * f / fs;
    std::complex<double> z1 = std::polar(1.0, -w);
    std::complex<double> z2 = std::polar(1.0, -2.0 * w);
    std::complex<double> h{1.0, 0.0};
    for (const Section& s : sections) {
      std::complex<double> num = s.b0 + s.b1 * z1 + s.b2 * z2;
      std::complex<double> den = 1.0 + s.a1 * z1 + s.a2 * z2;
      h *= num / den;
    }
    double mag = std::abs(h);
    double magDb = 20.0 * std::log10(std::max(mag, 1e-12));
    double phase = std::arg(h);
    if (i > 0) {
      // `while` (not `if`) handles sparse grids where phase jumps by more than
      // a single 2π between samples (high-order filters, wide log spans).
      double delta = phase - prevPhase;
      while (delta > std::numbers::pi) {
        offset -= 2.0 * std::numbers::pi;
        delta -= 2.0 * std::numbers::pi;
      }
      while (delta < -std::numbers::pi) {
        offset += 2.0 * std::numbers::pi;
        delta += 2.0 * std::numbers::pi;
      }
    }
    prevPhase = phase;
    out.frequencies.push_back(f);
    out.magnitudesDb.push_back(magDb);
    out.phasesDegrees.push_back((phase + offset) * 180.0 / std::numbers::pi);
  }
  return out;
}

}  // namespace wpi::filterdesigner
