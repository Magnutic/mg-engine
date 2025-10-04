//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_physics_body_handle.h
 * Reference-counted physics body.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/physics/mg_collision_group.h"
#include "mg/physics/mg_shape.h"
#include "mg/utils/mg_optional.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <utility>

namespace Mg::physics {

/** Internal data for physics bodies. */
class PhysicsBody;

/** Internal data for static physics bodies. */
class StaticBody;

/** Internal data for dynamic physics bodies. */
class DynamicBody;

/** Internal data for ghost objects. */
class GhostObject;

enum class PhysicsBodyType { static_body, dynamic_body, ghost_object };

// Forward-declare derived types.
class DynamicBodyHandle;
class StaticBodyHandle;
class GhostObjectHandle;

/** Base physics body handle.
 * Derived types are DynamicBodyHandle, StaticBodyHandle, and GhostBodyHandle.
 */
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
    friend class PhysicsWorld;
};

} // namespace Mg::physics
