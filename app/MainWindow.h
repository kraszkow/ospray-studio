// ======================================================================== //
// Copyright 2018-2020 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "ArcballCamera.h"
// glfw
#include "GLFW/glfw3.h"
// ospray sg
#include "sg/Frame.h"
#include "sg/renderer/MaterialRegistry.h"
// std
#include <functional>

using namespace rkcommon::math;
using namespace ospray;
using rkcommon::make_unique;

enum class OSPRayRendererType
{
  SCIVIS,
  PATHTRACER,
  DEBUGGER,
  OTHER
};

class MainWindow
{
 public:
  MainWindow(const vec2i &windowSize, bool denoiser = false);

  ~MainWindow();

  static MainWindow *getActiveWindow();

  void registerDisplayCallback(std::function<void(MainWindow *)> callback);

  void registerImGuiCallback(std::function<void()> callback);

  void mainLoop();

  void parseCommandLine(int &ac, const char **&av);
  void importFiles();

 protected:
  void updateCamera();

  void reshape(const vec2i &newWindowSize);
  void motion(const vec2f &position);
  void display();
  void startNewOSPRayFrame();
  void waitOnOSPRayFrame();
  void updateTitleBar();
  void buildUI();
  void refreshRenderer();
  void refreshScene();
  void refreshMaterialRegistry();
  void refreshLight();
  void addLight();
  void removeLight();
  void refreshMaterial();
  void addPTMaterials();
  bool importGeometry(std::shared_ptr<sg::Node> &world);
  bool importVolume(std::shared_ptr<sg::Node> &world);
  void saveCurrentFrame();

  // menu and window UI
  void buildMainMenu();
  void buildMainMenuFile();
  void buildMainMenuEdit();
  void buildMainMenuView();
  void buildWindows();
  void buildWindowPreferences();
  void buildWindowKeyframes();
  void buildWindowLightEditor();

  // imgui window visibility toggles
  bool showPreferences{false};
  bool showKeyframes{false};
  bool showLightEditor{false};

  // imgui-controlled options
  std::string screenshotFiletype{"png"};
  bool screenshotDepth{false};

  static MainWindow *activeWindow;

  vec2i windowSize;
  vec2i fbSize;
  vec2f previousMouse{-1.f};

  bool denoiserAvailable{false};
  bool showAlbedo{false};
  bool cancelFrameOnInteraction{false};
  bool autorotate{false};

  std::string scene;
  std::vector<std::string> filesToImport;

  OSPRayRendererType rendererType{OSPRayRendererType::SCIVIS};
  std::string rendererTypeStr{"scivis"};
  std::string lightTypeStr{"ambient"};
  std::string matTypeStr{"obj"};
  bool useTestTex     = false;
  bool useImportedTex = false;

  // GLFW window instance
  GLFWwindow *glfwWindow = nullptr;

  // Arcball camera instance
  std::unique_ptr<ArcballCamera> arcballCamera;

  std::shared_ptr<sg::Frame> frame;

  std::shared_ptr<sg::MaterialRegistry> baseMaterialRegistry;

  // OpenGL framebuffer texture
  GLuint framebufferTexture = 0;

  // optional registered display callback, called before every display()
  std::function<void(MainWindow *)> displayCallback;

  // toggles display of ImGui UI, if an ImGui callback is provided
  bool showUi = true;

  // optional registered ImGui callback, called during every frame to build UI
  std::function<void()> uiCallback;

  // FPS measurement of last frame
  float latestFPS{0.f};

  // auto rotation speed, 1=0.1% window width mouse movement, 100=10%
  int autorotateSpeed{1};
};
