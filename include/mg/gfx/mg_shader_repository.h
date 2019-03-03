//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_shader_repository.h
 * Creates, stores, and updates shader programs.
 */

#pragma once

#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <string>

namespace Mg::gfx {

class ShaderProgram;
struct ShaderNode;

using ShaderHandle = ShaderNode*;

ShaderProgram& access_shader_program(ShaderHandle handle);

struct ShaderCode {
    std::string vertex_code;
    std::string fragment_code;
    std::string geometry_code; // May be empty.
};

enum class ShaderCompileResult {
    Success,
    VertexShaderError,
    FragmentShaderError,
    GeometryShaderError,
    LinkingError
};

struct CreateShaderReturn {
    ShaderHandle        handle;
    ShaderCompileResult return_code;
};

class ShaderRepositoryImpl;

class ShaderRepository : PimplMixin<ShaderRepositoryImpl> {
public:
    explicit ShaderRepository();
    ~ShaderRepository();

    MG_MAKE_NON_COPYABLE(ShaderRepository);
    MG_MAKE_DEFAULT_MOVABLE(ShaderRepository);

    CreateShaderReturn create(const ShaderCode& code);
    void               destroy(ShaderHandle handle);
};

} // namespace Mg::gfx
