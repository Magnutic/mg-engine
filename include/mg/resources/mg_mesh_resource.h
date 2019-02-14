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

/** @file mg_mesh_resource.h
 * Mesh data resource type.
 */

#pragma once

#include <glm/vec3.hpp>

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_submesh.h"
#include "mg/gfx/mg_vertex.h"
#include "mg/memory/mg_compacting_heap.h"
#include "mg/resources/mg_base_resource.h"
#include "mg/utils/mg_gsl.h"

namespace Mg {

/** Mesh data resource. */
class MeshResource : public BaseResource {
public:
    using BaseResource::BaseResource;

    span<const SubMesh>           sub_meshes() const { return span{ m_sub_meshes }; }
    span<const Vertex>            vertices() const { return span{ m_vertices }; }
    span<const uint_vertex_index> indices() const { return span{ m_indices }; }

    /** Get model-space centre coordinate (mean vertex coordinate). */
    glm::vec3 centre() const { return m_centre; }

    /** Get mesh radius (distance from centre to most distant vertex). */
    float radius() const { return m_radius; }

    /** Returns whether mesh data is valid. Useful for debug mode asserts. */
    bool validate() const;

    /** Calculate bounds of this mesh, i.e. centre and radius. */
    void calculate_bounds();

    bool should_reload_on_file_change() const override { return true; }

    void load_resource(const ResourceDataLoader& data_loader) override;

private:
    // Owning pointers to mesh data
    memory::DA_UniquePtr<SubMesh[]>           m_sub_meshes;
    memory::DA_UniquePtr<Vertex[]>            m_vertices;
    memory::DA_UniquePtr<uint_vertex_index[]> m_indices;

    glm::vec3 m_centre{};
    float     m_radius = 0.0f;
};

} // namespace Mg
