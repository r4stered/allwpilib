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
 * Both source nodes resample onto a uniform grid rather than rejecting
 * nonuniform data, which means a signal can look perfectly plottable while
 * being mostly interpolant across a disabled period. This is the only place
 * the user finds out, so the line escalates from muted grey to amber to red
 * as the reconstruction takes over — see @ref ClassifySampling.
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
