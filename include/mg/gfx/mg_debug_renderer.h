//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
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

/** @file mg_debug_renderer.h
 * Immediate-mode renderer for drawing debug geometry.
 */

#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <memory>

#include "mg/gfx/mg_camera.h"
#include "mg/utils/mg_macros.h"

namespace Mg {
class Rotation;
}

namespace Mg::gfx {

/** Renderer for drawing debug geometry.
 * N.B. this renderer is relatively inefficient and is intended for debugging visualisation.
 */
class DebugRenderer {
public:
    DebugRenderer();
    MG_MAKE_NON_COPYABLE(DebugRenderer);
    MG_MAKE_NON_MOVABLE(DebugRenderer);
    ~DebugRenderer();

    struct PrimitiveDrawParams {
        glm::vec3 centre;
        glm::vec3 dimensions{ 1.0f };
        Rotation  orientation;
        glm::vec4 colour{ 1.0f };
        bool      wireframe = false;
    };

    struct BoxDrawParams : PrimitiveDrawParams {};

    struct EllipsoidDrawParams : PrimitiveDrawParams {
        size_t steps = 24;
    };

    void draw_box(const ICamera& camera, BoxDrawParams params);

    void draw_ellipsoid(const ICamera& camera, EllipsoidDrawParams params);

private:
    struct Data;
    Data& data() const;

    std::unique_ptr<Data> m_data;
};

} // namespace Mg::gfx
