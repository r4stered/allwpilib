// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/filterdesigner/model/ApplyFilter.hpp"

#include <cmath>
#include <numbers>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "TestAssertions.hpp"
#include "wpi/filterdesigner/model/Stage.hpp"
#include "wpi/math/filter/BiquadFilter.hpp"
#include "wpi/units/frequency.hpp"

namespace {

using wpi::filterdesigner::ApplyFilter;
using wpi::filterdesigner::Sections;
using wpi::math::BiquadFilter;
using namespace wpi::units;

Sections SectionsOf(const BiquadFilter& f) {
  auto span = f.Sections();
  return Sections(span.begin(), span.end());
}

TEST_CASE("ApplyFilterTest EmptySectionsPassesThrough", "[filterdesigner]") {
  std::vector<double> in{1.0, 2.0, 3.0, -4.0};
  auto out = ApplyFilter(in, Sections{});
  CHECK(out == in);
}

TEST_CASE("ApplyFilterTest EmptyInputReturnsEmpty", "[filterdesigner]") {
  auto filter = SectionsOf(BiquadFilter::Butterworth(
      BiquadFilter::Kind::LowPass, 4, 1000_Hz, 100_Hz));
  std::vector<double> in;
  auto out = ApplyFilter(in, filter);
  CHECK(out.empty());
}

TEST_CASE("ApplyFilterTest LengthMatchesInput", "[filterdesigner]") {
  auto filter = SectionsOf(BiquadFilter::Butterworth(
      BiquadFilter::Kind::LowPass, 4, 1000_Hz, 100_Hz));
  std::vector<double> in(1000, 1.0);
  auto out = ApplyFilter(in, filter);
  CHECK(out.size() == in.size());
}

TEST_CASE("ApplyFilterTest DcStepSettlesToUnity", "[filterdesigner]") {
  auto filter = SectionsOf(BiquadFilter::Butterworth(
      BiquadFilter::Kind::LowPass, 4, 1000_Hz, 100_Hz));
  std::vector<double> step(500, 1.0);
  auto out = ApplyFilter(step, filter);
  // After enough samples a Butterworth LP settles to the DC gain of 1.
  CHECK_NEAR(out.back(), 1.0, 1e-6);
}

TEST_CASE("ApplyFilterTest MovingAverageImpulseProducesBoxcar",
          "[filterdesigner]") {
  auto filter = SectionsOf(BiquadFilter::MovingAverage(5));
  std::vector<double> in(10, 0.0);
  in[0] = 1.0;
  auto out = ApplyFilter(in, filter);
  for (int i = 0; i < 5; ++i) {
    UNSCOPED_INFO("tap " << i);
    CHECK_NEAR(out[i], 0.2, 1e-12);
  }
  for (size_t i = 5; i < out.size(); ++i) {
    UNSCOPED_INFO("after-window " << i);
    CHECK_NEAR(out[i], 0.0, 1e-12);
  }
}

TEST_CASE("ApplyFilterTest LowPassAttenuatesHighFrequency",
          "[filterdesigner]") {
  constexpr double kFs = 1000.0;
  constexpr int kN = 1024;
  auto filter = SectionsOf(BiquadFilter::Butterworth(
      BiquadFilter::Kind::LowPass, 4, hertz_t{kFs}, 50_Hz));
  std::vector<double> in(kN);
  for (int n = 0; n < kN; ++n) {
    in[n] = std::sin(2.0 * std::numbers::pi * 400.0 * n / kFs);
  }
  auto out = ApplyFilter(in, filter);
  double inEnergy = 0.0;
  double outEnergy = 0.0;
  // Skip the first 256 samples to let the transient die out.
  for (size_t i = 256; i < in.size(); ++i) {
    inEnergy += in[i] * in[i];
    outEnergy += out[i] * out[i];
  }
  CHECK(outEnergy / inEnergy < 1e-3);
}

}  // namespace
