//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_physics.h
 * Collision detection and rigid-body physics.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace Mg::gfx {
class DebugRenderer;
class ICamera;
class IRenderTarget;
} // namespace Mg::gfx

namespace Mg::gfx::Mesh {
struct MeshDataView;
struct Vertex;
} // namespace Mg::gfx::Mesh

/** Collision detection and rigid-body physics. */
namespace Mg::physics {

/** Pre-defined set of collision filter groups. Note: these correspond to the Bullet library's
 * pre-defined filter groups. Using the same ones in this API reduces risk of collisions.
 */
enum CollisionGroup : uint32_t {
    None = 0u,
    Default = 1u,
    Static = 2u,
    Kinematic = 4u,
    Debris = 8u,
    Sensor = 16u,
    Character = 32u,
    All = ~0u
};
MG_DEFINE_BITMASK_OPERATORS(CollisionGroup);

/** Interface for all collision shapes. Collision shapes are used to give a PhysicsBody objects a
 * shape. A Shape can be used in multiple PhysicsBody objects, and it is recommended to re-use Shape
 * objects whenever possible.
 *
 * Shape objects can be constructed using the member functions on `Mg::physics::World`. Their
 * lifetime is automatically managed; do not delete them.
 */
class Shape {
protected:
    Shape();

public:
    virtual ~Shape();

    MG_MAKE_NON_MOVABLE(Shape);
    MG_MAKE_NON_COPYABLE(Shape);

    /** Supported types of collision shapes. */
    enum class Type {
        Box,
        Capsule,
        Cylinder,
        Sphere,
        Cone,
        ConvexHull,
        Mesh,
        Compound,
        NumEnumValues_
    };

    /** Get what type of collision shape this object is.  */
    virtual Type type() const = 0;

    bool is_convex() const
    {
        const auto t = type();
        return t != Type::Mesh && t != Type::Compound;
    }
};

/** Internal data for physics bodies. */
class PhysicsBody;

/** Internal data for static physics bodies. */
class StaticBody;

/** Internal data for dynamic physics bodies. */
class DynamicBody;

/** Internal data for ghost objects. */
class GhostObject;

enum class PhysicsBodyType { static_body, dynamic_body, ghost_object };

// Defined below.
class DynamicBodyHandle;
class StaticBodyHandle;
class GhostObjectHandle;


class PhysicsBodyHandle {
public:
    PhysicsBodyHandle() = default;

    // Constructor is called by Mg::physics::World.
    explicit PhysicsBodyHandle(PhysicsBody* data);

    ~PhysicsBodyHandle();

    PhysicsBodyHandle(const PhysicsBodyHandle& rhs);

    PhysicsBodyHandle& operator=(const PhysicsBodyHandle& rhs);

    PhysicsBodyHandle(PhysicsBodyHandle&& rhs) noexcept : m_data(std::exchange(rhs.m_data, nullptr))
    {}

    PhysicsBodyHandle& operator=(PhysicsBodyHandle&& rhs) noexcept
    {
        PhysicsBodyHandle tmp{ std::move(rhs) };
        swap(tmp);
        return *this;
    }

    void swap(PhysicsBodyHandle& rhs) noexcept { std::swap(m_data, rhs.m_data); }

    Identifier id() const;

    PhysicsBodyType type() const;

    void has_contact_response(bool enable);
    bool has_contact_response() const;

    glm::mat4 get_transform() const;

    glm::vec3 get_position() const { return get_transform() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); }

    void set_filter_group(CollisionGroup group);
    CollisionGroup get_filter_group() const;

    void set_filter_mask(CollisionGroup mask);
    CollisionGroup get_filter_mask() const;

    Shape& shape();
    const Shape& shape() const;

    Opt<DynamicBodyHandle> as_dynamic_body() const;
    Opt<StaticBodyHandle> as_static_body() const;
    Opt<GhostObjectHandle> as_ghost_body() const;

    bool is_null() const { return m_data == nullptr; }

    operator bool() const { return is_null(); }

    friend bool operator==(const PhysicsBodyHandle& l, const PhysicsBodyHandle& r)
    {
        return l.m_data == r.m_data;
    }
    friend bool operator!=(const PhysicsBodyHandle& l, const PhysicsBodyHandle& r)
    {
        return l.m_data != r.m_data;
    }

protected:
    PhysicsBody* m_data = nullptr;

    // The derived handle classes have to be friends to be able to access m_data from within static
    // member functions.
    friend class DynamicBodyHandle;
    friend class StaticBodyHandle;
    friend class GhostObjectHandle;
    friend class World;
};

/** Handle to a DynamicBody. Lifetime of the referee is automatically managed via reference
 * counting.
 */
