// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "frc/AnalogTrigger.h"

#include <utility>

#include <hal/FRCUsageReporting.h>

#include "frc/AnalogInput.h"
#include "frc/Base.h"
#include "frc/DutyCycle.h"
#include "frc/Errors.h"
#include "frc/smartdashboard/SendableRegistry.h"

using namespace frc;

AnalogTrigger::AnalogTrigger(int channel)
    : AnalogTrigger(new AnalogInput(channel)) {
  m_ownsAnalog = true;
  SendableRegistry::GetInstance().AddChild(this, m_analogInput);
}

AnalogTrigger::AnalogTrigger(AnalogInput* input) {
  m_analogInput = input;
  int32_t status = 0;
  m_trigger = HAL_InitializeAnalogTrigger(input->m_port, &status);
  FRC_CheckErrorStatus(status, "Channel {}", input->GetChannel());
  int index = GetIndex();

  HAL_Report(HALUsageReporting::kResourceType_AnalogTrigger, index + 1);
  SendableRegistry::GetInstance().AddLW(this, "AnalogTrigger", index);
}

AnalogTrigger::AnalogTrigger(DutyCycle* input) {
  m_dutyCycle = input;
  int32_t status = 0;
  m_trigger = HAL_InitializeAnalogTriggerDutyCycle(input->m_handle, &status);
  FRC_CheckErrorStatus(status, "Channel {}", m_dutyCycle->GetSourceChannel());
  int index = GetIndex();

  HAL_Report(HALUsageReporting::kResourceType_AnalogTrigger, index + 1);
  SendableRegistry::GetInstance().AddLW(this, "AnalogTrigger", index);
}

AnalogTrigger::~AnalogTrigger() {
  int32_t status = 0;
  HAL_CleanAnalogTrigger(m_trigger, &status);
  FRC_ReportError(status, "Channel {}", GetSourceChannel());

  if (m_ownsAnalog) {
    delete m_analogInput;
  }
}

void AnalogTrigger::SetLimitsVoltage(double lower, double upper) {
  int32_t status = 0;
  HAL_SetAnalogTriggerLimitsVoltage(m_trigger, lower, upper, &status);
  FRC_CheckErrorStatus(status, "Channel {}", GetSourceChannel());
}

void AnalogTrigger::SetLimitsDutyCycle(double lower, double upper) {
  int32_t status = 0;
  HAL_SetAnalogTriggerLimitsDutyCycle(m_trigger, lower, upper, &status);
  FRC_CheckErrorStatus(status, "Channel {}", GetSourceChannel());
}

void AnalogTrigger::SetLimitsRaw(int lower, int upper) {
  int32_t status = 0;
  HAL_SetAnalogTriggerLimitsRaw(m_trigger, lower, upper, &status);
  FRC_CheckErrorStatus(status, "Channel {}", GetSourceChannel());
}

void AnalogTrigger::SetAveraged(bool useAveragedValue) {
  int32_t status = 0;
  HAL_SetAnalogTriggerAveraged(m_trigger, useAveragedValue, &status);
  FRC_CheckErrorStatus(status, "Channel {}", GetSourceChannel());
}

void AnalogTrigger::SetFiltered(bool useFilteredValue) {
  int32_t status = 0;
  HAL_SetAnalogTriggerFiltered(m_trigger, useFilteredValue, &status);
  FRC_CheckErrorStatus(status, "Channel {}", GetSourceChannel());
}

int AnalogTrigger::GetIndex() const {
  int32_t status = 0;
  auto ret = HAL_GetAnalogTriggerFPGAIndex(m_trigger, &status);
  FRC_CheckErrorStatus(status, "Channel {}", GetSourceChannel());
  return ret;
}

bool AnalogTrigger::GetInWindow() {
  int32_t status = 0;
  bool result = HAL_GetAnalogTriggerInWindow(m_trigger, &status);
  FRC_CheckErrorStatus(status, "Channel {}", GetSourceChannel());
  return result;
}

bool AnalogTrigger::GetTriggerState() {
  int32_t status = 0;
  bool result = HAL_GetAnalogTriggerTriggerState(m_trigger, &status);
  FRC_CheckErrorStatus(status, "Channel {}", GetSourceChannel());
  return result;
}

std::shared_ptr<AnalogTriggerOutput> AnalogTrigger::CreateOutput(
    AnalogTriggerType type) const {
  return std::shared_ptr<AnalogTriggerOutput>(
      new AnalogTriggerOutput(*this, type), NullDeleter<AnalogTriggerOutput>());
}

void AnalogTrigger::InitSendable(SendableBuilder& builder) {
  if (m_ownsAnalog) {
    m_analogInput->InitSendable(builder);
  }
}

int AnalogTrigger::GetSourceChannel() const {
  if (m_analogInput) {
    return m_analogInput->GetChannel();
  } else if (m_dutyCycle) {
    return m_dutyCycle->GetSourceChannel();
  } else {
    return -1;
  }
}
