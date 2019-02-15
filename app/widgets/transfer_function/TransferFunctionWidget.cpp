// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
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

#include "TransferFunctionWidget.h"

// stl
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
// imgui
#include <imconfig.h>
#include <imgui.h>
#include "imguifilesystem/imguifilesystem.h"
// ospcommon
#include "ospray/ospcommon/FileName.h"
// job_scheduler
#include "../../jobs/JobScheduler.h"
// sg_ui
#include "../sg_ui/ospray_sg_ui.h"
// ospray_sg
#include "sg/visitor/MarkAllAsModified.h"

using namespace tfn;
using namespace tfn_widget;
using namespace ospcommon;

namespace help {

  template <typename T>
  int find_idx(const T &A, float p, int l = -1, int r = -1)
  {
    l = l == -1 ? 0 : l;
    r = r == -1 ? A.size() - 1 : r;

    int m = (r + l) / 2;
    if (A[l].x > p) {
      return l;
    } else if (A[r].x <= p) {
      return r + 1;
    } else if ((m == l) || (m == r)) {
      return m + 1;
    } else {
      if (A[m].x <= p) {
        return find_idx(A, p, m, r);
      } else {
        return find_idx(A, p, l, m);
      }
    }
  }

  float lerp(const float &l,
             const float &r,
             const float &pl,
             const float &pr,
             const float &p)
  {
    const float dl = std::abs(pr - pl) > 0.0001f ? (p - pl) / (pr - pl) : 0.f;
    const float dr = 1.f - dl;
    return l * dr + r * dl;
  }

}  // namespace help

void TransferFunctionWidget::LoadDefaultMap()
{
  std::vector<ColorPoint> colors;
  std::vector<OpacityPoint> opacities;

  // Jet //

  colors.emplace_back(0.0f, 0.f, 0.f, 1.f);
  colors.emplace_back(0.3f, 0.f, 1.f, 1.f);
  colors.emplace_back(0.6f, 1.f, 1.f, 0.f);
  colors.emplace_back(1.0f, 1.f, 0.f, 0.f);

  opacities.emplace_back(0.00f, 0.00f);
  opacities.emplace_back(1.00f, 1.00f);

  tfn_c_list.push_back(colors);
  tfn_o_list.push_back(opacities);

  tfn_editable.push_back(true);
  tfn_names.push_back("Jet");

  colors.clear();

  // Ice Fire //

  float spacing = 1.f / 16;

  colors.emplace_back(0 * spacing, 0, 0, 0);
  colors.emplace_back(1 * spacing, 0, 0.120394, 0.302678);
  colors.emplace_back(2 * spacing, 0, 0.216587, 0.524575);
  colors.emplace_back(3 * spacing, 0.0552529, 0.345022, 0.659495);
  colors.emplace_back(4 * spacing, 0.128054, 0.492592, 0.720287);
  colors.emplace_back(5 * spacing, 0.188952, 0.641306, 0.792096);
  colors.emplace_back(6 * spacing, 0.327672, 0.784939, 0.873426);
  colors.emplace_back(7 * spacing, 0.60824, 0.892164, 0.935546);
  colors.emplace_back(8 * spacing, 0.881376, 0.912184, 0.818097);
  colors.emplace_back(9 * spacing, 0.9514, 0.835615, 0.449271);
  colors.emplace_back(10 * spacing, 0.904479, 0.690486, 0);
  colors.emplace_back(11 * spacing, 0.854063, 0.510857, 0);
  colors.emplace_back(12 * spacing, 0.777096, 0.330175, 0.000885023);
  colors.emplace_back(13 * spacing, 0.672862, 0.139086, 0.00270085);
  colors.emplace_back(14 * spacing, 0.508812, 0, 0);
  colors.emplace_back(15 * spacing, 0.299413, 0.000366217, 0.000549325);

  colors.emplace_back(1.f, 0.0157473, 0.00332647, 0);

  tfn_c_list.push_back(colors);
  tfn_o_list.push_back(opacities);

  tfn_editable.push_back(true);
  tfn_names.push_back("Ice Fire");
};

void TransferFunctionWidget::SetTFNSelection(int selection)
{
  if (tfn_selection != selection) {
    tfn_selection = selection;
    // Remember to update other constructors as well
    tfn_c = &(tfn_c_list[selection]);
#if 0  // NOTE(jda) - this will use the first tf's opacities for all color maps
    tfn_o       = &(tfn_o_list[selection]);
#endif
    tfn_edit    = tfn_editable[selection];
    tfn_changed = true;
  }
}

