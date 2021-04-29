//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_billboard_renderer.h
 * Billboard renderer.
 */

#pragma once

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
    A_TEST = 0x1,          // discard fragment when alpha < 0.5
    FADE_WHEN_CLOSE = 0x2, // shrink and fade billboards near camera to reduce overdraw
    FIXED_SIZE = 0x4 // radius as proportion of view height (same size irrespective of distance)
};
} // namespace BillboardSetting

struct Billboard {
    glm::vec3 pos;
    glm::vec4 colour;
    float radius;
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
    void clear() noexcept { m_billboards.clear(); }

    /** Sort render list so that most distant billboard is rendered first. This is useful when using
     * alpha-blending.
     */
    void sort_farthest_first(const ICamera& camera) noexcept;

    const std::vector<Billboard>& view() const noexcept { return m_billboards; }

private:
    std::vector<Billboard> m_billboards;
};

struct BillboardRendererData;

class BillboardRenderer : PImplMixin<BillboardRendererData> {
public:
    BillboardRenderer();
    ~BillboardRenderer();

    MG_MAKE_NON_COPYABLE(BillboardRenderer);
    MG_MAKE_NON_MOVABLE(BillboardRenderer);

    void
    render(const ICamera& camera, const BillboardRenderList& render_list, const Material& material);

    void drop_shaders() noexcept;
};

} // namespace Mg::gfx
