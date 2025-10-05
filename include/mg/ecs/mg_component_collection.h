//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file ecs/mg_component_collection.h
 * Collection of components within an EntityCollection.
 * @see mg_entity.h
 */

#pragma once

#include "mg/containers/mg_slot_map.h"
#include "mg/ecs/mg_base_component.h"
#include "mg/utils/mg_macros.h"

namespace Mg::ecs {

/** Interface for ComponentCollection for any Component type. */
class IComponentCollection {
public:
    MG_INTERFACE_BOILERPLATE(IComponentCollection);
    virtual void erase(Slot_map_handle handle) noexcept = 0;
    virtual void clear() noexcept = 0;
};

/** ComponentCollection creates, stores, and destroys Components. */
template<Component C> class ComponentCollection : public IComponentCollection {
public:
    explicit ComponentCollection(uint32_t num_elems) : m_data{ num_elems } {}

    template<typename... Ts> Slot_map_handle emplace(Ts&&... args)
    {
        return { m_data.insert(C{ /*BaseComponent*/ {}, std::forward<Ts>(args)... }) };
    }

    void erase(Slot_map_handle handle) noexcept override { m_data.erase(handle); }

    void clear() noexcept override { m_data.clear(); }

    C& get_component(Slot_map_handle handle) { return m_data[handle]; }

private:
    Slot_map<C> m_data;
};

} // namespace Mg::ecs
