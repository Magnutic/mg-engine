//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_pipeline_pool.h"

#include "../mg_shader.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_log.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_pipeline.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/mg_defs.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_iteration_utils.h"
#include "mg/utils/mg_optional.h"
#include "mg/utils/mg_stl_helpers.h"

#include <fmt/core.h>

namespace Mg::gfx {

//--------------------------------------------------------------------------------------------------
// Shader code assembly and compilation
//--------------------------------------------------------------------------------------------------

namespace {

enum class ShaderErrorFlags : uint32_t {
    VertexShader = 0x1,
    FragmentShader = 0x1 << 1,
    GeometryShader = 0x1 << 2
};

MG_DEFINE_BITMASK_OPERATORS(ShaderErrorFlags)

// Dump code to log with line numbers
std::string error_dump_code(std::string_view code)
{
    std::string str;
    str.reserve(1024);
    size_t line = 1;

    str += std::to_string(line) + '\t';
    for (const char c : code) {
        str += c;
        if (c == '\n') {
            ++line;
            str += std::to_string(line) + '\t';
        }
    }

    return str;
}

inline ShaderCode append_shader_code(const ShaderCode& first, const ShaderCode& second)
{
    ShaderCode code = first;
    code.vertex.code += second.vertex.code;
    code.geometry.code += second.geometry.code;
    code.fragment.code += second.fragment.code;

    return code;
}

std::string shader_input_layout_code(const Material& material)
{
    std::string snippet;
    snippet.reserve(256);

    // Include definition of each parameter.
    if (!material.parameters().empty()) {
        snippet += "layout (std140) uniform MaterialParams {\n";

        for (const Material::Parameter& p : material.parameters()) {
            snippet += '\t';
            snippet += shader::parameter_type_to_string(p.type);
            snippet += " ";
            snippet += p.name.str_view();
            snippet += ";\n";
        }

        snippet += "} material_params;\n\n";
    }

    // Include definition of each sampler.
    for (const Material::Sampler& s : material.samplers()) {
        snippet += "uniform ";
        snippet += shader::sampler_type_to_string(s.type);
        snippet += " ";
        snippet += s.name.str_view();
        snippet += ";\n";
    }

    // Include pre-processor #defines for each option.
    for (const Material::Option& o : material.options()) {
        snippet += fmt::format("#define {} {:d}\n", o.c_str(), material.get_option(o));
    }

    snippet += '\n';
    return snippet;
}

struct ShaderCompileResult {
    Opt<VertexShaderHandle> vs_handle;
    Opt<GeometryShaderHandle> gs_handle;
    Opt<FragmentShaderHandle> fs_handle;
    ShaderErrorFlags error_flags = {};
};

// Write details on shader compilation error to log.
void log_shader_error(const ShaderCode& code, ShaderErrorFlags error_flags)
{
    if ((error_flags & ShaderErrorFlags::VertexShader) != 0) {
        log.error("Error compiling vertex shader");
        log.message(error_dump_code(code.vertex.code));
    }

    if ((error_flags & ShaderErrorFlags::GeometryShader) != 0) {
        log.error("Error compiling geometry shader");
        log.message(error_dump_code(code.geometry.code));
    }

    if ((error_flags & ShaderErrorFlags::FragmentShader) != 0) {
        log.error("Error compiling fragment shader");
        log.message(error_dump_code(code.fragment.code));
    }
}

// Write details on shader linking error to log.
void log_shader_link_error(std::string_view shader_name, const ShaderCode& shader_code)
{
    log.error("Error linking shaders for program {}.", shader_name);
    log.verbose("Vertex code:\n{}", error_dump_code(shader_code.vertex.code));
    log.verbose("Geometry code:\n{}", error_dump_code(shader_code.geometry.code));
    log.verbose("Fragment code:\n{}", error_dump_code(shader_code.fragment.code));
}

ShaderCompileResult compile_shader(const ShaderCode& code)
{
    MG_GFX_DEBUG_GROUP("compile_shader")
    ShaderCompileResult result;

    result.vs_handle = compile_vertex_shader(code.vertex.code);
    if (!result.vs_handle) {
        result.error_flags |= ShaderErrorFlags::VertexShader;
    }

    if (!code.geometry.code.empty()) {
        result.gs_handle = compile_geometry_shader(code.geometry.code);
        if (!result.gs_handle) {
            result.error_flags |= ShaderErrorFlags::GeometryShader;
        }
    }

    if (!code.fragment.code.empty()) {
        result.fs_handle = compile_fragment_shader(code.fragment.code);
        if (!result.fs_handle) {
            result.error_flags |= ShaderErrorFlags::FragmentShader;
        }
    }

    log_shader_error(code, result.error_flags);

    return result;
}

ShaderCode assemble_shader_code(const ShaderCode& preamble_shader_code, const Material& material)
{
    ShaderCode code = preamble_shader_code;

    // Include sampler, parameter, and enabled-option definitions
    code.vertex.code += shader_input_layout_code(material);
    code.fragment.code += shader_input_layout_code(material);

    // Access shader resource
    {
        ResourceAccessGuard shader_resource_access(material.shader());

        // If there is a vertex-preprocess function, then include the corresponding #define
        if ((shader_resource_access->tags() & shader::Tag::DEFINES_VERTEX_PREPROCESS) != 0) {
            code.vertex.code += "#define VERTEX_PREPROCESS_ENABLED 1\n";
        }

        code.vertex.code += shader_resource_access->vertex_code();
        code.fragment.code += shader_resource_access->fragment_code();
    }

    return code;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// PipelinePool implementation
//--------------------------------------------------------------------------------------------------

struct PipelinePool::Impl {
    using PipelineMap = FlatMap<Material::PipelineId, Pipeline, MaterialPipelineIdCmp>;

    PipelinePoolConfig config;
    PipelineMap pipelines;
    UniformBuffer material_params_ubo{ defs::k_material_parameters_buffer_size };
};

namespace {

Opt<Pipeline> make_pipeline(const ShaderCompileResult& compiled_shader,
                            span<const PipelineInputDescriptor> shared_input_layout,
                            span<const PipelineInputDescriptor> material_input_layout)
{
    Pipeline::Params params = {};
    params.vertex_shader = compiled_shader.vs_handle.value();
    params.fragment_shader = compiled_shader.fs_handle.value();
    params.geometry_shader = compiled_shader.gs_handle;
    params.shared_input_layout = shared_input_layout;
    params.material_input_layout = material_input_layout;

    return Pipeline::make(params);
}

// Create a PipelineInputLayout corresponding to the given material.
Array<PipelineInputDescriptor>
generate_material_input_layout(const Material& material, const uint32_t material_params_ubo_slot)
{
    const auto& samplers = material.samplers();

    auto descriptors = Array<PipelineInputDescriptor>::make(samplers.size() + 1);

    for (size_t i = 0; i < samplers.size(); ++i) {
        descriptors[i].input_name = samplers[i].name;
        descriptors[i].type = PipelineInputType::Sampler2D;
        descriptors[i].location = as<uint32_t>(i);
        descriptors[i].mandatory = false;
    }

    PipelineInputDescriptor& material_params_descriptor = descriptors.back();
    material_params_descriptor.input_name = "MaterialParams";
    material_params_descriptor.type = PipelineInputType::UniformBuffer;
    material_params_descriptor.location = material_params_ubo_slot;
    material_params_descriptor.mandatory = false;

    return descriptors;
}

// Make Pipeline to use as fallback when shaders fail to compile.
Pipeline make_fallback_pipeline(const PipelinePoolConfig& config, const Material& material)
{
    log.message("Using error-fallback shader.");

    const ShaderCode fallback_shader_code = append_shader_code(config.preamble_shader_code,
                                                               config.on_error_shader_code);
    const ShaderCompileResult compile_result = compile_shader(fallback_shader_code);

    MG_ASSERT(compile_result.error_flags == 0);

    auto material_input_layout = generate_material_input_layout(material,
                                                                config.material_params_ubo_slot);
    Opt<Pipeline> pipeline =
        make_pipeline(compile_result, config.shared_input_layout, material_input_layout);

    return std::move(pipeline.value());
}

Pipeline make_pipeline_for_material(const PipelinePoolConfig& config, const Material& material)
{
    MG_GFX_DEBUG_GROUP("make_pipeline_for_material")

    const std::string_view shader_name = material.shader().resource_id().str_view();

    log.message("Compiling permutation of shader '{}'.", shader_name);

    // Assemble and compile shader code for this particular material.
    const ShaderCode shader_code = assemble_shader_code(config.preamble_shader_code, material);
    const ShaderCompileResult compile_result = compile_shader(shader_code);

    if (compile_result.error_flags != 0) {
        return make_fallback_pipeline(config, material);
    }

    auto material_input_layout = generate_material_input_layout(material,
                                                                config.material_params_ubo_slot);
    Opt<Pipeline> opt_pipeline =
        make_pipeline(compile_result, config.shared_input_layout, material_input_layout);

    if (!opt_pipeline) {
        log_shader_link_error(shader_name, shader_code);
        return make_fallback_pipeline(config, material);
    }

    return std::move(opt_pipeline.value());
}

Pipeline& get_or_make_pipeline(PipelinePool::Impl& data, const Material& material)
{
    const Material::PipelineId key = material.pipeline_identifier();

    auto it = data.pipelines.find(key);
    const bool was_found = (it != data.pipelines.end());
    if (was_found) {
        return it->second;
    }

    // Not found, make pipeline.
    Pipeline pipeline = make_pipeline_for_material(data.config, material);
    it = data.pipelines.insert({ key, std::move(pipeline) }).first;
    return it->second;
}

} // anonymous namespace

PipelinePool::PipelinePool(PipelinePoolConfig&& config)
{
    m_impl->config = std::move(config);

    for (auto& input_location : m_impl->config.shared_input_layout) {
        switch (input_location.type) {
        case PipelineInputType::BufferTexture:
            [[fallthrough]];

        case PipelineInputType::Sampler2D:
            MG_ASSERT(input_location.location >= 8 &&
                      "Texture slots [0,7] are reserved for material samplers.");

        default:
            break;
        }
    }
}

void PipelinePool::bind_material_pipeline(const Material& material,
                                          const Pipeline::Settings& settings,
                                          PipelineBindingContext& binding_context)
{
    MG_GFX_DEBUG_GROUP("PipelinePool::bind_material_pipeline")

    const Pipeline& pipeline = get_or_make_pipeline(*m_impl, material);
    binding_context.bind_pipeline(pipeline, settings);

    // Upload material parameter values to MaterialParams uniform buffer.
    m_impl->material_params_ubo.set_data(material.material_params_buffer());

    // Set up input bindings for material parameters; one for the MaterialParams uniform buffer and
    // the material's up-to-eight samplers.
    small_vector<PipelineInputBinding, 9> material_input_bindings;

    material_input_bindings.push_back(
        { m_impl->config.material_params_ubo_slot, m_impl->material_params_ubo });

    for (const auto& [i, sampler] : enumerate<uint32_t>(material.samplers())) {
        material_input_bindings.push_back({ i, sampler.texture });
    }

    Pipeline::bind_material_inputs(material_input_bindings);
}

void PipelinePool::prepare_material_pipeline(const Material& material)
{
    MG_GFX_DEBUG_GROUP("PipelinePool::prepare_material_pipeline")
    get_or_make_pipeline(*m_impl, material);
}

void PipelinePool::drop_pipelines() noexcept
{
    m_impl->pipelines.clear();
}

void PipelinePool::drop_pipeline(const Material& material) noexcept
{
    const auto it = m_impl->pipelines.find(material.pipeline_identifier());
    if (it != m_impl->pipelines.end()) {
        m_impl->pipelines.erase(it);
    }
}

} // namespace Mg::gfx
