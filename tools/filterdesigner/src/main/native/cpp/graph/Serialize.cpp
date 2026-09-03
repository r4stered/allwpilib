// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/filterdesigner/graph/Serialize.hpp"

#include <cmath>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <ImNodeFlow.h>

#include "wpi/filterdesigner/graph/FilterDesignerNode.hpp"
#include "wpi/filterdesigner/graph/Graph.hpp"
#include "wpi/filterdesigner/graph/JsonInt.hpp"
#include "wpi/filterdesigner/graph/NodeRegistry.hpp"
#include "wpi/util/json.hpp"

namespace wpi::filterdesigner {

namespace {

using wpi::util::json;

constexpr const char* kRejectV1Message =
    "This .fdsgn file uses an older format (v1) that this tool no longer "
    "supports. Rebuild the design as a node graph and re-save.";

// Ids and the version live in an int, and a loaded id is bumped by one to
// mint the next fresh one, so on top of ReadJsonInt's range check a graph
// int must be non-negative and leave room for that increment: only
// [0, INT_MAX) qualifies.
std::optional<int> ReadGraphInt(const json& value) {
  const std::optional<int> v = ReadJsonInt(value);
  if (!v || *v < 0 || *v >= std::numeric_limits<int>::max()) {
    return std::nullopt;
  }
  return v;
}

// ImNodeFlow keeps node positions in floats and does canvas arithmetic on
// them every frame. Narrowing a double outside the float range is undefined
// to begin with, and the infinity it produces in practice puts the node
// somewhere the canvas can never scroll to.
std::optional<float> ReadCoord(const json& value) {
  if (!value.is_number()) {
    return std::nullopt;
  }
  const double v = value.get_number();
  if (!std::isfinite(v) ||
      std::abs(v) > static_cast<double>(std::numeric_limits<float>::max())) {
    return std::nullopt;
  }
  return static_cast<float>(v);
}

// ImFlow::BaseNode's own inPin/outPin assert and UB-deref on a miss, which
// the load path cannot afford: its pin names come from arbitrary JSON.
ImFlow::Pin* FindOutPinByName(ImFlow::BaseNode* node, std::string_view name) {
  for (const auto& pin : node->getOuts()) {
    if (pin && pin->getName() == name) {
      return pin.get();
    }
  }
  return nullptr;
}

ImFlow::Pin* FindInPinByName(ImFlow::BaseNode* node, std::string_view name) {
  for (const auto& pin : node->getIns()) {
    if (pin && pin->getName() == name) {
      return pin.get();
    }
  }
  return nullptr;
}

}  // namespace

std::string SerializeGraph(const Graph& graph) {
  auto nodesArr = json::array();
  for (FilterDesignerNode* node : graph.Nodes()) {
    json entry = json::object("id", node->GraphId(), "type",
                              std::string{node->TypeTag()}, "pos",
                              json::array(node->getPos().x, node->getPos().y));
    node->SerializeParams(entry);
    nodesArr.emplace_back(std::move(entry));
  }

  auto linksArr = json::array();
  for (const auto& link : graph.Links()) {
    linksArr.emplace_back(json::object(
        "src", json::object("node", link.srcId, "pin", link.srcPin), "dst",
        json::object("node", link.dstId, "pin", link.dstPin)));
  }

  json root = json::object("version", kFdsgnVersion, "nodes",
                           std::move(nodesArr), "links", std::move(linksArr));
  return root.to_string_pretty();
}

DeserializeResult DeserializeGraph(std::string_view jsonText, Graph& graph,
                                   const NodeRegistry& registry) {
  DeserializeResult result;

  auto parsed = json::parse(jsonText);
  if (!parsed) {
    result.error = std::string{"Malformed JSON: "} + parsed.error();
    return result;
  }

  const json& root = *parsed;
  if (!root.is_object()) {
    result.error = "File root must be a JSON object";
    return result;
  }

  // v1 is the pre-node-graph format, and gets its own message; a version
  // newer than this build knows is rejected outright.
  const json* versionNode = root.lookup("version");
  if (!versionNode || !versionNode->is_number()) {
    result.error = kRejectV1Message;
    return result;
  }
  const std::optional<int> parsedVersion = ReadGraphInt(*versionNode);
  if (!parsedVersion) {
    result.error =
        "Unsupported .fdsgn version (not a whole number this build can read)";
    return result;
  }
  const int version = *parsedVersion;
  if (version == 1) {
    result.error = kRejectV1Message;
    return result;
  }
  if (version != kFdsgnVersion) {
    result.error = "Unsupported .fdsgn version " + std::to_string(version) +
                   " (this build understands version " +
                   std::to_string(kFdsgnVersion) + ")";
    return result;
  }

  const json* nodesNode = root.lookup("nodes");
  const json* linksNode = root.lookup("links");
  if (!nodesNode || !nodesNode->is_array()) {
    result.error = "Missing or non-array \"nodes\"";
    return result;
  }
  if (!linksNode || !linksNode->is_array()) {
    result.error = "Missing or non-array \"links\"";
    return result;
  }

  graph.Reset();

  // Ids must be unique or link restoration is a coin toss between the
  // duplicates, silently, and the next save keeps the collision.
  std::unordered_set<int> seenIds;
  for (const json& entry : nodesNode->get_array()) {
    if (!entry.is_object()) {
      result.warnings.emplace_back("Skipping non-object node entry");
      continue;
    }
    const json* idNode = entry.lookup("id");
    const json* typeNode = entry.lookup("type");
    const json* posNode = entry.lookup("pos");
    if (!idNode || !typeNode || !typeNode->is_string() || !posNode ||
        !posNode->is_array() || posNode->get_array().size() != 2) {
      result.warnings.emplace_back(
          "Skipping node with missing id/type/pos fields");
      continue;
    }
    const std::optional<int> parsedId = ReadGraphInt(*idNode);
    if (!parsedId) {
      result.warnings.emplace_back(
          "Skipping node whose id is not a whole number in range");
      continue;
    }
    const int id = *parsedId;
    const std::string& type = typeNode->get_string();
    const auto& pos = posNode->get_array();
    const std::optional<float> px = ReadCoord(pos[0]);
    const std::optional<float> py = ReadCoord(pos[1]);
    if (!px || !py) {
      result.warnings.emplace_back(
          "Skipping node whose pos coordinates are not numbers the canvas "
          "can hold");
      continue;
    }
    ImVec2 p{*px, *py};

    if (!seenIds.insert(id).second) {
      result.warnings.emplace_back("Duplicate node id " + std::to_string(id) +
                                   " — skipped");
      continue;
    }

    const NodeRegistry::Entry* regEntry = registry.FindByTag(type);
    if (!regEntry) {
      result.warnings.emplace_back("Unknown node type '" + type +
                                   "' — skipped");
      continue;
    }
    auto node = regEntry->factory(graph, p);
    if (!node) {
      result.warnings.emplace_back("Factory for '" + type + "' returned null");
      continue;
    }
    // The factory minted a fresh id; overwrite it with the one from disk and
    // re-bump the counter past it.
    node->SetGraphId(id);
    graph.BumpNextIdAbove(id);
    node->DeserializeParams(entry);
  }

  for (const json& link : linksNode->get_array()) {
    if (!link.is_object()) {
      result.warnings.emplace_back("Skipping non-object link entry");
      continue;
    }
    const json* srcNode = link.lookup("src");
    const json* dstNode = link.lookup("dst");
    if (!srcNode || !srcNode->is_object() || !dstNode ||
        !dstNode->is_object()) {
      result.warnings.emplace_back("Skipping link missing src/dst");
      continue;
    }
    const json* srcId = srcNode->lookup("node");
    const json* srcPin = srcNode->lookup("pin");
    const json* dstId = dstNode->lookup("node");
    const json* dstPin = dstNode->lookup("pin");
    if (!srcId || !srcPin || !srcPin->is_string() || !dstId || !dstPin ||
        !dstPin->is_string()) {
      result.warnings.emplace_back("Skipping link with malformed endpoints");
      continue;
    }
    const std::optional<int> srcGraphId = ReadGraphInt(*srcId);
    const std::optional<int> dstGraphId = ReadGraphInt(*dstId);
    if (!srcGraphId || !dstGraphId) {
      result.warnings.emplace_back(
          "Skipping link whose endpoint id is not a whole number in range");
      continue;
    }
    FilterDesignerNode* src = graph.FindNodeById(*srcGraphId);
    FilterDesignerNode* dst = graph.FindNodeById(*dstGraphId);
    if (!src || !dst) {
      // Endpoint was skipped earlier, and already warned about.
      continue;
    }
    ImFlow::Pin* outPin = FindOutPinByName(src, srcPin->get_string());
    ImFlow::Pin* inPin = FindInPinByName(dst, dstPin->get_string());
    if (!outPin || !inPin) {
      result.warnings.emplace_back("Link references missing pin; skipped");
      continue;
    }
    // ImNodeFlow's createLink is a toggle for the same output and a silent
    // replacement for a different one: a file listing a link twice would load
    // with no wire, and one listing two sources for the same input would keep
    // whichever came last. Either way the next save makes it permanent, so
    // the first link into an input wins and the rest are reported.
    if (auto existing = inPin->getLink().lock()) {
      const char* what =
          existing->left() == outPin ? "Duplicate" : "Conflicting";
      result.warnings.emplace_back(std::string{what} + " link into pin '" +
                                   dstPin->get_string() + "' of node " +
                                   std::to_string(*dstGraphId) + " — skipped");
      continue;
    }
    inPin->createLink(outPin);
  }

  return result;
}

std::string SaveGraphToFile(std::string_view path, const Graph& graph) {
  std::string text = SerializeGraph(graph);
  std::ofstream out(std::string{path}, std::ios::binary | std::ios::trunc);
  if (!out) {
    return std::string{"Could not open for writing: "} + std::string{path};
  }
  out << text;
  if (!out) {
    return std::string{"Write failed: "} + std::string{path};
  }
  return {};
}

DeserializeResult LoadGraphFromFile(std::string_view path, Graph& graph,
                                    const NodeRegistry& registry) {
  DeserializeResult result;
  std::ifstream in(std::string{path}, std::ios::binary);
  if (!in) {
    result.error = std::string{"Could not open: "} + std::string{path};
    return result;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return DeserializeGraph(ss.str(), graph, registry);
}

}  // namespace wpi::filterdesigner
