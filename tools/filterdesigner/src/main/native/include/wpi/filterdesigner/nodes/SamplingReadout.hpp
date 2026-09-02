// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <string>

#include <imgui.h>

#include "wpi/filterdesigner/model/Signal.hpp"

namespace wpi::filterdesigner {

/**
 * Renders a source node's one-line sampling readout, e.g.
 * "248 Hz, 0.2% filled, longest gap 24.0 ms".
 *
 * The loaders resample rather than reject, so a signal can look plottable
 * while being mostly interpolant across a disabled period. This line is where
 * the user finds out, escalating grey to amber to red as the reconstruction
 * takes over — see @ref ClassifySampling.
 */
inline void DrawSamplingReadout(const Signal& signal) {
  const std::string text = DescribeSampling(signal);
  switch (ClassifySampling(signal)) {
    case SamplingSeverity::Warn:
      ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f}, "%s", text.c_str());
      break;
    case SamplingSeverity::Bad:
      ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.4f, 1.0f}, "%s", text.c_str());
      break;
    case SamplingSeverity::Ok:
      ImGui::TextDisabled("%s", text.c_str());
      break;
  }
}

}  // namespace wpi::filterdesigner
