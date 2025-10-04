//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/physics/mg_static_body_handle.h"

#include "mg_static_body.h"

namespace Mg::physics {

StaticBody& StaticBodyHandle::data()
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::static_body);
    return *static_cast<StaticBody*>(m_data); // NOLINT
}

const StaticBody& StaticBodyHandle::data() const
{
    MG_ASSERT_DEBUG(type() == PhysicsBodyType::static_body);
    return *static_cast<const StaticBody*>(m_data); // NOLINT
}

} // namespace Mg::physics
