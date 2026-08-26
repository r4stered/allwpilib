// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/filterdesigner/graph/Serialize.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <ImNodeFlow.h>
#include <catch2/catch_test_macros.hpp>

#include "wpi/filterdesigner/graph/FilterDesignerNode.hpp"
#include "wpi/filterdesigner/graph/Graph.hpp"
#include "wpi/filterdesigner/graph/NodeRegistry.hpp"
#include "wpi/util/json.hpp"

namespace {

using wpi::filterdesigner::DeserializeGraph;
using wpi::filterdesigner::FilterDesignerNode;
using wpi::filterdesigner::Graph;
using wpi::filterdesigner::NodeRegistry;
using wpi::filterdesigner::SerializeGraph;

// Two test-only node types: one source (one int-typed output), one sink
// (one int-typed input). Mirrors the shape of a real source/sink pair
// without dragging in ImGui dependencies (wpilog loading, ImPlot
// rendering).

class FakeSourceNode : public FilterDesignerNode {
 public:
  FakeSourceNode() {
    setTitle("Fake Source");
    addOUT<int>("out")->behaviour([this] { return m_value; });
  }
  std::string_view TypeTag() const override { return "FakeSource"; }

  void SerializeParams(wpi::util::json& obj) const override {
    obj["value"] = m_value;
  }
  void DeserializeParams(const wpi::util::json& obj) override {
    if (const auto* v = obj.lookup("value"); v && v->is_number()) {
      m_value = static_cast<int>(v->get_number());
    }
  }

  int m_value = 7;
};

class FakeSinkNode : public FilterDesignerNode {
 public:
  FakeSinkNode() {
    setTitle("Fake Sink");
    addIN<int>("in", 0, ImFlow::ConnectionFilter::SameType());
  }
  std::string_view TypeTag() const override { return "FakeSink"; }
};

void RegisterFakes(NodeRegistry& reg) {
  NodeRegistry::Entry src;
  src.tag = "FakeSource";
  src.menuLabel = "Fake Source";
  src.menuCategory = "Sources";
  src.outputTypes.emplace_back(typeid(int));
  src.factory = [](Graph& g, const ImVec2& pos) {
    return std::static_pointer_cast<FilterDesignerNode>(
        g.AddNode<FakeSourceNode>(pos));
  };
  reg.Register(std::move(src));

  NodeRegistry::Entry sink;
  sink.tag = "FakeSink";
  sink.menuLabel = "Fake Sink";
  sink.menuCategory = "Sinks";
  sink.inputTypes.emplace_back(typeid(int));
  sink.factory = [](Graph& g, const ImVec2& pos) {
    return std::static_pointer_cast<FilterDesignerNode>(
        g.AddNode<FakeSinkNode>(pos));
  };
  reg.Register(std::move(sink));
}

TEST_CASE("SerializeTest EmptyGraphRoundTrips", "[filterdesigner]") {
  Graph graph;
  NodeRegistry reg;
  RegisterFakes(reg);

  std::string json = SerializeGraph(graph);

  Graph restored;
  auto result = DeserializeGraph(json, restored, reg);
  UNSCOPED_INFO(result.error);
  CHECK(result.ok());
  CHECK(restored.Nodes().empty());
  CHECK(restored.Links().empty());
}

TEST_CASE("SerializeTest SourceSinkPairRoundTrips", "[filterdesigner]") {
  NodeRegistry reg;
  RegisterFakes(reg);

  Graph graph;
  auto src = graph.AddNode<FakeSourceNode>(ImVec2{10.0f, 20.0f});
  src->m_value = 42;
  auto sink = graph.AddNode<FakeSinkNode>(ImVec2{300.0f, 20.0f});
  sink->inPin("in")->createLink(src->outPin("out"));

  int srcId = src->GraphId();
  int sinkId = sink->GraphId();

  std::string json = SerializeGraph(graph);

  Graph restored;
  auto result = DeserializeGraph(json, restored, reg);
  UNSCOPED_INFO(result.error);
  REQUIRE(result.ok());
  REQUIRE(restored.Nodes().size() == 2u);

  auto* restoredSrc =
      dynamic_cast<FakeSourceNode*>(restored.FindNodeById(srcId));
  auto* restoredSink =
      dynamic_cast<FakeSinkNode*>(restored.FindNodeById(sinkId));
  REQUIRE(restoredSrc != nullptr);
  REQUIRE(restoredSink != nullptr);
  UNSCOPED_INFO("per-node params round-trip");
  CHECK(restoredSrc->m_value == 42);

  auto links = restored.Links();
  REQUIRE(links.size() == 1u);
  CHECK(links[0].srcId == srcId);
  CHECK(links[0].dstId == sinkId);
  CHECK(links[0].srcPin == "out");
  CHECK(links[0].dstPin == "in");
}

TEST_CASE("SerializeTest V1FileIsRejectedWithUserFacingMessage",
          "[filterdesigner]") {
  NodeRegistry reg;
  RegisterFakes(reg);

  Graph graph;
  graph.AddNode<FakeSourceNode>(
      ImVec2{0.0f, 0.0f});  // Should not survive load.

  // v1 was the pre-node-graph format; it must be rejected explicitly.
  std::string v1 = R"({"version": 1, "nodes": [], "links": []})";
  auto result = DeserializeGraph(v1, graph, reg);
  CHECK_FALSE(result.ok());
  UNSCOPED_INFO(
      "error should call out the v1/older format, was: " << result.error);
  CHECK(result.error.find("older format") != std::string::npos);
  // Failed loads must not partially clobber the live graph (the deserializer
  // calls Reset() only after the version check passes).
  CHECK_FALSE(graph.Nodes().empty());
}

TEST_CASE("SerializeTest MissingVersionTreatedAsV1", "[filterdesigner]") {
  NodeRegistry reg;
  RegisterFakes(reg);

  Graph graph;
  std::string noVersion = R"({"nodes": [], "links": []})";
  auto result = DeserializeGraph(noVersion, graph, reg);
  CHECK_FALSE(result.ok());
}

TEST_CASE("SerializeTest UnknownNodeTypeSkippedWithWarning",
          "[filterdesigner]") {
  NodeRegistry reg;
  RegisterFakes(reg);

  // FakeSource (id 1) + a node of an unknown type (id 2). Link from 1→2
  // is dropped because endpoint 2 doesn't exist after the skip.
  std::string json = R"({
    "version": 2,
    "nodes": [
      {"id": 1, "type": "FakeSource", "pos": [0, 0], "value": 99},
      {"id": 2, "type": "FromTheFuture", "pos": [100, 0]}
    ],
    "links": [
      {"src": {"node": 1, "pin": "out"}, "dst": {"node": 2, "pin": "in"}}
    ]
  })";

  Graph graph;
  auto result = DeserializeGraph(json, graph, reg);
  UNSCOPED_INFO(result.error);
  REQUIRE(result.ok());
  UNSCOPED_INFO("unknown type should warn");
  CHECK_FALSE(result.warnings.empty());

  auto nodes = graph.Nodes();
  REQUIRE(nodes.size() == 1u);
  CHECK(nodes[0]->TypeTag() == "FakeSource");
  CHECK(dynamic_cast<FakeSourceNode*>(nodes[0])->m_value == 99);
  CHECK(graph.Links().empty());
}

