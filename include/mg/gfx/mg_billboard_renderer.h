//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_billboard_renderer.h
 * Billboard renderer.
 */

#pragma once

#include "mg/utils/mg_angle.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_rand.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <span>
#include <vector>

namespace Mg::gfx {

class ICamera;
class IRenderTarget;
class Material;

// TODO: implement support for different materials?
// TODO: texture atlas for mixing billboard textures or animating.
//
// Later thoughts on the above TODOs: no to supporting multiple materials, because that would
// require switching pipelines in the middle of rendering a set of billboards. If such a thing is
// required, it would make sense to that the user has to explicitly do that by rendering two
// separate render lists in two calls to BillboardRenderer::render.
//
// Multiple textures and texture atlas support can be done almost entirely in a material shader. The
// code would pick out a atlas subtexture via some parameter, possibly modulated by a pseudorandom
// function and some material parameters. However we might at least want some unique seed per
// billboard or particle, so that could be added to the billboard struct (or as a separate vertex
// data stream).

struct alignas(16) Billboard {
    glm::vec4 colour;
    glm::vec3 pos;
    float radius;
    // TODO float rotation;
};

/** Sort render list so that most distant billboard is rendered first. This is useful when using
 * alpha-blending.
 */
void sort_farthest_first(const ICamera& camera, std::span<Billboard> billboards) noexcept;

class BillboardRenderer {
public:
    BillboardRenderer();
    ~BillboardRenderer();

    MG_MAKE_NON_COPYABLE(BillboardRenderer);
    MG_MAKE_NON_MOVABLE(BillboardRenderer);

    void render(const IRenderTarget& render_target,
                const ICamera& camera,
                std::span<const Billboard> billboards,
                const Material& material);

    void drop_shaders() noexcept;

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};

enum class ParticleEmitterShape {
    Point
    // Add more shapes as needed
};

class ParticleSystem {
public:
    ParticleEmitterShape shape = ParticleEmitterShape::Point;
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    float particle_lifetime = 5.0f;
    glm::vec3 gravity = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 emission_direction = glm::vec3(0.0f, 0.0f, 1.0f);
    Angle emission_angle_range = Angle::from_degrees(20.0f);

    void emit(size_t num);

    void update(float time_step);

    std::span<const Billboard> particles() const { return m_particles; }

private:
    std::vector<Billboard> m_particles;
    std::vector<glm::vec3> m_velocities;
    std::vector<float> m_ages;
    std::vector<size_t> m_unused_indices;
    Random m_rand = Random(0xdeadbeef);
};

} // namespace Mg::gfx
