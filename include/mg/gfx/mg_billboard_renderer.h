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

/** @file mg_billboard_renderer.h
 * Billboard renderer.
 */

#pragma once

#include "mg/gfx/mg_texture_handle.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vector>

namespace Mg::gfx {

class ICamera;
class Material;

// TODO: implement support for different materials?
// TODO: texture atlas for mixing billboard textures or animating.

namespace BillboardSetting {
using Value = uint8_t;
enum Flags : Value {
    A_TEST          = 0x1, // discard fragment when alpha < 0.5
    FADE_WHEN_CLOSE = 0x2, // shrink and fade billboards near camera to reduce overdraw
    FIXED_SIZE = 0x4 // radius as proportion of view height (same size irrespective of distance)
};
} // namespace BillboardSetting

struct Billboard {
    glm::vec3 pos;
    glm::vec4 colour;
    float     radius;
};

/** List of billboards to render. */
class BillboardRenderList {
public:
    /** Add a new billboard to the end of the render list.
     * @return reference to new billboard.
     */
    Billboard& add()
    {
        m_billboards.push_back(Billboard{});
        return m_billboards.back();
    }

    /** Empty list of billboards. */
    void clear() { m_billboards.clear(); }

    /** Sort render list so that most distant billboard is rendered first. This is useful when using
     * alpha-blending.
     */
    void sort_farthest_first(const ICamera& camera);

    const std::vector<Billboard>& view() const { return m_billboards; }

private:
    std::vector<Billboard> m_billboards;
};

struct BillboardRendererData;

class BillboardRenderer : PImplMixin<BillboardRendererData> {
public:
    explicit BillboardRenderer();
    MG_MAKE_NON_COPYABLE(BillboardRenderer);
    MG_MAKE_NON_MOVABLE(BillboardRenderer);
    ~BillboardRenderer();

    void
    render(const ICamera& camera, const BillboardRenderList& render_list, const Material& material);

    void drop_shaders();
};

} // namespace Mg::gfx
