#include "catch.hpp"

#define MG_CONTRACT_VIOLATION_THROWS 1

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_pipeline.h"

#include <mg/gfx/mg_pipeline_pool.h>

TEST_CASE("PipelinePoolFromGoodConfig")
{
    Mg::gfx::PipelinePoolConfig config;
    config.material_parameters_binding_location = 0;
    config.shared_input_layout = Mg::Array<Mg::gfx::PipelineInputDescriptor>::make(2);

    config.shared_input_layout[0].input_name = "Input1";
    config.shared_input_layout[0].location = 1;
    config.shared_input_layout[0].type = Mg::gfx::PipelineInputType::UniformBuffer;
    config.shared_input_layout[0].mandatory = false;

    config.shared_input_layout[1].input_name = "Input2";
    config.shared_input_layout[1].location = 8; // For texture types, 0-7 are reserved for
                                                // material samplers.
    config.shared_input_layout[1].type = Mg::gfx::PipelineInputType::Sampler2D;
    config.shared_input_layout[1].mandatory = false;

    REQUIRE_NOTHROW(Mg::gfx::validate(config));
}

TEST_CASE("PipelinePoolFromBadConfig")
{
    Mg::gfx::PipelinePoolConfig config;
    config.material_parameters_binding_location = 0;
    config.shared_input_layout = Mg::Array<Mg::gfx::PipelineInputDescriptor>::make(2);

    config.shared_input_layout[0].input_name = "Input1";
    config.shared_input_layout[0].location = 0; // overlaps material_parameters_binding_location.
    config.shared_input_layout[0].type = Mg::gfx::PipelineInputType::UniformBuffer;
    config.shared_input_layout[0].mandatory = false;

    REQUIRE_THROWS(Mg::gfx::validate(config));
}

TEST_CASE("PipelinePoolFromBadConfig2")
{
    Mg::gfx::PipelinePoolConfig config;
    config.material_parameters_binding_location = 0;
    config.shared_input_layout = Mg::Array<Mg::gfx::PipelineInputDescriptor>::make(2);

    config.shared_input_layout[0].input_name = "Input1";
    config.shared_input_layout[0].location = 1; // texture not allowed in 0-7 (reserved for material
                                                // samplers)
    config.shared_input_layout[0].type = Mg::gfx::PipelineInputType::Sampler2D;
    config.shared_input_layout[0].mandatory = false;

    REQUIRE_THROWS(Mg::gfx::validate(config));
}
