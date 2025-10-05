//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/physics/mg_ghost_object_handle.h"

#include "mg_ghost_object.h"

namespace Mg::physics {

void GhostObjectHandle::set_transform(const glm::mat4& transform)
{
    data().object.setWorldTransform(convert_transform(transform));
    data().transform = transform;
}

std::span<const Collision* const> GhostObjectHandle::collisions() const
{
    return data().collisions;
}

GhostObject& GhostObjectHandle::data()
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::ghost_object);
    return *static_cast<GhostObject*>(m_data); // NOLINT
}

const GhostObject& GhostObjectHandle::data() const
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::ghost_object);
    return *static_cast<const GhostObject*>(m_data); // NOLINT
}

} // namespace Mg::physics
