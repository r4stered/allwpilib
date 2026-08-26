// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/filterdesigner/nodes/WpiLogSourceNodeLogic.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "TestAssertions.hpp"
#include "wpi/datalog/DataLogWriter.hpp"
#include "wpi/util/Logger.hpp"
#include "wpi/util/raw_ostream.hpp"

namespace {

using wpi::filterdesigner::WpiLogSourceNodeLogic;

class WpiLogSourceNodeLogicTest {
 public:
  wpi::util::Logger msglog;
  std::vector<uint8_t> data;
  wpi::log::DataLogWriter log{
      msglog, std::make_unique<wpi::util::raw_uvector_ostream>(data)};
};

TEST_CASE_METHOD(WpiLogSourceNodeLogicTest,
                 "WpiLogSourceNodeLogicTest FreshLogicIsEmpty",
                 "[filterdesigner]") {
  WpiLogSourceNodeLogic logic;
  CHECK_FALSE(logic.HasFile());
  CHECK(logic.Signal() == nullptr);
  CHECK(logic.LogPath().empty());
  CHECK(logic.SelectedEntry().empty());
  CHECK(logic.LoadError().empty());
}

TEST_CASE_METHOD(
    WpiLogSourceNodeLogicTest,
    "WpiLogSourceNodeLogicTest OpenBufferAndSelectEntryExposesSignal",
    "[filterdesigner]") {
  wpi::log::DoubleLogEntry d{log, "/accel/x", 0};
  d.Append(0.5, 1'000'000);
  d.Append(0.75, 2'000'000);
  d.Append(1.0, 3'000'000);
  log.Flush();

  WpiLogSourceNodeLogic logic;
  REQUIRE(logic.OpenBuffer(data));
  CHECK(logic.HasFile());
  UNSCOPED_INFO("no entry selected yet");
  CHECK(logic.Signal() == nullptr);

  REQUIRE(logic.SelectEntry("/accel/x"));
  const auto* sig = logic.Signal();
  REQUIRE(sig != nullptr);
  CHECK(sig->name == "/accel/x");
  REQUIRE(sig->values.size() == 3u);
  CHECK_DOUBLE_EQ(sig->values[0], 0.5);
  CHECK_DOUBLE_EQ(sig->values[2], 1.0);
  CHECK(sig->revision > 0u);
}

TEST_CASE_METHOD(
    WpiLogSourceNodeLogicTest,
    "WpiLogSourceNodeLogicTest SelectMissingEntryKeepsPreviousAndReportsError",
    "[filterdesigner]") {
  wpi::log::DoubleLogEntry d{log, "good", 0};
  d.Append(1.0, 1'000);
  log.Flush();

  WpiLogSourceNodeLogic logic;
  REQUIRE(logic.OpenBuffer(data));
  REQUIRE(logic.SelectEntry("good"));
  const auto* before = logic.Signal();
  REQUIRE(before != nullptr);

  CHECK_FALSE(logic.SelectEntry("missing"));
  CHECK_FALSE(logic.LoadError().empty());
  // Previous selection is preserved on a failed pick.
  CHECK(logic.Signal() == before);
  CHECK(logic.SelectedEntry() == "good");
}

TEST_CASE_METHOD(
    WpiLogSourceNodeLogicTest,
    "WpiLogSourceNodeLogicTest RestoreFromPathOpensFileAndPicksEntry",
    "[filterdesigner]") {
  wpi::log::DoubleLogEntry d{log, "persisted", 0};
  d.Append(2.0, 1'000);
  d.Append(4.0, 2'000);
  log.Flush();

  auto tmp = std::filesystem::temp_directory_path() /
             ("filterdesigner_node_test_" +
              std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count()) +
              ".wpilog");
  {
    std::ofstream out{tmp, std::ios::binary};
    REQUIRE(out.is_open());
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
  }

  WpiLogSourceNodeLogic logic;
  logic.RestoreFromPath(tmp.string(), "persisted");
  CHECK(logic.HasFile());
  CHECK(logic.SelectedEntry() == "persisted");
  REQUIRE(logic.Signal() != nullptr);
  CHECK(logic.Signal()->values.size() == 2u);
  CHECK(logic.LoadError().empty());

  std::filesystem::remove(tmp);
}

TEST_CASE_METHOD(
    WpiLogSourceNodeLogicTest,
    "WpiLogSourceNodeLogicTest RestoreFromMissingPathLeavesErrorState",
    "[filterdesigner]") {
  WpiLogSourceNodeLogic logic;
  logic.RestoreFromPath("/no/such/file.wpilog", "entry");
  CHECK_FALSE(logic.HasFile());
  CHECK(logic.Signal() == nullptr);
  UNSCOPED_INFO("missing file should produce a re-pick banner, not throw");
  CHECK_FALSE(logic.LoadError().empty());
  UNSCOPED_INFO("path is remembered so the UI can offer to re-pick");
  CHECK(logic.LogPath() == "/no/such/file.wpilog");
}

TEST_CASE_METHOD(WpiLogSourceNodeLogicTest,
                 "WpiLogSourceNodeLogicTest "
                 "RestoreWithMissingEntryLoadsFileAndSurfacesError",
                 "[filterdesigner]") {
  wpi::log::DoubleLogEntry d{log, "still_here", 0};
  d.Append(1.0, 1'000);
  log.Flush();

  auto tmp = std::filesystem::temp_directory_path() /
             ("filterdesigner_node_test_" +
              std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count()) +
              "_b.wpilog");
  {
    std::ofstream out{tmp, std::ios::binary};
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
  }

  WpiLogSourceNodeLogic logic;
  logic.RestoreFromPath(tmp.string(), "gone");
  UNSCOPED_INFO("log still opens");
  CHECK(logic.HasFile());
  UNSCOPED_INFO("but the named entry is missing");
  CHECK(logic.Signal() == nullptr);
  CHECK_FALSE(logic.LoadError().empty());

  std::filesystem::remove(tmp);
}

TEST_CASE_METHOD(WpiLogSourceNodeLogicTest,
                 "WpiLogSourceNodeLogicTest RevisionAdvancesAcrossSelections",
                 "[filterdesigner]") {
  wpi::log::DoubleLogEntry a{log, "a", 0};
  wpi::log::DoubleLogEntry b{log, "b", 0};
  a.Append(1.0, 1'000);
  b.Append(2.0, 2'000);
  log.Flush();

  WpiLogSourceNodeLogic logic;
  REQUIRE(logic.OpenBuffer(data));

  REQUIRE(logic.SelectEntry("a"));
  auto firstRev = logic.Signal()->revision;
  REQUIRE(logic.SelectEntry("b"));
  CHECK(logic.Signal()->revision > firstRev);
}

}  // namespace