TEST_CASE("SerializeTest UnknownPinNameSkippedWithWarning",
          "[filterdesigner]") {
  // Pre-fix, BaseNode::inPin/outPin asserted on miss + dereffed end() in
  // release builds — so a stale or hand-edited file referencing a pin name
  // that doesn't exist on the node would UB-deref instead of warning. Now
  // the load path matches by name via getIns()/getOuts() and skips
  // gracefully.
  NodeRegistry reg;
  RegisterFakes(reg);

  std::string json = R"({
    "version": 2,
    "nodes": [
      {"id": 1, "type": "FakeSource", "pos": [0, 0], "value": 1},
      {"id": 2, "type": "FakeSink", "pos": [100, 0]}
    ],
    "links": [
      {"src": {"node": 1, "pin": "ghost"}, "dst": {"node": 2, "pin": "in"}}
    ]
  })";

  Graph graph;
  auto result = DeserializeGraph(json, graph, reg);
  UNSCOPED_INFO(result.error);
  REQUIRE(result.ok());
  UNSCOPED_INFO("missing pin must surface as a warning");
  CHECK_FALSE(result.warnings.empty());
  // Both nodes still loaded, but the link did not.
  CHECK(graph.Nodes().size() == 2u);
  CHECK(graph.Links().empty());
}

TEST_CASE("SerializeTest NonNumericPosSkippedWithWarning", "[filterdesigner]") {
  // Pre-fix, pos[0].get_number() would throw/abort in release builds on a
  // malformed pos field. Now the loader validates types and skips the
  // offending node with a warning.
  NodeRegistry reg;
  RegisterFakes(reg);

  std::string json = R"({
    "version": 2,
    "nodes": [
      {"id": 1, "type": "FakeSource", "pos": ["x", 0]}
    ],
    "links": []
  })";

  Graph graph;
  auto result = DeserializeGraph(json, graph, reg);
  UNSCOPED_INFO(result.error);
  REQUIRE(result.ok());
  UNSCOPED_INFO("non-numeric pos must surface as a warning");
  CHECK_FALSE(result.warnings.empty());
  UNSCOPED_INFO("bad node must be skipped");
  CHECK(graph.Nodes().empty());
}

TEST_CASE("SerializeTest GraphResetCallbackFiresOnDeserialize",
          "[filterdesigner]") {
  // GraphEditor relies on the OnReset hook to re-attach the CreationPopup
  // after the deserializer rebuilds the underlying ImNodeFlow. Pin that
  // contract here so a future refactor of Graph::Reset can't silently drop
  // the call without a failing test.
  NodeRegistry reg;
  RegisterFakes(reg);

  Graph graph;
  int hits = 0;
  graph.SetOnReset([&hits] { ++hits; });

  std::string empty = R"({"version": 2, "nodes": [], "links": []})";
  auto result = DeserializeGraph(empty, graph, reg);
  UNSCOPED_INFO(result.error);
  REQUIRE(result.ok());
  UNSCOPED_INFO("Reset (via deserialize) must fire the callback");
  CHECK(hits == 1);

  // Direct Reset() also fires it.
  graph.Reset();
  CHECK(hits == 2);
}

TEST_CASE("SerializeTest NewNodesAfterLoadDontCollideWithLoadedIds",
          "[filterdesigner]") {
  NodeRegistry reg;
  RegisterFakes(reg);

  std::string json = R"({
    "version": 2,
    "nodes": [
      {"id": 5, "type": "FakeSource", "pos": [0, 0]},
      {"id": 12, "type": "FakeSink", "pos": [100, 0]}
    ],
    "links": []
  })";

  Graph graph;
  auto result = DeserializeGraph(json, graph, reg);
  UNSCOPED_INFO(result.error);
  REQUIRE(result.ok());

  // The next added node should get id > 12, not collide with a loaded one.
  auto fresh = graph.AddNode<FakeSourceNode>(ImVec2{200.0f, 0.0f});
  CHECK(fresh->GraphId() > 12);
}

}  // namespace