TransferFunctionWidget::~TransferFunctionWidget()
{
  if (tfn_palette) {
    glDeleteTextures(1, &tfn_palette);
  }
}

TransferFunctionWidget::TransferFunctionWidget(
    std::shared_ptr<sg::TransferFunction> tfn)
    : tfn_text_buffer(512, '\0')
{
  tfn_editable.reserve(20);
  tfn_c_list.reserve(20);
  tfn_o_list.reserve(20);
  tfn_names.reserve(20);

  sg_tfn = tfn;

  tfn_sample_set = [&](const std::vector<ColorPoint> &c,
                       const std::vector<OpacityPoint> &a) {
    job_scheduler::scheduleNodeOp([&]() {
      auto colors = ospray::sg::createNode("colorControlPoints", "DataVector4f")
                        ->nodeAs<ospray::sg::DataVector4f>();
      auto alphas =
          ospray::sg::createNode("opacityControlPoints", "DataVector2f")
              ->nodeAs<ospray::sg::DataVector2f>();
      colors->v.resize(c.size());
      alphas->v.resize(a.size());

      std::copy(c.data(), c.data() + c.size(), colors->v.data());
      std::transform(a.begin(), a.end(), alphas->v.begin(), [&](auto &v) {
        return OpacityPoint(v.x, v.y * globalOpacityScale);
      });

      sg_tfn->add(colors);
      sg_tfn->add(alphas);
      sg_tfn->updateChildDataValues();

      sg_tfn->traverse(sg::MarkAllAsModified{});
    });
  };

  numSamples = tfn->child("numSamples").valueAs<int>();

  LoadDefaultMap();

  tfn_c    = &(tfn_c_list[tfn_selection]);
  tfn_o    = &(tfn_o_list[tfn_selection]);
  tfn_edit = tfn_editable[tfn_selection];

  tfn_sample_set(*tfn_c, *tfn_o);
}

TransferFunctionWidget::TransferFunctionWidget(
    const TransferFunctionWidget &core)
    : tfn_c_list(core.tfn_c_list),
      tfn_o_list(core.tfn_o_list),
      tfn_readers(core.tfn_readers),
      tfn_selection(core.tfn_selection),
      tfn_changed(true),
      tfn_palette(0),
      tfn_text_buffer(512, '\0'),
      tfn_sample_set(core.tfn_sample_set)
{
  tfn_c    = &(tfn_c_list[tfn_selection]);
  tfn_o    = &(tfn_o_list[tfn_selection]);
  tfn_edit = tfn_editable[tfn_selection];
}

TransferFunctionWidget &TransferFunctionWidget::operator=(
    const tfn::tfn_widget::TransferFunctionWidget &core)
{
  if (this == &core) {
    return *this;
  }
  tfn_c_list     = core.tfn_c_list;
  tfn_o_list     = core.tfn_o_list;
  tfn_readers    = core.tfn_readers;
  tfn_selection  = core.tfn_selection;
  tfn_changed    = true;
  tfn_palette    = 0;
  tfn_sample_set = core.tfn_sample_set;
  return *this;
}

void TransferFunctionWidget::drawUI()
{
  if (!ImGui::Begin("Transfer Function Widget")) {
    ImGui::End();
    return;
  }

  ImGui::Text("Linear Transfer Function");
  // radio paremeters
  ImGui::Separator();
  std::vector<const char *> names(tfn_names.size(), nullptr);
  std::transform(tfn_names.begin(),
                 tfn_names.end(),
                 names.begin(),
                 [](const std::string &t) { return t.c_str(); });
  int newSelection = tfn_selection;
  ImGui::ListBox("Color maps", &newSelection, names.data(), names.size());

  static ImGuiFs::Dialog openFileDialog;

  const bool pressed         = ImGui::Button("Choose File...");
  const std::string fileName = openFileDialog.chooseFileDialog(pressed);

  static std::string fileToOpen;

  if (!fileName.empty())
    fileToOpen = fileName;

  ImGui::SameLine();

  std::array<char, 512> buf;
  strcpy(buf.data(), fileToOpen.c_str());

  if (ImGui::InputText(
          "", buf.data(), buf.size(), ImGuiInputTextFlags_EnterReturnsTrue))
    fileToOpen = buf.data();

  if (ImGui::Button("Load##tfn_editor")) {
    try {
      load(fileToOpen.c_str());
    } catch (const std::runtime_error &error) {
      std::cerr << "\033[1;33m"
                << "Error:" << error.what() << "\033[0m" << std::endl;
    }
  }

  ImGui::Separator();
  ImGui::Separator();

  // TODO: save function is not implemented
  // if (ImGui::Button("save")) { save(tfn_text_buffer.data()); }

  auto &valueRangeNode = sg_tfn->child("valueRange");
  guiSGSingleNode("valueRange", valueRangeNode);

  ImGui::Text("opacity scale");
  ImGui::SameLine();
  if (ImGui::SliderFloat("##OpacityScale", &globalOpacityScale, 0.f, 10.f))
    tfn_changed = true;

  SetTFNSelection(newSelection);

  drawUI_currentTF();

  ImGui::End();
}

