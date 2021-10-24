//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_debug_renderer.h
 * Immediate-mode renderer for drawing debug geometry.
 */

#pragma once

#include "mg/core/mg_rotation.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mg::gfx {

class Skeleton;
struct SkeletonPose;

struct DebugRendererData;

/** Renderer for drawing debug geometry.
 * This renderer is relatively inefficient and is intended for debugging visualisation.
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

    void draw_box(const glm::mat4& view_proj, BoxDrawParams params);

    void draw_ellipsoid(const glm::mat4& view_proj, EllipsoidDrawParams params);

    void draw_line(const glm::mat4& view_proj,
                   span<const glm::vec3> points,
                   const glm::vec4& colour,
                   float width = 1.0f);

    void draw_line(const glm::mat4& view_proj,
                   const glm::vec3& start,
                   const glm::vec3& end,
                   const glm::vec4& colour,
                   const float width = 1.0f)
    {
        std::array<glm::vec3, 2> points = { start, end };
        draw_line(view_proj, points, colour, width);
    }

    void draw_bones(const glm::mat4& view_proj,
                    const glm::mat4& M,
                    const Skeleton& skeleton,
                    const SkeletonPose& pose);

    void draw_view_frustum(const glm::mat4& view_projection,
                           const glm::mat4& view_projection_frustum,
                           float max_distance = 0.0f);
};

struct DebugRenderQueueData;

/** A utility for queuing up debug render commands to dispatch later at a convenient point in the
 * rendering pipeline. This makes it easier to set up debug rendering from different points in the
 * codebase.
 */
class DebugRenderQueue : PImplMixin<DebugRenderQueueData> {
public:
    DebugRenderQueue();
    ~DebugRenderQueue();

    MG_MAKE_NON_COPYABLE(DebugRenderQueue);
    MG_MAKE_NON_MOVABLE(DebugRenderQueue);

    void draw_box(DebugRenderer::BoxDrawParams params);

    void draw_ellipsoid(DebugRenderer::EllipsoidDrawParams params);

    void draw_line(span<const glm::vec3> points, const glm::vec4& colour, float width = 1.0f);

    void draw_line(const glm::vec3& start,
                   const glm::vec3& end,
                   const glm::vec4& colour,
                   const float width = 1.0f)
    {
        std::array<glm::vec3, 2> points = { start, end };
        draw_line(points, colour, width);
    }

    void dispatch(DebugRenderer& renderer, const glm::mat4& view_proj_matrix);

    void clear();
};

inline DebugRenderQueue& get_debug_render_queue()
{
    static DebugRenderQueue queue;
    return queue;
}

} // namespace Mg::gfx
