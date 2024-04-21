//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_billboard_renderer.h
 * Billboard renderer.
 */

#pragma once

#include "mg/containers/mg_flat_map.h"
#include "mg/utils/mg_angle.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_rand.h"
#include "glm/common.hpp"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <span>
#include <vector>

namespace Mg::gfx {

class ICamera;
class IRenderTarget;
class Material;

// TODO: texture atlas for mixing billboard textures or animating.
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
    float rotation;
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

struct ColourRange {
    // Optimization idea: precalculate a fixed-size look-up table.
    FlatMap<float, glm::vec4> colours;
};

inline glm::vec4 evaluate_colour(const ColourRange& range, float location)
{
    MG_ASSERT(!range.colours.empty());
    const auto it = range.colours.lower_bound(location);
    if (it == range.colours.begin()) {
        return it->second;
    }
    if (it == range.colours.end()) {
        return std::prev(it)->second;
    }
    const auto& a = *std::prev(it);
    const auto& b = *it;
    const auto x = (location - a.first) / (b.first - a.first);
    return glm::mix(a.second, b.second, x);
}

class ParticleSystem {
public:
    explicit ParticleSystem(uint32_t seed = 0xdeadbeef) : m_rand{ seed } {}

    ParticleEmitterShape shape = ParticleEmitterShape::Point;

    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);

    float particle_lifetime_mean = 1.0f;
    float particle_lifetime_stddev = 0.5f;

    float initial_speed_mean = 2.0f;
    float initial_speed_stddev = 1.0f;

    float initial_rotation_mean = 0.0f;
    float initial_rotation_stddev = float(std::numbers::pi);

    float rotation_velocity_mean = 0.0f;
    float rotation_velocity_stddev = 1.0f;

    float initial_radius_mean = 0.03f;
    float initial_radius_stddev = 0.03f;

    float final_radius_mean = 0.01f;
    float final_radius_stddev = 0.01f;

    glm::vec3 gravity = glm::vec3(0.0f, 0.0f, -2.0f);

    glm::vec3 emission_direction = glm::vec3(0.0f, 0.0f, 1.0f);

    Angle emission_angle_range = Angle::from_degrees(20.0f);

    // Range of colours over the lifetime of the particle.
    // Each particle will choose a value between zero and one and use that to interpolate a colour
    // between colour_range_a and colour_range_b.
    ColourRange colour_range_a = { {
        { 0.0f, glm::vec4{ 10.0f, 10.0f, 10.0f, 1.0f } },
        { 0.1f, glm::vec4{ 0.0f, 0.0f, 8.0f, 1.0f } },
        { 0.25f, glm::vec4{ 4.0f, 0.0f, 0.0f, 4.0f } },
        { 1.0f, glm::vec4{ 2.0f, 0.0f, 0.0f, 0.0f } },
    } };

    // Range of colours over the lifetime of the particle.
    // Each particle will choose a value between zero and one and use that to interpolate a colour
    // between colour_range_a and colour_range_b.
    ColourRange colour_range_b = { {
        { 0.0f, glm::vec4{ 10.0f, 10.0f, 10.0f, 1.0f } },
        { 0.15f, glm::vec4{ 0.0f, 8.0f, 8.0f, 1.0f } },
        { 0.4f, glm::vec4{ 4.0f, 2.0f, 0.0f, 4.0f } },
        { 1.0f, glm::vec4{ 2.0f, 0.0f, 0.0f, 0.0f } },
    } };

    void emit(size_t num);

    void update(float time_step);

    std::span<const Billboard> particles() const { return m_billboards; }

private:
    struct Particle {
        // Current velocity of this particle.
        glm::vec3 velocity{ 0.0f, 0.0f, 0.0f };

        // How long this particle has existed.
        float age = 0.0f;

        // How long this particle will live.
        float lifetime = 0.0;

        // In [0.0, 1.0]. The choice between colour_range_a and colour_range_b.
        float colour_choice = 0.0f;

        // How fast the particle rotates.
        float rotation_velocity = 0.0f;

        float initial_radius = 1.0f;
        float final_radius = 1.0f;
    };

    std::vector<Billboard> m_billboards;
    std::vector<Particle> m_particles;
    std::vector<size_t> m_unused_indices;
    Random m_rand;
};

} // namespace Mg::gfx
