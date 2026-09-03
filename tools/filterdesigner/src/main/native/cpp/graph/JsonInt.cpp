// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/filterdesigner/graph/JsonInt.hpp"

#include <limits>

namespace wpi::filterdesigner {

std::optional<int> ReadJsonInt(const wpi::util::json& value) {
  constexpr auto kMax = std::numeric_limits<int>::max();
  constexpr auto kMin = std::numeric_limits<int>::min();
  if (value.is_uint()) {
    const auto v = value.get_uint();
    if (v > static_cast<decltype(v)>(kMax)) {
      return std::nullopt;
    }
    return static_cast<int>(v);
  }
  if (value.is_int()) {
    const auto v = value.get_int();
    if (v < kMin || v > kMax) {
      return std::nullopt;
    }
    return static_cast<int>(v);
  }
  return std::nullopt;
}

std::optional<int> ReadIntField(const wpi::util::json& obj,
                                std::string_view key) {
  const wpi::util::json* value = obj.lookup(key);
  if (!value) {
    return std::nullopt;
  }
  return ReadJsonInt(*value);
}

}  // namespace wpi::filterdesigner
