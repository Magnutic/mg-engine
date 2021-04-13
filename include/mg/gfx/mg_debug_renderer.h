//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_debug_renderer.h
 * Immediate-mode renderer for drawing debug geometry.
 */

#pragma once

#include "mg/gfx/mg_camera.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mg {
class Rotation;
} // namespace Mg

namespace Mg::gfx {

class Skeleton;
struct SkeletonPose;

struct DebugRendererData;

/** Renderer for drawing debug geometry.
 * N.B. this renderer is relatively inefficient and is intended for debugging visualisation.
 */
class DebugRenderer : PImplMixin<DebugRendererData> {
public:
    DebugRenderer();
    ~DebugRenderer();

    MG_MAKE_NON_COPYABLE(DebugRenderer);
    MG_MAKE_NON_MOVABLE(DebugRenderer);

    struct PrimitiveDrawParams {
        glm::vec3 centre{ 0.0f };
        glm::vec3 dimensions{ 1.0f };
        Rotation orientation;
        glm::vec4 colour{ 1.0f };
        bool wireframe = false;
    };

    struct BoxDrawParams : PrimitiveDrawParams {};

    struct EllipsoidDrawParams : PrimitiveDrawParams {
        size_t steps = 24;
    };

    void draw_box(const ICamera& camera, BoxDrawParams params);

    void draw_ellipsoid(const ICamera& camera, EllipsoidDrawParams params);

    void draw_line(const ICamera& camera,
                   span<const glm::vec3> points,
                   const glm::vec4 colour,
                   const float width = 1.0f);

    void draw_line(const ICamera& camera,
                   const glm::vec3 start,
                   const glm::vec3 end,
                   const glm::vec4 colour,
                   const float width = 1.0f)
    {
        std::array<glm::vec3, 2> points = { start, end };
        draw_line(camera, points, colour, width);
    }

    void draw_bones(const ICamera& camera,
                    const glm::mat4 M,
                    const Skeleton& skeleton,
                    const SkeletonPose& pose);
};

} // namespace Mg::gfx
