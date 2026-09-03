// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/filterdesigner/nodes/FrequencyPlotNodeLogic.hpp"

namespace wpi::filterdesigner {

const Spectrum* FrequencyPlotNodeLogic::SpectrumFor(int slot,
                                                    const Signal* sig) {
  if (slot < 0 || slot >= kInputCount) {
    return nullptr;
  }
  CachedSpectrum& c = m_cache[slot];
  if (!sig) {
    c = {};
    return nullptr;
  }
  if (c.input == sig && c.revision == sig->revision &&
      c.sampleRate == sig->sampleRate) {
    return c.spectrum ? &*c.spectrum : nullptr;
  }
  c.input = sig;
  c.revision = sig->revision;
  c.sampleRate = sig->sampleRate;
  // Compute's own guards (rate, length) decide whether there is a spectrum;
  // a signal that fails them is cached as "none" so it isn't retried each
  // frame either.
  c.spectrum = Spectrum::Compute(sig->values, sig->sampleRate);
  ++m_computeCount;
  return c.spectrum ? &*c.spectrum : nullptr;
}

}  // namespace wpi::filterdesigner
