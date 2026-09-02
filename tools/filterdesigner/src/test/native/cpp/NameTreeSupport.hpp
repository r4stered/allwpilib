// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <string_view>

#include "wpi/filterdesigner/nodes/NameTree.hpp"

/**
 * Child of @p node named @p name, or nullptr. Both the tree unit's tests and
 * WpiLogSourceNodeLogic's walk trees by path, and naming each hop keeps the
 * assertions readable next to the paths they are checking.
 */
inline const wpi::filterdesigner::NameTreeNode* FindChild(
    const wpi::filterdesigner::NameTreeNode& node, std::string_view name) {
  for (const auto& child : node.children) {
    if (child.name == name) {
      return &child;
    }
  }
  return nullptr;
}
