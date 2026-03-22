//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/resources/mg_material_resource.h"

#include "mg/core/mg_runtime_error.h"
#include "mg/core/parser/mg_parser.h"
#include "mg/core/resource_cache/mg_resource_loading_input.h"
#include "mg/core/resources/mg_shader_resource.h"

#include <vector>

namespace Mg {

LoadResourceResult MaterialResource::load_resource_impl(ResourceLoadingInput& input)
{
    const std::string_view resource_definition = input.resource_data_as_text();

    try {
        parser::MaterialParseResult parse_result = parser::parse_material(resource_definition);

        m_blend_mode = parse_result.blend_mode;

        m_parameters = std::move(parse_result.parameters);
        m_samplers = std::move(parse_result.samplers);
        m_options = std::move(parse_result.options);

        m_shader_resource = input.load_dependency<ShaderResource>(parse_result.shader_resource_id);
    }
    catch (const std::exception& e) {
        return LoadResourceResult::data_error(e.what());
    }
    catch (const RuntimeError& e) {
        return LoadResourceResult::data_error(e.what());
    }

    return LoadResourceResult::success();
}

} // namespace Mg