class DynamicBodyHandle : public PhysicsBodyHandle {
public:
    DynamicBodyHandle() = default;

    static DynamicBodyHandle downcast(const PhysicsBodyHandle& handle)
    {
        MG_ASSERT(handle.type() == PhysicsBodyType::dynamic_body);
        return DynamicBodyHandle(handle.m_data);
    }

    glm::mat4 interpolated_transform() const;

    void set_transform(const glm::mat4& transform);

    void set_gravity(const glm::vec3& gravity);
    glm::vec3 get_gravity() const;

    //----------------------------------------------------------------------------------------------
    // Physical manipulations
    // TODO double-check that all units are correct.

    /** Apply force (in Newtons) to the DynamicBody, at the given relative position (relative to
     * centre of mass).
     */
    void apply_force(const glm::vec3& force, const glm::vec3& relative_position);

    /** Apply force (in Newtons) to the DynamicBody's centre of mass.
     * Equivalent to `apply_force(force, {0.0f, 0.0f, 0.0f})`.
     */
    void apply_central_force(const glm::vec3& force);

    /** Apply impulse (in Newton-seconds) to the DynamicBody, at the given relative position
     * (relative to centre of mass).
     */
    void apply_impulse(const glm::vec3& impulse, const glm::vec3& relative_position);

    /** Apply impulse (in Newton-seconds) to the DynamicBody's centre of mass.
     * Equivalent to `apply_impulse(impulse, {0.0f, 0.0f, 0.0f})`.
     */
    void apply_central_impulse(const glm::vec3& impulse);

    /** Apply torque (in Newton-metres) to he DynamicBody. */
    void apply_torque(const glm::vec3& torque);

    /** Apply torque impulse (in Newton-metre-seconds) to the DynamicBody. */
    void apply_torque_impulse(const glm::vec3& torque);

    void velocity(const glm::vec3& velocity);

    void angular_velocity(const glm::vec3& angular_velocity);

    void move(const glm::vec3& offset);

    /** Clear all forces and torques acting on this DynamicBody. */
    void clear_forces();

    //----------------------------------------------------------------------------------------------
    // State getters

    float mass() const;

    /** Get the linear velocity (in metres/second) for this DynamicBody. */
    glm::vec3 velocity() const;

    /** Get the angular velocity (in radians/second) for this DynamicBody. */
    glm::vec3 angular_velocity() const;

    /** Get the total force (in Newtons) acting on this DynamicBody. */
    glm::vec3 total_force() const;

    /** Get the total torque (in Newton-metres) acting on this DynamicBody. */
    glm::vec3 total_torque() const;

private:
    explicit DynamicBodyHandle(PhysicsBody* data) : PhysicsBodyHandle(data) {}

    DynamicBody& data();
    const DynamicBody& data() const;
};

/** DynamicBody objects may be Dynamic, Static, Kinematic */
enum class DynamicBodyType {
    /** Dynamic bodies are affected by gravity and collisions with other bodies. */
    Dynamic,

    /** Kinematic bodies are like static ones, but may be manually moved outside of the
     * simulation. Dynamic objects will be affected by collisions with kinematic ones, but not vice
     * versa.
     */
    Kinematic
};

/** Construction parameters for dynamic bodies. */
struct DynamicBodyParameters {
    /** What type of dynamic body it is. */
    DynamicBodyType type;

    /** The body's mass (kg). Only meaningful if `type == DynamicBodyType::Dynamic`. */
    float mass = 0.0f;

    /** Whether to enable continuous collision detection for this body. */
    bool continuous_collision_detection = false;

    /** The speed of a body, in a single physics update, must exceed the bodies radius multiplied by
     * this factor for continuous collision detection to be applied.
     */
    float continuous_collision_detection_motion_threshold = 0.0f;

    /** How much the body resists translation. */
    float linear_damping = 0.01f;

    /** How much the body resists rotation. */
    float angular_damping = 0.0f;

    /** Surface (sliding) friction. */
    float friction = 0.5f;

    /** Prevents round shapes like spheres, cylinders, and capsules from rolling forever. */
    float rolling_friction = 0.0f;

    /** Torsional friction around contact normal. */
    float spinning_friction = 0.0f;

    /** Angular factor in [0.0f, 1.0f] restricts rotations per axis. */
    glm::vec3 angular_factor = glm::vec3(1.0f, 1.0f, 1.0f);
};

struct Collision {
    /** Handle of the first object involved in the collision. */
    PhysicsBodyHandle object_a;

    /** Handle of the second object involved in the collision. */
    PhysicsBodyHandle object_b;

    /** Contact point, in world space, on the first object. */
    glm::vec3 contact_point_on_a = glm::vec3(0.0f);

