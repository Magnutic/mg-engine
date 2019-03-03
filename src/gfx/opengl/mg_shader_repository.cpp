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

#include "mg/gfx/mg_shader_repository.h"

#include "mg/containers/mg_pooling_vector.h"
#include "mg/gfx/mg_shader.h"

namespace Mg::gfx {

struct MakeShaderReturn {
    std::optional<ShaderProgram> opt_program;
    ShaderCompileResult          return_code;
};

static MakeShaderReturn make_shader_program(const ShaderCode& code)
{
    auto error_value = [](ShaderCompileResult enum_value) {
        return MakeShaderReturn{ std::nullopt, enum_value };
    };

    std::optional<VertexShader> ovs = VertexShader::make(code.vertex_code);
    if (!ovs.has_value()) { return error_value(ShaderCompileResult::VertexShaderError); }

    std::optional<FragmentShader> ofs = FragmentShader::make(code.fragment_code);
    if (!ofs.has_value()) { return error_value(ShaderCompileResult::FragmentShaderError); }

    std::optional<GeometryShader> ogs;
    std::optional<ShaderProgram>  o_program;

    if (code.geometry_code.empty()) { o_program = ShaderProgram::make(ovs.value(), ofs.value()); }
    else {
        ogs = GeometryShader::make(code.geometry_code);
        if (!ogs.has_value()) { return error_value(ShaderCompileResult::GeometryShaderError); }

        o_program = ShaderProgram::make(ovs.value(), ogs.value(), ofs.value());
    }

    if (!o_program.has_value()) { return error_value(ShaderCompileResult::LinkingError); }

    return { std::move(o_program), ShaderCompileResult::Success };
}

struct ShaderNode {
    explicit ShaderNode(ShaderProgram&& program_) : program(std::move(program_)) {}

    // Program must be first value of ShaderNode, so that ShaderHandle values (which are
    // ShaderProgram*) can be casted to ShaderNode*.
    ShaderProgram program;

    uint32_t self_index{};
};

ShaderProgram& access_shader_program(ShaderHandle handle)
{
    return handle->program;
};

static constexpr size_t k_shader_program_pool_size = 64;

class ShaderRepositoryImpl {
public:
    CreateShaderReturn create(const ShaderCode& code)
    {
        auto [o_program, return_code] = make_shader_program(code);

        if (!o_program.has_value()) { return { ShaderHandle{ 0 }, return_code }; }

        auto [index, ptr] = m_programs.construct(std::move(o_program.value()));
        ptr->self_index   = index;

        return { ptr, return_code };
    }

    void destroy(ShaderHandle handle) { m_programs.destroy(handle->self_index); }

private:
    PoolingVector<ShaderNode> m_programs{ k_shader_program_pool_size };
};

ShaderRepository::ShaderRepository()  = default;
ShaderRepository::~ShaderRepository() = default;

CreateShaderReturn ShaderRepository::create(const ShaderCode& code)
{
    return data().create(code);
}

void ShaderRepository::destroy(ShaderHandle handle)
{
    data().destroy(handle);
}

} // namespace Mg::gfx