void TransferFunctionWidget::drawUI_currentTF()
{
  //------------ Transfer Function -------------------
  // style
  // only God and me know what do they do ...
  ImDrawList *draw_list   = ImGui::GetWindowDrawList();
  float canvas_x          = ImGui::GetCursorScreenPos().x;
  float canvas_y          = ImGui::GetCursorScreenPos().y;
  float canvas_avail_x    = ImGui::GetContentRegionAvail().x;
  float canvas_avail_y    = ImGui::GetContentRegionAvail().y;
  const float mouse_x     = ImGui::GetMousePos().x;
  const float mouse_y     = ImGui::GetMousePos().y;
  const float scroll_x    = ImGui::GetScrollX();
  const float scroll_y    = ImGui::GetScrollY();
  const float margin      = 10.f;
  const float width       = canvas_avail_x - 2.f * margin;
  const float height      = 260.f;
  const float color_len   = 9.f;
  const float opacity_len = 7.f;
  // draw preview texture
  ImGui::SetCursorScreenPos(ImVec2(canvas_x + margin, canvas_y));
  ImGui::Image(reinterpret_cast<void *>(tfn_palette), ImVec2(width, height));
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Double left click to add new control point");

  ImGui::SetCursorScreenPos(ImVec2(canvas_x, canvas_y));
  for (int i = 0; i < tfn_o->size() - 1; ++i) {
    std::vector<ImVec2> polyline;
    polyline.emplace_back(canvas_x + margin + (*tfn_o)[i].x * width,
                          canvas_y + height);
    polyline.emplace_back(canvas_x + margin + (*tfn_o)[i].x * width,
                          canvas_y + height - (*tfn_o)[i].y * height);
    polyline.emplace_back(canvas_x + margin + (*tfn_o)[i + 1].x * width + 1,
                          canvas_y + height - (*tfn_o)[i + 1].y * height);
    polyline.emplace_back(canvas_x + margin + (*tfn_o)[i + 1].x * width + 1,
                          canvas_y + height);
    draw_list->AddConvexPolyFilled(
        polyline.data(), polyline.size(), 0xFFD8D8D8);
  }
  canvas_y += height + margin;
  canvas_avail_y -= height + margin;
  // draw color control points
  ImGui::SetCursorScreenPos(ImVec2(canvas_x, canvas_y));
  if (tfn_edit) {
    // draw circle background
    draw_list->AddRectFilled(
        ImVec2(canvas_x + margin, canvas_y - margin),
        ImVec2(canvas_x + margin + width, canvas_y - margin + 2.5 * color_len),
        0xFF474646);
    // draw circles
    for (int i = tfn_c->size() - 1; i >= 0; --i) {
      const ImVec2 pos(canvas_x + width * (*tfn_c)[i].x + margin, canvas_y);
      ImGui::SetCursorScreenPos(ImVec2(canvas_x, canvas_y));
      // white background
      draw_list->AddTriangleFilled(ImVec2(pos.x - 0.5f * color_len, pos.y),
                                   ImVec2(pos.x + 0.5f * color_len, pos.y),
                                   ImVec2(pos.x, pos.y - color_len),
                                   0xFFD8D8D8);
      draw_list->AddCircleFilled(
          ImVec2(pos.x, pos.y + 0.5f * color_len), color_len, 0xFFD8D8D8);
      // draw picker
      ImVec4 picked_color =
          ImColor((*tfn_c)[i].y, (*tfn_c)[i].z, (*tfn_c)[i].w, 1.f);
      ImGui::SetCursorScreenPos(
          ImVec2(pos.x - color_len, pos.y + 1.5f * color_len));
      if (ImGui::ColorEdit4(("##ColorPicker" + std::to_string(i)).c_str(),
                            (float *)&picked_color,
                            ImGuiColorEditFlags_NoAlpha |
                                ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoLabel |
                                ImGuiColorEditFlags_AlphaPreview |
                                ImGuiColorEditFlags_NoOptions |
                                ImGuiColorEditFlags_NoTooltip)) {
        (*tfn_c)[i].y = picked_color.x;
        (*tfn_c)[i].z = picked_color.y;
        (*tfn_c)[i].w = picked_color.z;
        tfn_changed   = true;
      }
      if (ImGui::IsItemHovered()) {
        // convert float color to char
        int cr = static_cast<int>(picked_color.x * 255);
        int cg = static_cast<int>(picked_color.y * 255);
        int cb = static_cast<int>(picked_color.z * 255);
        // setup tooltip
        ImGui::BeginTooltip();
        ImVec2 sz(
            ImGui::GetFontSize() * 4 + ImGui::GetStyle().FramePadding.y * 2,
            ImGui::GetFontSize() * 4 + ImGui::GetStyle().FramePadding.y * 2);
        ImGui::ColorButton(
            "##PreviewColor",
            picked_color,
            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaPreview,
            sz);
        ImGui::SameLine();
        ImGui::Text(
            "Left click to edit\n"
            "HEX: #%02X%02X%02X\n"
            "RGB: [%3d,%3d,%3d]\n(%.2f, %.2f, %.2f)",
            cr,
            cg,
            cb,
            cr,
            cg,
            cb,
            picked_color.x,
            picked_color.y,
            picked_color.z);
        ImGui::EndTooltip();
      }
    }
    for (int i = 0; i < tfn_c->size(); ++i) {
      const ImVec2 pos(canvas_x + width * (*tfn_c)[i].x + margin, canvas_y);
      // draw button
      ImGui::SetCursorScreenPos(
          ImVec2(pos.x - color_len, pos.y - 0.5 * color_len));
      ImGui::InvisibleButton(("##ColorControl-" + std::to_string(i)).c_str(),
                             ImVec2(2.f * color_len, 2.f * color_len));
      // dark highlight
      ImGui::SetCursorScreenPos(ImVec2(pos.x - color_len, pos.y));
      draw_list->AddCircleFilled(
          ImVec2(pos.x, pos.y + 0.5f * color_len),
          0.5f * color_len,
          ImGui::IsItemHovered() ? 0xFF051C33 : 0xFFBCBCBC);
      // delete color point
      if (ImGui::IsMouseDoubleClicked(1) && ImGui::IsItemHovered()) {
        if (i > 0 && i < tfn_c->size() - 1) {
          tfn_c->erase(tfn_c->begin() + i);
          tfn_changed = true;
        }
      }
      // drag color control point
      else if (ImGui::IsItemActive()) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (i > 0 && i < tfn_c->size() - 1) {
          (*tfn_c)[i].x += delta.x / width;
          (*tfn_c)[i].x =
              clamp((*tfn_c)[i].x, (*tfn_c)[i - 1].x, (*tfn_c)[i + 1].x);
        }
        tfn_changed = true;
      }
    }
  }
  // draw opacity control points
  ImGui::SetCursorScreenPos(ImVec2(canvas_x, canvas_y));
  {
    // draw circles
    for (int i = 0; i < tfn_o->size(); ++i) {
      const ImVec2 pos(canvas_x + width * (*tfn_o)[i].x + margin,
                       canvas_y - height * (*tfn_o)[i].y - margin);
      ImGui::SetCursorScreenPos(
          ImVec2(pos.x - opacity_len, pos.y - opacity_len));
      ImGui::InvisibleButton(("##OpacityControl-" + std::to_string(i)).c_str(),
                             ImVec2(2.f * opacity_len, 2.f * opacity_len));
      ImGui::SetCursorScreenPos(ImVec2(canvas_x, canvas_y));
      // dark bounding box
      draw_list->AddCircleFilled(pos, opacity_len, 0xFF565656);
      // white background
      draw_list->AddCircleFilled(pos, 0.8f * opacity_len, 0xFFD8D8D8);
      // highlight
      draw_list->AddCircleFilled(
          pos,
          0.6f * opacity_len,
          ImGui::IsItemHovered() ? 0xFF051c33 : 0xFFD8D8D8);
      // setup interaction
      // delete opacity point
      if (ImGui::IsMouseDoubleClicked(1) && ImGui::IsItemHovered()) {
        if (i > 0 && i < tfn_o->size() - 1) {
          tfn_o->erase(tfn_o->begin() + i);
          tfn_changed = true;
        }
      } else if (ImGui::IsItemActive()) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        (*tfn_o)[i].y -= delta.y / height;
        (*tfn_o)[i].y = clamp((*tfn_o)[i].y, 0.0f, 1.0f);
        if (i > 0 && i < tfn_o->size() - 1) {
          (*tfn_o)[i].x += delta.x / width;
          (*tfn_o)[i].x =
              clamp((*tfn_o)[i].x, (*tfn_o)[i - 1].x, (*tfn_o)[i + 1].x);
        }
        tfn_changed = true;
      } else if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Double right click botton to delete point\n"
            "Left click and drag to move point");
      }
    }
  }
  // draw background interaction
  ImGui::SetCursorScreenPos(ImVec2(canvas_x + margin, canvas_y - margin));
  ImGui::InvisibleButton("##tfn_palette_color", ImVec2(width, 2.5 * color_len));
  // add color point
  if (tfn_edit && ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) {
    const float p = clamp(
        (mouse_x - canvas_x - margin - scroll_x) / (float)width, 0.f, 1.f);
    const int ir   = help::find_idx(*tfn_c, p);
    const int il   = ir - 1;
    const float pr = (*tfn_c)[ir].x;
    const float pl = (*tfn_c)[il].x;
    const float r  = help::lerp((*tfn_c)[il].y, (*tfn_c)[ir].y, pl, pr, p);
    const float g  = help::lerp((*tfn_c)[il].z, (*tfn_c)[ir].z, pl, pr, p);
    const float b  = help::lerp((*tfn_c)[il].w, (*tfn_c)[ir].w, pl, pr, p);
    ColorPoint pt(p, r, g, b);
    tfn_c->insert(tfn_c->begin() + ir, pt);
    tfn_changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Double left click to add new color point");
  }
  // draw background interaction
  ImGui::SetCursorScreenPos(
      ImVec2(canvas_x + margin, canvas_y - height - margin));
  ImGui::InvisibleButton("##tfn_palette_opacity", ImVec2(width, height));
  // add opacity point
  if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) {
    const float x = clamp(
        (mouse_x - canvas_x - margin - scroll_x) / (float)width, 0.f, 1.f);
    const float y = clamp(
        -(mouse_y - canvas_y + margin - scroll_y) / (float)height, 0.f, 1.f);
    const int idx = help::find_idx(*tfn_o, x);
    OpacityPoint pt(x, y);
    tfn_o->insert(tfn_o->begin() + idx, pt);
    tfn_changed = true;
  }
  // update cursors
  canvas_y += 4.f * color_len + margin;
  canvas_avail_y -= 4.f * color_len + margin;

  ImGui::SetCursorScreenPos(ImVec2(canvas_x, canvas_y));
  //------------ Transfer Function -------------------
}

