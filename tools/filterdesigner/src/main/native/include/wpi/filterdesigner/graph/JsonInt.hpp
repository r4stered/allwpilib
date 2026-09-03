// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <optional>
#include <string_view>

#include "wpi/util/json.hpp"

namespace wpi::filterdesigner {

/**
 * Reads a JSON value as an int, or nothing if it is not one.
 *
 * Only a plain integer literal that fits in an int qualifies. The parser
 * types anything with a fraction or an exponent as a double, and converting
 * a double outside int's range is undefined, so a design file holding
 * "order": 1e100 or "taps": 2.5 must be refused up front rather than cast
 * and clamped afterwards.
 */
std::optional<int> ReadJsonInt(const wpi::util::json& value);

/**
 * Looks up @a key in @a obj and reads it with ReadJsonInt. Empty when the
 * key is absent or the value is not an in-range integer, so a caller keeps
 * its default in both cases.
 */
std::optional<int> ReadIntField(const wpi::util::json& obj,
                                std::string_view key);

}  // namespace wpi::filterdesigner
