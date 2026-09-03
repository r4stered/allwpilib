// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/filterdesigner/model/ApplyFilter.hpp"

#include <cmath>
#include <vector>

#include "wpi/math/filter/BiquadFilter.hpp"

namespace wpi::filterdesigner {

std::vector<double> ApplyFilter(std::span<const double> samples,
                                const Sections& sections, FilterStart start) {
  if (sections.empty()) {
    return {samples.begin(), samples.end()};
  }
  wpi::math::BiquadFilter filter(sections);
  if (start == FilterStart::SteadyState && !samples.empty()) {
    filter.Reset(samples.front());
    // Reset(value) divides by each section's 1 + a1 + a2, so a cascade with a
    // pole at z = 1 — or a first sample that is not finite — leaves no usable
    // steady state to start from. Falling back to zero state only costs a
    // startup transient, while keeping such a seed would smear a NaN over
    // every remaining sample.
    if (!std::isfinite(filter.LastValue())) {
      filter.Reset();
    }
  }
  std::vector<double> out;
  out.reserve(samples.size());
  for (double x : samples) {
    out.push_back(filter.Calculate(x));
  }
  return out;
}

}  // namespace wpi::filterdesigner
