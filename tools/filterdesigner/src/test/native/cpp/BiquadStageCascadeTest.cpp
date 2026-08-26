// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include <memory>
#include <string>

#include <ImNodeFlow.h>
#include <catch2/catch_test_macros.hpp>

#include "TestAssertions.hpp"
#include "wpi/filterdesigner/graph/Graph.hpp"
#include "wpi/filterdesigner/graph/NodeRegistry.hpp"
#include "wpi/filterdesigner/graph/Serialize.hpp"
#include "wpi/filterdesigner/nodes/BiquadStageNode.hpp"
#include "wpi/filterdesigner/nodes/CodeGenNode.hpp"
#include "wpi/filterdesigner/nodes/ImpulseNode.hpp"
#include "wpi/filterdesigner/nodes/ImpulseNodeLogic.hpp"

namespace {

using wpi::filterdesigner::BiquadStageNode;
using wpi::filterdesigner::CodeGenNode;
using wpi::filterdesigner::DeserializeGraph;
using wpi::filterdesigner::Graph;
using wpi::filterdesigner::NodeRegistry;
using wpi::filterdesigner::SerializeGraph;

TEST_CASE("BiquadStageCascadeTest UnchainedStageFilterEmitsOwnSectionsOnly",
          "[filterdesigner]") {
  Graph graph;
  auto stage = graph.AddNode<BiquadStageNode>(ImVec2{0.0f, 0.0f});
  stage->Logic().sampleRate = 1000.0;
  // Defaults design something sensible — exact section count isn't the
  // point; we just need a non-null combined filter to compare against.
  const auto* combined = stage->CombinedFilter();
  REQUIRE(combined != nullptr);
  const auto* own = stage->Logic().Filter();
  REQUIRE(own != nullptr);
  CHECK(combined->sections.size() == own->sections.size());
  CHECK_DOUBLE_EQ(combined->sampleRate, own->sampleRate);
}

TEST_CASE("BiquadStageCascadeTest ChainedStagesEmitCumulativeCascade",
          "[filterdesigner]") {
  // The why-cascade-on-filter-pin demo: Stage A → Stage B → CodeGen exports
  // A+B, not just B. Users opt in to "just this stage" by wiring earlier in
  // the Signal chain.
  Graph graph;
  auto stageA = graph.AddNode<BiquadStageNode>(ImVec2{0.0f, 0.0f});
  auto stageB = graph.AddNode<BiquadStageNode>(ImVec2{200.0f, 0.0f});
  stageA->Logic().sampleRate = 1000.0;
  stageB->Logic().sampleRate = 1000.0;
  stageB->inPin("in")->createLink(stageA->outPin("signal"));

  const auto* combinedB = stageB->CombinedFilter();
  REQUIRE(combinedB != nullptr);
  const auto* ownA = stageA->Logic().Filter();
  const auto* ownB = stageB->Logic().Filter();
  REQUIRE(ownA != nullptr);
  REQUIRE(ownB != nullptr);
  CHECK(combinedB->sections.size() ==
        ownA->sections.size() + ownB->sections.size());
  CHECK_DOUBLE_EQ(combinedB->sampleRate, 1000.0);
}

TEST_CASE("BiquadStageCascadeTest ChainedStagesCacheStablePointer",
          "[filterdesigner]") {
  // Pointer stability matters — downstream sinks may compare pointer
  // identity. Repeated calls without param/topology changes must return
  // the same pointer.
  Graph graph;
  auto stageA = graph.AddNode<BiquadStageNode>(ImVec2{0.0f, 0.0f});
  auto stageB = graph.AddNode<BiquadStageNode>(ImVec2{200.0f, 0.0f});
  stageB->inPin("in")->createLink(stageA->outPin("signal"));

  const auto* first = stageB->CombinedFilter();
  const auto* second = stageB->CombinedFilter();
  CHECK(first == second);
}

TEST_CASE("BiquadStageCascadeTest SampleRateMismatchSurfacesAsCombinedError",
          "[filterdesigner]") {
  Graph graph;
  auto stageA = graph.AddNode<BiquadStageNode>(ImVec2{0.0f, 0.0f});
  auto stageB = graph.AddNode<BiquadStageNode>(ImVec2{200.0f, 0.0f});
  stageA->Logic().sampleRate = 1000.0;
  stageB->Logic().sampleRate = 2000.0;  // mismatch
  stageB->inPin("in")->createLink(stageA->outPin("signal"));

  const auto* combined = stageB->CombinedFilter();
  CHECK(combined == nullptr);
  CHECK(stageB->CombinedError().find("Sample rate") != std::string::npos);
}

TEST_CASE("BiquadStageCascadeTest CombinedFilterSurvivesSerializeDeserialize",
          "[filterdesigner]") {
  // Two-stage cascade A → B → CodeGen, save, reload, then pull
  // CombinedFilter() on the restored stage B and confirm the cumulative
  // section count matches A + B. Pairs with the topology round-trip test
  // to verify the math survives serialize too.
  NodeRegistry reg;
  BiquadStageNode::Register(reg);
  CodeGenNode::Register(reg);

  Graph graph;
  auto stageA = graph.AddNode<BiquadStageNode>(ImVec2{0.0f, 0.0f});
  auto stageB = graph.AddNode<BiquadStageNode>(ImVec2{200.0f, 0.0f});
  auto codegen = graph.AddNode<CodeGenNode>(ImVec2{400.0f, 0.0f});
  stageA->Logic().sampleRate = 1000.0;
  stageB->Logic().sampleRate = 1000.0;
  stageB->inPin("in")->createLink(stageA->outPin("signal"));
  codegen->inPin("in")->createLink(stageB->outPin("filter"));
  int aId = stageA->GraphId();
  int bId = stageB->GraphId();

  std::string json = SerializeGraph(graph);

  Graph restored;
  auto result = DeserializeGraph(json, restored, reg);
  UNSCOPED_INFO(result.error);
  REQUIRE(result.ok());
  auto* restoredA = dynamic_cast<BiquadStageNode*>(restored.FindNodeById(aId));
  auto* restoredB = dynamic_cast<BiquadStageNode*>(restored.FindNodeById(bId));
  REQUIRE(restoredA != nullptr);
  REQUIRE(restoredB != nullptr);
  const auto* combinedB = restoredB->CombinedFilter();
  REQUIRE(combinedB != nullptr);
  const auto* ownA = restoredA->Logic().Filter();
  const auto* ownB = restoredB->Logic().Filter();
  REQUIRE(ownA != nullptr);
  REQUIRE(ownB != nullptr);
  CHECK(combinedB->sections.size() ==
        ownA->sections.size() + ownB->sections.size());
}

TEST_CASE("BiquadStageCascadeTest NonBiquadUpstreamYieldsThisStageOnly",
          "[filterdesigner]") {
  // ImpulseSource → BiquadStage → CodeGen. The Impulse-style source isn't a
  // BiquadStage, so the dynamic_cast in UpstreamStage() must return null
  // and the cascade collapses to just this stage's sections. Closes the
  // "dynamic_cast nullptr branch is uncovered" gap.
  Graph graph;
  auto impulse =
      graph.AddNode<wpi::filterdesigner::ImpulseNode>(ImVec2{0.0f, 0.0f});
  auto stage = graph.AddNode<BiquadStageNode>(ImVec2{200.0f, 0.0f});
  impulse->Logic().sampleRate = 1000.0;
  impulse->Logic().length = 64;
  stage->Logic().sampleRate = 1000.0;
  stage->inPin("in")->createLink(impulse->outPin("out"));

  const auto* combined = stage->CombinedFilter();
  REQUIRE(combined != nullptr);
  const auto* own = stage->Logic().Filter();
  REQUIRE(own != nullptr);
  // Same section count as the single stage — nothing got prepended from
  // the non-BiquadStage upstream.
  CHECK(combined->sections.size() == own->sections.size());
}

TEST_CASE("BiquadStageCascadeTest CycleGuardCatchesTwoNodeCycleWithoutCrashing",
          "[filterdesigner]") {
  // A.signal → B.in and B.signal → A.in. ImNodeFlow refuses same-node
  // links so a length-1 self-loop can't be wired through the public API,
  // but a two-node cycle slips past the same-parent guard. Without the
  // depth guard, CombinedFilter() would recurse unbounded between A and B
  // and stack-overflow on the per-frame walk; the guard turns that into a
  // nullptr + cycle error. Graph-level cycle detection (TopologyTest) is
  // the primary defense in production; this test pins the per-stage
  // backstop that fires when callers walk upstream directly.
  Graph graph;
  auto a = graph.AddNode<BiquadStageNode>(ImVec2{0.0f, 0.0f});
  auto b = graph.AddNode<BiquadStageNode>(ImVec2{200.0f, 0.0f});
  a->Logic().sampleRate = 1000.0;
  b->Logic().sampleRate = 1000.0;
  b->inPin("in")->createLink(a->outPin("signal"));
  a->inPin("in")->createLink(b->outPin("signal"));

  const auto* combined = a->CombinedFilter();
  CHECK(combined == nullptr);
  UNSCOPED_INFO("expected cycle-guard message, got: " << a->CombinedError());
  CHECK(a->CombinedError().find("cycle") != std::string::npos);
}

TEST_CASE("BiquadStageCascadeTest UpstreamErrorForReportsUnwiredAsEmpty",
          "[filterdesigner]") {
  // Helper that sinks call to differentiate "no input wired" from "input
  // wired but errored": no link → empty string.
  Graph graph;
  auto codegen = graph.AddNode<CodeGenNode>(ImVec2{0.0f, 0.0f});
  CHECK(BiquadStageNode::UpstreamErrorFor(codegen->inPin("in")).empty());
}

TEST_CASE("BiquadStageCascadeTest UpstreamErrorForReportsStageDesignError",
          "[filterdesigner]") {
  // Wire a deliberately broken BiquadStage to the sink and verify the
  // helper surfaces the upstream's error string.
  Graph graph;
  auto stage = graph.AddNode<BiquadStageNode>(ImVec2{0.0f, 0.0f});
  auto codegen = graph.AddNode<CodeGenNode>(ImVec2{200.0f, 0.0f});
  // Negative sample rate fails the Filter() factory's pre-check.
  stage->Logic().sampleRate = -1.0;
  codegen->inPin("in")->createLink(stage->outPin("filter"));

  // Force the upstream to populate its error state.
  REQUIRE(stage->CombinedFilter() == nullptr);
  CHECK_FALSE(stage->CombinedError().empty());
  CHECK_FALSE(BiquadStageNode::UpstreamErrorFor(codegen->inPin("in")).empty());
}

}  // namespace
