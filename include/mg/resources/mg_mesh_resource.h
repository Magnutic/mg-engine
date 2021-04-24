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
#include "mg/gfx/mg_animation.h"
#include "mg/mg_bounding_volumes.h"
#include "mg/resource_cache/mg_base_resource.h"
#include "mg/utils/mg_gsl.h"

#include <memory>

namespace Mg::gfx::Mesh {
struct Vertex;
struct JointData;
struct Submesh;
struct Joint;
struct Influences;
struct MeshDataView;
using Index = uint16_t;
} // namespace Mg::gfx::Mesh

namespace Mg {

/** Mesh data resource. */
class MeshResource final : public BaseResource {
public:
    struct Data;

    explicit MeshResource(Identifier id);
    ~MeshResource() override;

    MG_MAKE_DEFAULT_MOVABLE(MeshResource);
    MG_MAKE_NON_COPYABLE(MeshResource);

    gfx::Mesh::MeshDataView data_view() const noexcept;

    span<const gfx::Mesh::Vertex> vertices() const noexcept;
    span<const gfx::Mesh::Index> indices() const noexcept;
    span<const gfx::Mesh::Submesh> submeshes() const noexcept;
    span<const gfx::Mesh::Influences> influences() const noexcept;
    span<const gfx::Mesh::Joint> joints() const noexcept;
    span<const gfx::Mesh::AnimationClip> animation_clips() const noexcept;

    const glm::mat4& skeleton_root_transform() const noexcept;

    bool has_joints() const noexcept { return !joints().empty(); }

    /** Get model-space bounding sphere. */
    BoundingSphere bounding_sphere() const noexcept;

    /** Get model-space axis-aligned bounding box. */
    AxisAlignedBoundingBox axis_aligned_bounding_box() const noexcept;

    /** Returns whether mesh data is valid. Useful for debug mode asserts. */
    bool validate() const;

    bool should_reload_on_file_change() const noexcept override { return true; }

    Identifier type_id() const noexcept override { return "MeshResource"; }

protected:
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

private:
    // Calculate bounds of this mesh, i.e. centre and radius.
    void calculate_bounds() noexcept;
    std::unique_ptr<Data> m_data;
};

} // namespace Mg
