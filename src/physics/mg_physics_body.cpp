//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_bullet_utils.h"
#include "mg_physics_body.h"

namespace Mg::physics {

PhysicsBodyHandle::PhysicsBodyHandle(PhysicsBody* data) : m_data(data)
{
    if (data) {
        ++m_data->ref_count;
    }
}

PhysicsBodyHandle::~PhysicsBodyHandle()
{
    if (m_data) {
        --m_data->ref_count;
    }
}

PhysicsBodyHandle::PhysicsBodyHandle(const PhysicsBodyHandle& rhs) : m_data(rhs.m_data)
{
    if (m_data) {
        ++m_data->ref_count;
    }
}

PhysicsBodyHandle& PhysicsBodyHandle::operator=(const PhysicsBodyHandle& rhs)
{
    auto tmp(rhs);
    swap(tmp);
    return *this;
}

Identifier PhysicsBodyHandle::id() const
{
    return m_data->id;
}

PhysicsBodyType PhysicsBodyHandle::type() const
{
    return m_data->type;
}

void PhysicsBodyHandle::has_contact_response(const bool enable)
{
    btCollisionObject& bt_body = m_data->get_bt_body();
    const auto old_collision_flags = bt_body.getCollisionFlags();
    const auto new_collision_flags =
        enable ? (old_collision_flags & ~btCollisionObject::CF_NO_CONTACT_RESPONSE)
               : (old_collision_flags | btCollisionObject::CF_NO_CONTACT_RESPONSE);
    bt_body.setCollisionFlags(new_collision_flags);
}

bool PhysicsBodyHandle::has_contact_response() const
{
    return m_data->get_bt_body().hasContactResponse();
}

glm::mat4 PhysicsBodyHandle::get_transform() const
{
    return m_data->transform;
}

void PhysicsBodyHandle::set_filter_group(const CollisionGroup group)
{
    m_data->get_bt_body().getBroadphaseHandle()->m_collisionFilterGroup =
        convert_collision_group(group);
}

CollisionGroup PhysicsBodyHandle::get_filter_group() const
{
    return convert_collision_group(
        m_data->get_bt_body().getBroadphaseHandle()->m_collisionFilterGroup);
}

void PhysicsBodyHandle::set_filter_mask(const CollisionGroup mask)
{
    m_data->get_bt_body().getBroadphaseHandle()->m_collisionFilterMask =
        convert_collision_group(mask);
}

CollisionGroup PhysicsBodyHandle::get_filter_mask() const
{
    return convert_collision_group(
        m_data->get_bt_body().getBroadphaseHandle()->m_collisionFilterMask);
}

Shape& PhysicsBodyHandle::shape()
{
    return *m_data->shape;
}

const Shape& PhysicsBodyHandle::shape() const
{
    return *m_data->shape;
}

} // namespace Mg::physics
