//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_resource.h
 * Mesh data resource type.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/resource_cache/mg_base_resource.h"
#include "mg/utils/mg_gsl.h"

#include <glm/vec3.hpp>

namespace Mg {

/** Mesh data resource. */
class MeshResource final : public BaseResource {
public:
    using BaseResource::BaseResource;

    gfx::MeshDataView data_view() const noexcept
    {
        return { vertices(), indices(), sub_meshes(), { { centre(), radius() } } };
    }

    span<const gfx::SubMesh> sub_meshes() const noexcept { return span{ m_sub_meshes }; }
    span<const gfx::Vertex> vertices() const noexcept { return span{ m_vertices }; }
    span<const gfx::uint_vertex_index> indices() const noexcept { return span{ m_indices }; }

    /** Get model-space centre coordinate (mean vertex coordinate). */
    glm::vec3 centre() const noexcept { return m_centre; }

    /** Get mesh radius (distance from centre to most distant vertex). */
    float radius() const noexcept { return m_radius; }

    /** Returns whether mesh data is valid. Useful for debug mode asserts. */
    bool validate() const;

    /** Calculate bounds of this mesh, i.e. centre and radius. */
    void calculate_bounds() noexcept;

    bool should_reload_on_file_change() const noexcept override { return true; }

    Identifier type_id() const noexcept override { return "MeshResource"; }

protected:
    LoadResourceResult load_resource_impl(const ResourceLoadingInput& input) override;

private:
    Array<gfx::SubMesh> m_sub_meshes;
    Array<gfx::Vertex> m_vertices;
    Array<gfx::uint_vertex_index> m_indices;

    glm::vec3 m_centre{};
    float m_radius = 0.0f;
};

} // namespace Mg
