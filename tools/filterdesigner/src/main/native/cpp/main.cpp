// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include <memory>
#include <string_view>

#include <imgui.h>

#include "wpi/glass/Context.hpp"
#include "wpi/glass/MainMenuBar.hpp"
#include "wpi/glass/Storage.hpp"
#include "wpi/gui/wpigui.hpp"

namespace gui = wpi::gui;

const char* GetWPILibVersion();

namespace filterdesigner {
std::string_view GetResource_filterdesigner_16_png();
std::string_view GetResource_filterdesigner_32_png();
std::string_view GetResource_filterdesigner_48_png();
std::string_view GetResource_filterdesigner_64_png();
std::string_view GetResource_filterdesigner_128_png();
std::string_view GetResource_filterdesigner_256_png();
std::string_view GetResource_filterdesigner_512_png();
}  // namespace filterdesigner

static bool gMainWindowVisible = true;

static void DisplayMainWindow() {
  if (!gMainWindowVisible) {
    return;
  }

  ImGui::SetNextWindowPos(ImVec2{50, 50}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2{800, 600}, ImGuiCond_FirstUseEver);

  if (ImGui::Begin("Filter Designer", &gMainWindowVisible)) {
    ImGui::Text("Welcome to WPILib Filter Designer!");
    ImGui::Separator();

    ImGui::TextWrapped(
        "This tool will help you design and test digital filters for sensor "
        "data. You will be able to:");

    ImGui::BulletText("Load sensor data from .wpilog files");
    ImGui::BulletText("Stream live data from NetworkTables");
    ImGui::BulletText("Design filters (Butterworth, Chebyshev, etc.)");
    ImGui::BulletText("Visualize frequency response");
    ImGui::BulletText("Export to LinearFilter coefficients");

    ImGui::Separator();
    ImGui::Text("Phase 1: Basic GUI - COMPLETE!");
  }
  ImGui::End();
}

static void DisplayMainMenu() {
  ImGui::BeginMainMenuBar();

  static wpi::glass::MainMenuBar mainMenu;
  mainMenu.WorkspaceMenu();
  gui::EmitViewMenu();

  if (ImGui::BeginMenu("Window")) {
    ImGui::MenuItem("Main Window", nullptr, &gMainWindowVisible);
    ImGui::EndMenu();
  }

  bool about = false;
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("About")) {
      about = true;
    }
    ImGui::EndMenu();
  }

  ImGui::EndMainMenuBar();

  if (about) {
    ImGui::OpenPopup("About");
  }
  if (ImGui::BeginPopupModal("About")) {
    ImGui::Text("WPILib Filter Designer");
    ImGui::Separator();
    ImGui::Text("v%s", GetWPILibVersion());
    ImGui::Separator();
    ImGui::Text("A tool for designing digital filters for FRC sensor data");
    ImGui::Text("Save location: %s", wpi::glass::GetStorageDir().c_str());
    ImGui::Text("%.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate,
                ImGui::GetIO().Framerate);
    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

static void DisplayGui() {
  DisplayMainMenu();
  DisplayMainWindow();
}

void Application(std::string_view saveDir) {
  gui::CreateContext();
  wpi::glass::CreateContext();

  // Add icons
  gui::AddIcon(filterdesigner::GetResource_filterdesigner_16_png());
  gui::AddIcon(filterdesigner::GetResource_filterdesigner_32_png());
  gui::AddIcon(filterdesigner::GetResource_filterdesigner_48_png());
  gui::AddIcon(filterdesigner::GetResource_filterdesigner_64_png());
  gui::AddIcon(filterdesigner::GetResource_filterdesigner_128_png());
  gui::AddIcon(filterdesigner::GetResource_filterdesigner_256_png());
  gui::AddIcon(filterdesigner::GetResource_filterdesigner_512_png());

  wpi::glass::SetStorageName("filterdesigner");
  wpi::glass::SetStorageDir(saveDir.empty() ? gui::GetPlatformSaveFileDir()
                                            : saveDir);

  gui::AddLateExecute(DisplayGui);
  gui::Initialize("Filter Designer", 1024, 768);

  gui::Main();

  wpi::glass::DestroyContext();
  gui::DestroyContext();
}

#ifdef _WIN32
int __stdcall WinMain(void* hInstance, void* hPrevInstance, char* pCmdLine,
                      int nCmdShow) {
  int argc = __argc;
  char** argv = __argv;
#else
int main(int argc, char** argv) {
#endif
  std::string_view saveDir;
  if (argc == 2) {
    saveDir = argv[1];
  }

  Application(saveDir);

  return 0;
}
