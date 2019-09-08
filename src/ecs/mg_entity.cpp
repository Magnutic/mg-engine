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

#include "mg/ecs/mg_entity.h"

#include "mg/utils/mg_gsl.h"

namespace Mg::ecs {

size_t ComponentTag::n_component_types = 0;

// Reset EntityCollection, destroying all entities and components
void EntityCollection::reset() noexcept
{
    for (auto& ptr : m_component_collections) { ptr.release(); }
    m_entity_data.clear();
    m_component_lists.clear();
}

Entity EntityCollection::create_entity()
{
    Entity entity                      = m_entity_data.emplace();
    data(entity).component_list_handle = m_component_lists.emplace();
    return entity;
}

void EntityCollection::delete_entity(Entity entity)
{
    // Delete all components belonging to this Entity
    for (uint32_t i = 0; i < k_max_component_types; ++i) {
        // Remove component if it exists
        if (component_mask(entity).test(i)) {
            const auto& handle = component_handle_ref(entity, i);
            at(m_component_collections, i)->erase(handle);
        }
    }

    // Delete components
    m_component_lists.erase(data(entity).component_list_handle);

    m_entity_data.erase(entity.handle());
}

Slot_map_handle& EntityCollection::component_handle_ref(Entity entity, size_t component_type_id)
{
    const auto& list_handle  = data(entity).component_list_handle;
    auto&       handle_array = m_component_lists[list_handle];
    return handle_array.at(component_type_id);
}

} // namespace Mg::ecs
