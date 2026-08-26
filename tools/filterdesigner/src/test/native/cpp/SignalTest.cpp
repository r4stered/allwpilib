// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/filterdesigner/model/Signal.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "TestAssertions.hpp"

namespace {

using wpi::filterdesigner::Signal;

TEST_CASE("SignalTest InferSampleRateEmpty", "[filterdesigner]") {
  std::vector<double> ts;
  CHECK(Signal::InferSampleRate(ts) == 0.0);
}

TEST_CASE("SignalTest InferSampleRateSingleSample", "[filterdesigner]") {
  std::vector<double> ts{0.1};
  CHECK(Signal::InferSampleRate(ts) == 0.0);
}

TEST_CASE("SignalTest InferSampleRateUniform1kHz", "[filterdesigner]") {
  std::vector<double> ts;
  for (int i = 0; i < 100; ++i) {
    ts.push_back(i * 0.001);
  }
  CHECK_NEAR(Signal::InferSampleRate(ts), 1000.0, 1e-9);
}

TEST_CASE("SignalTest InferSampleRateRobustToOutlierGap", "[filterdesigner]") {
  // 50 Hz sampling with one 10x gap in the middle; median should survive.
  std::vector<double> ts;
  for (int i = 0; i < 20; ++i) {
    ts.push_back(i * 0.02);
  }
  ts.push_back(ts.back() + 0.2);  // big gap
  for (int i = 0; i < 20; ++i) {
    ts.push_back(ts.back() + 0.02);
  }
  CHECK_NEAR(Signal::InferSampleRate(ts), 50.0, 1e-9);
}

TEST_CASE("SignalTest InferSampleRateZeroForNonPositivePeriod",
          "[filterdesigner]") {
  std::vector<double> ts{0.0, 0.0, 0.0, 0.0};
  CHECK(Signal::InferSampleRate(ts) == 0.0);
}

TEST_CASE("SignalTest IsUniformTrueForEvenlySpaced", "[filterdesigner]") {
  std::vector<double> ts;
  for (int i = 0; i < 50; ++i) {
    ts.push_back(i * 0.01);
  }
  CHECK(Signal::IsUniform(ts));
}

TEST_CASE("SignalTest IsUniformFalseWithOneOffGap", "[filterdesigner]") {
  std::vector<double> ts;
  for (int i = 0; i < 10; ++i) {
    ts.push_back(i * 0.01);
  }
  ts.push_back(ts.back() + 0.05);
  CHECK_FALSE(Signal::IsUniform(ts));
}

TEST_CASE("SignalTest IsUniformFalseForEmptyOrSingle", "[filterdesigner]") {
  std::vector<double> empty;
  CHECK_FALSE(Signal::IsUniform(empty));
  std::vector<double> one{1.0};
  CHECK_FALSE(Signal::IsUniform(one));
}

TEST_CASE("SignalTest IsUniformToleranceRespected", "[filterdesigner]") {
  std::vector<double> ts{0.0, 0.01, 0.02000001, 0.03};
  CHECK(Signal::IsUniform(ts, 1e-4));
  CHECK_FALSE(Signal::IsUniform(ts, 1e-9));
}

}  // namespace
