//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_physics_world.h
 * Collision detection and rigid-body physics world.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/core/physics/mg_collision.h"
#include "mg/core/physics/mg_dynamic_body_handle.h"
#include "mg/core/physics/mg_ghost_object_handle.h"
#include "mg/core/physics/mg_ray_hit.h"
#include "mg/core/physics/mg_shape.h"
#include "mg/core/physics/mg_static_body_handle.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace Mg::gfx {
class DebugRenderQueue;
} // namespace Mg::gfx

namespace Mg::gfx::mesh_data {
struct MeshDataView;
struct Vertex;
} // namespace Mg::gfx::mesh_data

/** Collision detection and rigid-body physics. */
namespace Mg::physics {

/** World containing physics bodies for collision detection and rigid-body physics. */
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    MG_MAKE_NON_COPYABLE(PhysicsWorld);
    MG_MAKE_DEFAULT_MOVABLE(PhysicsWorld);

    //----------------------------------------------------------------------------------------------
    // Shape constructors

    Shape* create_box_shape(const glm::vec3& extents);

    /** The total height is height + 2 * radius; `height` is the height between the center of
     * each 'sphere' of the capsule caps.
     * */
    Shape* create_capsule_shape(float radius, float height);

    Shape* create_cylinder_shape(const glm::vec3& extents);

    Shape* create_sphere_shape(float radius);

    Shape* create_cone_shape(float radius, float height);

    Shape* create_mesh_shape(const gfx::mesh_data::MeshDataView& mesh_data);

    Shape* create_convex_hull(std::span<const gfx::mesh_data::Vertex> vertices,
                              const glm::vec3& centre_of_mass,
                              const glm::vec3& scale);

    Shape* create_compound_shape(std::span<Shape* const> parts,
                                 std::span<const glm::mat4> part_transforms);

    //----------------------------------------------------------------------------------------------
    // Body constructors

    StaticBodyHandle
    create_static_body(const Identifier& id, Shape& shape, const glm::mat4& transform);

    DynamicBodyHandle create_dynamic_body(const Identifier& id,
                                          Shape& shape,
                                          const DynamicBodyParameters& parameters,
                                          const glm::mat4& transform);

    GhostObjectHandle
    create_ghost_object(const Identifier& id, Shape& shape, const glm::mat4& transform);

    //----------------------------------------------------------------------------------------------
    // World settings

    void gravity(const glm::vec3& gravity);

    glm::vec3 gravity() const;

    //----------------------------------------------------------------------------------------------
    // Update

    void update(float time_step);

    void interpolate(float factor);

    //----------------------------------------------------------------------------------------------
    // Collision detection

    /** Get all collisions that occurred in the last update. The returned std::span is valid until
     * next `update()`.
     */
    std::span<const Collision> collisions() const;

    /** Get all collisions involving the object with the given id that occurred in the last update.
     * Pointers are valid until next update.
     */
    void find_collisions_for(Identifier id, std::vector<const Collision*>& out) const;

    /** Get an up-to-date set of collisions involving the given GhostObject. This is
     * only needed when something has changed since last `Mg::physics::World::update()`, such as if
     * this GhostObject or some other object in the scene has moved, and you need to get the
     * resulting collisions without waiting for the next World update. Otherwise, it is more
     * efficient to get the collisions using `GhostObjectHandle::collisions()`.
     */
    void calculate_collisions_for(const GhostObjectHandle& ghost_object_handle,
                                  std::vector<Collision>& out);

    size_t raycast(const glm::vec3& start,
                   const glm::vec3& end,
                   CollisionGroup filter_mask,
                   std::vector<RayHit>& out);

    size_t convex_sweep(Shape& shape,
                        const glm::vec3& start,
                        const glm::vec3& end,
                        CollisionGroup filter_mask,
                        std::vector<RayHit>& out);

    //----------------------------------------------------------------------------------------------
    // Miscellaneous

    /** Add debug visualizations to the debug render queue, visualising the collision shapes as
     * the physics simulation sees it.
     */
    void draw_debug(gfx::DebugRenderQueue& render_queue);

    struct Impl;

private:
    /** Clean up data structures, removing unused objects, bodies, and shapes. */
    void collect_garbage();

    ImplPtr<Impl> m_impl;
};

} // namespace Mg::physics