void RenderTFNTexture(GLuint &tex, int width, int height)
{
  GLint prevBinding = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevBinding);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA8,
               width,
               height,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  if (prevBinding) {
    glBindTexture(GL_TEXTURE_2D, prevBinding);
  }
}

void TransferFunctionWidget::render()
{
  size_t tfn_w = numSamples, tfn_h = 1;
  // Upload to GL if the transfer function has changed
  if (!tfn_palette) {
    RenderTFNTexture(tfn_palette, tfn_w, tfn_h);
  }

  // Update texture color
  if (tfn_changed) {
    // Backup old states
    GLint prevBinding = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevBinding);

    // Sample the palette then upload the data
    std::vector<uint8_t> palette(tfn_w * tfn_h * 4, 0);
    std::vector<float> colors(3 * tfn_w, 1.f);
    std::vector<float> alpha(2 * tfn_w, 1.f);
    const float step = 1.0f / (float)(tfn_w - 1);
    for (int i = 0; i < tfn_w; ++i) {
      const float p = clamp(i * step, 0.0f, 1.0f);
      int ir, il;
      float pr, pl;
      // color
      ir = help::find_idx(*tfn_c, p);
      il = ir - 1;
      pr = (*tfn_c)[ir].x;
      pl = (*tfn_c)[il].x;

      const float r = help::lerp((*tfn_c)[il].y, (*tfn_c)[ir].y, pl, pr, p);
      const float g = help::lerp((*tfn_c)[il].z, (*tfn_c)[ir].z, pl, pr, p);
      const float b = help::lerp((*tfn_c)[il].w, (*tfn_c)[ir].w, pl, pr, p);

      colors[3 * i + 0] = r;
      colors[3 * i + 1] = g;
      colors[3 * i + 2] = b;

      // opacity
      ir = help::find_idx(*tfn_o, p);
      il = ir - 1;
      pr = (*tfn_o)[ir].x;
      pl = (*tfn_o)[il].x;

      const float a = help::lerp((*tfn_o)[il].y, (*tfn_o)[ir].y, pl, pr, p);

      alpha[2 * i + 0] = p;
      alpha[2 * i + 1] = a;

      // palette
      palette[i * 4 + 0] = static_cast<uint8_t>(r * 255.f);
      palette[i * 4 + 1] = static_cast<uint8_t>(g * 255.f);
      palette[i * 4 + 2] = static_cast<uint8_t>(b * 255.f);
      palette[i * 4 + 3] = 255;
    }

    // Render palette again
    glBindTexture(GL_TEXTURE_2D, tfn_palette);
    // LOGICALLY we need to resize texture of texture is resized
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 tfn_w,
                 tfn_h,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 static_cast<const void *>(palette.data()));
    // Restore previous binded texture
    if (prevBinding) {
      glBindTexture(GL_TEXTURE_2D, prevBinding);
    }

    tfn_sample_set(*tfn_c, *tfn_o);
    tfn_changed = false;
  }
}