    /** Contact point, in world space, on the second object. */
    glm::vec3 contact_point_on_b = glm::vec3(0.0f);

    glm::vec3 normal_on_b = glm::vec3(0.0f);

    /** The impulse that was applied as a result of the collision. */
    float applied_impulse = 0.0f;

    /** The distance between the contact points. TODO: understand and document this better. To my
     * understanding, it is signed, with negative distance meaning that the bodies penetrate each
     * other.
     */
    float distance = 0.0f;
};

/** Handle to a StaticBody. Lifetime of the referee is automatically managed via reference
 * counting.
 */
class StaticBodyHandle : public PhysicsBodyHandle {
public:
    StaticBodyHandle() = default;

    static StaticBodyHandle downcast(const PhysicsBodyHandle& handle)
    {
        MG_ASSERT(handle.type() == PhysicsBodyType::static_body);
        return StaticBodyHandle(handle.m_data);
    }

private:
    explicit StaticBodyHandle(PhysicsBody* data) : PhysicsBodyHandle(data) {}

    StaticBody& data();
    const StaticBody& data() const;
};

/** Handle to a GhostObject. Lifetime of the referee is automatically managed via reference
 * counting.
 */
class GhostObjectHandle : public PhysicsBodyHandle {
public:
    GhostObjectHandle() = default;

    static GhostObjectHandle downcast(const PhysicsBodyHandle& handle)
    {
        MG_ASSERT(handle.type() == PhysicsBodyType::ghost_object);
        return GhostObjectHandle(handle.m_data);
    }

    void set_transform(const glm::mat4& transform);

    void set_position(const glm::vec3& position)
    {
        auto temp = get_transform();
        temp[3] = glm::vec4(position, 1.0f);
        set_transform(temp);
    }

    /** Get all collisions involving this object during the most recent update.
     * Pointers remain valid until the next call to Mg::physics::World::update().
     */
    std::span<const Collision* const> collisions() const;

private:
    explicit GhostObjectHandle(PhysicsBody* data) : PhysicsBodyHandle(data) {}

    GhostObject& data();
    const GhostObject& data() const;
};

/** Result of raycast or convex sweep. */
struct RayHit {
    /** The body that was hit by the ray or sweep. */
    PhysicsBodyHandle body;

    /** Position in world space at which the ray hit the body. */
    glm::vec3 hit_point_worldspace = { 0.0f, 0.0f, 0.0f };

    /** Normal vector in world space on the body where the ray hit. */
    glm::vec3 hit_normal_worldspace = { 0.0f, 0.0f, 0.0f };

    /** Fraction of distance between ray start and end where the ray hit `body`. */
    float hit_fraction = 0.0f;
};

class World {
public:
    World();
    ~World();

    MG_MAKE_NON_COPYABLE(World);
    MG_MAKE_DEFAULT_MOVABLE(World);

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

    // TODO: scaled instanced mesh shapes
    // IDEA: merge all world meshes into a big mesh, possibly split into tiles, and use it for
    // physics.
    Shape* create_mesh_shape(const gfx::Mesh::MeshDataView& mesh_data);

    // TODO: generate convex hull in mesh converter.
    Shape* create_convex_hull(std::span<const gfx::Mesh::Vertex> vertices,
                              const glm::vec3& centre_of_mass,
                              const glm::vec3& scale);

    Shape* create_compound_shape(std::span<Shape*> parts,
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

    /** Use the provided debug renderer and camera to draw debug geometry, visualising the collision
     * shapes as the physics simulation sees it.
     */
    void draw_debug(const gfx::IRenderTarget& render_target,
                    gfx::DebugRenderer& debug_renderer,
                    const glm::mat4& view_proj);

    struct Impl;

private:
    /** Clean up data structures, removing unused objects, bodies, and shapes. */
    void collect_garbage();

    ImplPtr<Impl> m_impl;
};

inline Opt<DynamicBodyHandle> PhysicsBodyHandle::as_dynamic_body() const
{
    if (type() != PhysicsBodyType::dynamic_body) {
        return nullopt;
    }
    return DynamicBodyHandle::downcast(*this);
}

inline Opt<StaticBodyHandle> PhysicsBodyHandle::as_static_body() const
{
    if (type() != PhysicsBodyType::static_body) {
        return nullopt;
    }
    return StaticBodyHandle::downcast(*this);
}

inline Opt<GhostObjectHandle> PhysicsBodyHandle::as_ghost_body() const
{
    if (type() != PhysicsBodyType::ghost_object) {
        return nullopt;
    }
    return GhostObjectHandle::downcast(*this);
}

} // namespace Mg::physics
