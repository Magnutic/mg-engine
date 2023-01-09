//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2023, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_blend_modes.h"
#include "mg/utils/mg_enum.h"

#include <hjson/hjson.h>

namespace Mg::gfx {

Hjson::Value BlendMode::serialize(const BlendMode& blend_mode)
{
    auto to_string = [](auto enum_value) { return std::string(enum_utils::to_string(enum_value)); };

    Hjson::Value v;
    v["BlendMode"]["colour_blend_op"] = to_string(blend_mode.colour_blend_op);
    v["BlendMode"]["alpha_blend_op"] = to_string(blend_mode.alpha_blend_op);
    v["BlendMode"]["src_colour_factor"] = to_string(blend_mode.src_colour_factor);
    v["BlendMode"]["dst_colour_factor"] = to_string(blend_mode.dst_colour_factor);
    v["BlendMode"]["src_alpha_factor"] = to_string(blend_mode.src_alpha_factor);
    v["BlendMode"]["dst_alpha_factor"] = to_string(blend_mode.dst_alpha_factor);

    return v;
}

template<typename EnumT> EnumT to(const Hjson::Value& from)
{
    return enum_utils::from_string<EnumT>(from.to_string()).value();
}

BlendMode BlendMode::deserialize(const Hjson::Value& v)
{
    BlendMode bm{};

    bm.colour_blend_op = to<BlendOp>(v["BlendMode"]["colour_blend_op"]);
    bm.alpha_blend_op = to<BlendOp>(v["BlendMode"]["alpha_blend_op"]);
    bm.src_colour_factor = to<BlendFactor>(v["BlendMode"]["src_colour_factor"]);
    bm.dst_colour_factor = to<BlendFactor>(v["BlendMode"]["dst_colour_factor"]);
    bm.src_alpha_factor = to<BlendFactor>(v["BlendMode"]["src_alpha_factor"]);
    bm.dst_alpha_factor = to<BlendFactor>(v["BlendMode"]["dst_alpha_factor"]);

    return bm;
}

} // namespace Mg::gfx
