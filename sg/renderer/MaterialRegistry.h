// ======================================================================== //
// Copyright 2020 Intel Corporation                                    //
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
#include <vector>
#include "../Node.h"
#include "../visitors/GenerateOSPRayMaterials.h"

namespace ospray::sg {

  struct MaterialRegistry : public Node
  {
    MaterialRegistry();

    ~MaterialRegistry() override = default;

    void addNewSGMaterial(std::string matType);

    void updateMaterialRegistry(const std::string &rType);

    void updateMaterialList(const std::string &rType);

    void refreshMaterialList(const std::string &matType, const std::string &rType);

    void rmMatImports();

    std::vector<std::string> matImportsList;

    std::vector<cpp::Material> cppMaterialList;

    std::vector<std::shared_ptr<sg::Material>> sgMaterialList;

  };

}  // namespace ospray::sg