void TransferFunctionWidget::load(const std::string &fileName)
{
  tfn_readers.emplace_back(fileName);
  const auto &tfn_new = tfn_readers.back();
  const int c_size    = tfn_new.rgbValues.size();

  // load data
  tfn_c_list.emplace_back(c_size);
  tfn_editable.push_back(false);  // TODO we dont want to edit loaded TFN

  ospcommon::FileName fName = fileName;
  tfn_names.push_back(fName.base());

  SetTFNSelection(tfn_names.size() - 1);  // set the loaded function as current

  if (c_size < 2)
    throw std::runtime_error("transfer function contains too few color points");

  const float c_step = 1.f / (c_size - 1);
  for (int i = 0; i < c_size; ++i) {
    const float p = static_cast<float>(i) * c_step;
    (*tfn_c)[i].x = p;
    (*tfn_c)[i].y = tfn_new.rgbValues[i].x;
    (*tfn_c)[i].z = tfn_new.rgbValues[i].y;
    (*tfn_c)[i].w = tfn_new.rgbValues[i].z;
  }

  tfn_changed = true;
}

void tfn::tfn_widget::TransferFunctionWidget::save(
    const std::string &fileName) const
{
  // // For opacity we can store the associated data value and only have 1 line,
  // // so just save it out directly
  // tfn::TransferFunction output(transferFunctions[tfcnSelection].name,
  //     std::vector<vec3f>(), rgbaLines[3].line, 0, 1, 1);

  // // Pull the RGB line values to compute the transfer function and save it
  // out
  // // here we may need to do some interpolation, if the RGB lines have
  // differing numbers
  // // of control points
  // // Find which x values we need to sample to get all the control points for
  // the tfcn. std::vector<float> controlPoints; for (size_t i = 0; i < 3; ++i)
  // {
  //   for (const auto &x : rgbaLines[i].line)
  //     controlPoints.push_back(x.x);
  // }

  // // Filter out same or within epsilon control points to get unique list
  // std::sort(controlPoints.begin(), controlPoints.end());
  // auto uniqueEnd = std::unique(controlPoints.begin(), controlPoints.end(),
  //     [](const float &a, const float &b) { return std::abs(a - b) < 0.0001;
  //     });
  // controlPoints.erase(uniqueEnd, controlPoints.end());

  // // Step along the lines and sample them
  // std::array<std::vector<vec2f>::const_iterator, 3> lit = {
  //   rgbaLines[0].line.begin(), rgbaLines[1].line.begin(),
  //   rgbaLines[2].line.begin()
  // };

  // for (const auto &x : controlPoints) {
  //   std::array<float, 3> sampleColor;
  //   for (size_t j = 0; j < 3; ++j) {
  //     if (x > (lit[j] + 1)->x)
  //       ++lit[j];

  //     assert(lit[j] != rgbaLines[j].line.end());
  //     const float t = (x - lit[j]->x) / ((lit[j] + 1)->x - lit[j]->x);
  //     // It's hard to click down at exactly 0, so offset a little bit
  //     sampleColor[j] = clamp(lerp(lit[j]->y - 0.001, (lit[j] + 1)->y - 0.001,
  //     t));
  //   }
  //   output.rgbValues.push_back(vec3f(sampleColor[0], sampleColor[1],
  //   sampleColor[2]));
  // }

  // output.save(fileName);
  ;
}
