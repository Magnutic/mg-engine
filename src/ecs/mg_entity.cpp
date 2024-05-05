//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/ecs/mg_entity.h"

#include "mg/utils/mg_gsl.h"

namespace Mg::ecs {

// Reset EntityCollection, destroying all entities and components
void EntityCollection::reset() noexcept
{
    for (auto& ptr : m_component_collections) {
        if (ptr != nullptr) {
            ptr->clear();
        }
    }
    m_entity_data.clear();
    m_component_lists.clear();
}

Entity EntityCollection::create_entity()
{
    Entity entity = m_entity_data.emplace();
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
    const auto& list_handle = data(entity).component_list_handle;
    auto& handle_array = m_component_lists[list_handle];
    return handle_array.at(component_type_id);
}

} // namespace Mg::ecs
