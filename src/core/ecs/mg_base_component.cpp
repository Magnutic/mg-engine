//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/ecs/mg_base_component.h"

#include <mutex>

namespace Mg::ecs {

namespace {
size_t g_next_component_type_id{ 0 };
std::mutex g_component_type_id_init_mutex;
} // namespace

namespace detail {
void set_component_id(size_t& component_type_id)
{
    std::lock_guard g{ g_component_type_id_init_mutex };
    if (component_type_id == k_unset_component_tag) {
        component_type_id = g_next_component_type_id++;
    }
    MG_ASSERT(component_type_id < k_max_component_types);
}
} // namespace detail

} // namespace Mg::ecs
