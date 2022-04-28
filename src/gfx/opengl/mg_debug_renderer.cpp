//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_debug_renderer.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_rotation.h"
#include "mg/gfx/mg_joint.h"
#include "mg/gfx/mg_skeleton.h"
#include "mg/utils/mg_assert.h"

#include "../mg_shader.h"
#include "mg_gl_debug.h"
#include "mg_opengl_shader.h"
#include "mg_glad.h"

#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace Mg::gfx {

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;

namespace {

const char* vs_code = R"(
    #version 330 core
    layout(location = 0) in vec3 vert_position;

    uniform mat4 MVP;

    void main()
    {
        gl_Position = MVP * vec4(vert_position, 1.0);
    }
)";

const char* fs_code = R"(
    #version 330 core
    uniform vec4 colour;

    layout(location = 0) out vec4 frag_colour;

    void main()
    {
        frag_colour = colour;
    }
)";

// RAII owner for OpenGL mesh buffers
class DebugMesh {
public:
    VertexArrayHandle::Owner vao;
    BufferHandle::Owner vbo;
    BufferHandle::Owner ibo;

    GLsizei num_indices = 0;
};

DebugMesh generate_mesh(span<const vec3> positions, span<const uint16_t> indices)
{
    GLuint vao_id = 0;
    GLuint vbo_id = 0;
    GLuint ibo_id = 0;

    glGenVertexArrays(1, &vao_id);
    glGenBuffers(1, &vbo_id);
    glGenBuffers(1, &ibo_id);

    glBindVertexArray(vao_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);

    if (!positions.empty()) {
        glBufferData(GL_ARRAY_BUFFER,
                     as<GLsizeiptr>(positions.size_bytes()),
                     positions.data(),
                     GL_DYNAMIC_DRAW);
    }
    if (!indices.empty()) {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     as<GLsizeiptr>(indices.size_bytes()),
                     indices.data(),
                     GL_DYNAMIC_DRAW);
    }

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    MG_CHECK_GL_ERROR();

    return { VertexArrayHandle::Owner{ vao_id },
             BufferHandle::Owner{ vbo_id },
             BufferHandle::Owner{ ibo_id },
             as<GLsizei>(indices.size()) };
}

void update_mesh(DebugMesh& debug_mesh, span<const vec3> positions, span<const uint16_t> indices)
{
    glBindVertexArray(debug_mesh.vao.handle.as_gl_id());

    glBindBuffer(GL_ARRAY_BUFFER, debug_mesh.vbo.handle.as_gl_id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, debug_mesh.ibo.handle.as_gl_id());

    glBufferData(GL_ARRAY_BUFFER,
                 as<GLsizeiptr>(positions.size_bytes()),
                 positions.data(),
                 GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 as<GLsizeiptr>(indices.size_bytes()),
                 indices.data(),
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    debug_mesh.num_indices = as<GLsizei>(indices.size());

    MG_CHECK_GL_ERROR();
}

const std::array<vec3, 8> box_vertices = { {
    { -0.5, -0.5, 0.5 },
    { 0.5, -0.5, 0.5 },
    { 0.5, 0.5, 0.5 },
    { -0.5, 0.5, 0.5 },
    { -0.5, -0.5, -0.5 },
    { 0.5, -0.5, -0.5 },
    { 0.5, 0.5, -0.5 },
    { -0.5, 0.5, -0.5 },
} };

// clang-format off
const std::array<uint16_t, 36> box_indices = { {
    0, 1, 2,  2, 3, 0,
    4, 5, 1,  1, 0, 4,
    5, 6, 2,  2, 1, 5,
    6, 7, 3,  3, 2, 6,
    7, 4, 0,  0, 3, 7,
    7, 6, 5,  5, 4, 7 
} };
// clang-format on

std::vector<uint16_t> generate_line_vertex_indices(const size_t num_vertices)
{
    std::vector<uint16_t> indices(num_vertices * 2u - 1u);
    for (size_t i = 0; i + 1 < indices.size(); i += 2u) {
        indices[i] = as<uint16_t>(i / 2);
        indices[i + 1] = as<uint16_t>(i / 2 + 1);
    }
    return indices;
}

struct EllipsoidData {
    std::vector<vec3> verts;
    std::vector<uint16_t> indices;
};

EllipsoidData generate_ellipsoid_verts(size_t steps)
{
    MG_ASSERT(steps > 3);
    EllipsoidData data;

    // Bottom cap vertex
    data.verts.emplace_back(0.0f, 0.0f, -1.0f);

    const size_t v_steps = steps / 2 - 1;
    const size_t h_steps = steps;

    // Vertical step
    for (size_t i = 0; i < v_steps; ++i) {
        const float z_offset = narrow_cast<float>(i + 1) * glm::pi<float>() /
                                   narrow_cast<float>(v_steps + 1) -
                               glm::half_pi<float>();

        const float z = std::sin(z_offset);
        const float r = std::cos(z_offset);

        // Horizontal step (flat circle)
        for (size_t u = 0; u < h_steps; ++u) {
            const float h_offset = float(u) * (2.0f * glm::pi<float>() / float(h_steps));
            const float x = std::cos(h_offset) * r;
            const float y = std::sin(h_offset) * r;
            data.verts.emplace_back(x, y, z);
        }
    }

    // Top cap vertex
    data.verts.emplace_back(0.0f, 0.0f, 1.0f);

    const size_t bottom_cap_vert_index = 0;
    const size_t top_cap_vert_index = data.verts.size() - 1;

    // Index of first vertex of strip
    const auto strip_vertex_index = [&](size_t strip_index) {
        MG_ASSERT_DEBUG(strip_index < v_steps);
        return 1 + strip_index * h_steps;
    };

    // Create a triangle by adding three vertex indices
    const auto add_tri = [&](size_t fst, size_t snd, size_t thd) {
        data.indices.push_back(gsl::narrow_cast<uint16_t>(fst));
        data.indices.push_back(gsl::narrow_cast<uint16_t>(snd));
        data.indices.push_back(gsl::narrow_cast<uint16_t>(thd));
    };

    // Generate triangles for the caps at bottom and top of sphere
    const auto gen_cap_tris = [&](size_t cap_vert_index, size_t vert_strip_begin, bool top) {
        for (size_t i = 0; i < h_steps; ++i) {
            const size_t fst = cap_vert_index;
            size_t snd = vert_strip_begin + (i + 1) % h_steps;
            size_t thd = vert_strip_begin + i;

            if (top) {
                std::swap(snd, thd); // Swap for triangle winding
            }
            add_tri(fst, snd, thd);
        }
    };

    gen_cap_tris(bottom_cap_vert_index, strip_vertex_index(0), false);

    // Generate triangles for the strips making up the rest of the sphere
    const auto gen_strip_tris = [&](size_t vert_strip_index) {
        const size_t vert_strip_begin = strip_vertex_index(vert_strip_index);
        const size_t next_strip_begin = strip_vertex_index(vert_strip_index + 1);

        for (size_t i = 0; i < h_steps; ++i) {
            const size_t fst = vert_strip_begin + i;
            const size_t snd = vert_strip_begin + (i + 1) % h_steps;
            const size_t thd = next_strip_begin + i;
            const size_t frth = next_strip_begin + (i + 1) % h_steps;

            add_tri(fst, snd, thd);
            add_tri(thd, snd, frth);
        }
    };

    for (size_t i = 0; i < v_steps - 1; ++i) {
        gen_strip_tris(i);
    }

    gen_cap_tris(top_cap_vert_index, strip_vertex_index(v_steps - 1), true);

    for (auto i : data.indices) {
        MG_ASSERT(i < data.verts.size());
    }

    return data;
}

struct Sphere {
    Sphere(const EllipsoidData& data) : mesh{ generate_mesh(data.verts, data.indices) } {}
    DebugMesh mesh;
};

/** RAII-owning wrapper for shader handles. */
template<typename ShaderHandleT> class ShaderOwner {
public:
    explicit ShaderOwner(ShaderHandleT handle_) noexcept : handle(std::move(handle_)) {}
    ~ShaderOwner() { destroy_shader(handle); }

    MG_MAKE_NON_COPYABLE(ShaderOwner);
    MG_MAKE_DEFAULT_MOVABLE(ShaderOwner);

    ShaderHandleT handle;
};

class ShaderProgramOwner {
public:
    explicit ShaderProgramOwner(opengl::ShaderProgramHandle handle_) : handle(std::move(handle_))
    {
        uniform_location_colour = opengl::uniform_location(handle, "colour").value();
        uniform_location_MVP = opengl::uniform_location(handle, "MVP").value();
    }
    ~ShaderProgramOwner() { opengl::destroy_shader_program(handle); }

    MG_MAKE_DEFAULT_MOVABLE(ShaderProgramOwner);
    MG_MAKE_NON_COPYABLE(ShaderProgramOwner);

    opengl::ShaderProgramHandle handle;
    opengl::UniformLocation uniform_location_colour;
    opengl::UniformLocation uniform_location_MVP;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// DebugRenderer implementation
//--------------------------------------------------------------------------------------------------

struct DebugRendererData {
    ShaderProgramOwner program = [] {
        ShaderOwner vs{ compile_vertex_shader(vs_code).value() };
        ShaderOwner fs{ compile_fragment_shader(fs_code).value() };
        return ShaderProgramOwner(
            opengl::link_shader_program(vs.handle, nullopt, fs.handle).value());
    }();

    DebugMesh box = generate_mesh(box_vertices, box_indices);
    FlatMap<size_t, Sphere> spheres;
    DebugMesh line = generate_mesh({}, {});
};

DebugRenderer::DebugRenderer() = default;
DebugRenderer::~DebugRenderer() = default;

namespace {

void draw(const ShaderProgramOwner& program,
          const mat4& MVP,
          const DebugMesh& mesh,
          const vec4& colour,
          const bool wireframe,
          const bool line_mode,
          const float line_width = 1.0f)
{
    opengl::use_program(program.handle);
    opengl::set_uniform(program.uniform_location_colour, colour);
    opengl::set_uniform(program.uniform_location_MVP, MVP);

    glBindVertexArray(mesh.vao.handle.as_gl_id());

    std::array<GLint, 2> old_poly_mode = { 0, 0 };
    GLboolean old_culling = 0;

    if (wireframe) {
        glGetIntegerv(GL_POLYGON_MODE, old_poly_mode.data());
        glGetBooleanv(GL_CULL_FACE, &old_culling);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_CULL_FACE);
    }

    float old_line_width = 0.0f;
    glGetFloatv(GL_LINE_WIDTH, &old_line_width);
    glLineWidth(line_width);

    const GLenum primitive_type = line_mode ? GL_LINES : GL_TRIANGLES;
    glDrawElements(primitive_type, mesh.num_indices, GL_UNSIGNED_SHORT, nullptr);

    if (wireframe) {
        // glGet(GL_POLYGON_MODE) returns front- and back polygon modes separately; but since
        // GL 3.2, it is not allowed to set front- and back separately.
        MG_ASSERT_DEBUG(old_poly_mode[0] == old_poly_mode[1]);
        glPolygonMode(GL_FRONT_AND_BACK, as<GLuint>(old_poly_mode[0]));
        glEnable(GL_CULL_FACE);
    }

    glLineWidth(old_line_width);

    glBindVertexArray(0);
}

void draw_primitive(const ShaderProgramOwner& program,
                    const mat4& view_proj,
                    const DebugMesh& mesh,
                    const DebugRenderer::PrimitiveDrawParams& params)
{
    const mat4 translation = [&] {
        mat4 t(1.0f);
        t[3] = vec4(params.centre, 1.0f);
        return t;
    }();

    const mat4 MVP = view_proj * translation * params.orientation.to_matrix() *
                     scale(params.dimensions);

    draw(program, MVP, mesh, params.colour, params.wireframe, false);
}

} // namespace

void DebugRenderer::draw_box(const mat4& view_proj, BoxDrawParams params)
{
    draw_primitive(impl().program, view_proj, impl().box, params);
}

void DebugRenderer::draw_ellipsoid(const mat4& view_proj, EllipsoidDrawParams params)
{
    auto it = impl().spheres.find(params.steps);

    // If no sphere mesh with the required amount of steps was found, create it.
    if (it == impl().spheres.end()) {
        auto p = impl().spheres.insert({ params.steps, generate_ellipsoid_verts(params.steps) });
        it = p.first;
    }

    const Sphere& sphere = it->second;
    draw_primitive(impl().program, view_proj, sphere.mesh, params);
}

void DebugRenderer::draw_line(const mat4& view_proj,
                              span<const vec3> points,
                              const vec4& colour,
                              const float width)
{
    const auto indices = generate_line_vertex_indices(points.size());
    update_mesh(impl().line, points, indices);
    draw(impl().program, view_proj, impl().line, colour, false, true, width);
}

void DebugRenderer::draw_bones(const mat4& view_proj,
                               const mat4& M,
                               const Skeleton& skeleton,
                               const SkeletonPose& pose)
{
    std::vector<mat4> joint_poses;
    {
        joint_poses.resize(skeleton.joints().size());
        const bool success = calculate_pose_transformations(skeleton, pose, joint_poses);
        MG_ASSERT(success);
    }

    if (joint_poses.empty()) {
        return;
    }

    const float bone_line_width = 10.0f;
    const float joint_axis_length = 0.1f;

    const vec4 bone_colour(0.5f, 0.5f, 1.0f, 0.5f);

    const vec4 origo(0.0f, 0.0f, 0.0f, 1.0f);

    auto draw_joint_axes = [&](const mat4 matrix) {
        const vec4 centre = matrix * origo;

        // Draw the axis and use the vector also as colour, so x is red, y is green, z is blue.
        // It is easy to memorize which axis has which color if you think xyz=rgb.
        auto draw_axis = [&](const vec4 axis) {
            const vec4 axis_point = centre + joint_axis_length * normalize(matrix * axis - centre);
            draw_line(view_proj, centre, axis_point, axis, 2.0f);
        };

        draw_axis({ 1.0f, 0.0f, 0.0f, 1.0f });
        draw_axis({ 0.0f, 1.0f, 0.0f, 1.0f });
        draw_axis({ 0.0f, 0.0f, 1.0f, 1.0f });
    };

    auto draw_bones_impl = [&](const vec4& parent_position,
                               const Mesh::JointId parent_joint_id,
                               const Mesh::JointId joint_id,
                               auto&& recurse) -> void {
        const bool is_root_joint = parent_joint_id == Mesh::joint_id_none;

        const mat4 matrix = M * joint_poses.at(joint_id);
        draw_joint_axes(matrix);

        const vec4 position = matrix * origo;
        if (!is_root_joint) {
            draw_line(view_proj, parent_position, position, bone_colour, bone_line_width);
        }

        for (const Mesh::JointId child_id : skeleton.joints()[joint_id].children) {
            if (child_id != Mesh::joint_id_none) {
                recurse(position, joint_id, child_id, recurse);
            }
        }
    };

    draw_bones_impl({}, Mesh::joint_id_none, 0, draw_bones_impl);
}

void DebugRenderer::draw_view_frustum(const glm::mat4& view_projection,
                                      const glm::mat4& view_projection_frustum,
                                      const float max_distance)
{
    // Corners in clip space
    std::array<glm::vec3, 8> corners = { {
        { -1, -1, -1 },
        { 1, -1, -1 },
        { 1, 1, -1 },
        { -1, 1, -1 },
        { -1, -1, 1 },
        { 1, -1, 1 },
        { 1, 1, 1 },
        { -1, 1, 1 },
    } };

    // Transform from clip to world space
    const glm::mat4 inverse_frustum = glm::inverse(view_projection_frustum);
    for (auto& corner : corners) {
        auto corner_temp = inverse_frustum * glm::vec4(corner, 1.0f);
        corner = glm::vec3(corner_temp / corner_temp.w);
    }

    if (max_distance > 0.0f) {
        for (size_t i = 4; i < corners.size(); ++i) {
            auto& far_corner = corners[i];
            const auto& near_corner = corners[i - 4];
            const float d2 = glm::length2(far_corner - near_corner);
            if (d2 > max_distance * max_distance) {
                far_corner = near_corner + glm::normalize(far_corner - near_corner) * max_distance;
            }
        }
    }

    // Corners are now in world space
    std::array<glm::vec3, 5> corners_near = {
        { corners[0], corners[1], corners[2], corners[3], corners[0] }
    };

    std::array<glm::vec3, 5> corners_far = {
        { corners[4], corners[5], corners[6], corners[7], corners[4] }
    };

    draw_line(view_projection, corners_near, { 1.0f, 0.0f, 0.0f, 1.0f }, 2.0f);
    draw_line(view_projection, corners_far, { 0.0f, 0.0f, 1.0f, 1.0f }, 2.0f);

    std::array<glm::vec3, 5> corners_middle = {};

    const float z_range = glm::length(corners_far[0] - corners_near[0]);

    // NOLINTNEXTLINE clang-tidy warns about float as loop counter. I think it's fine here.
    for (float d = z_range / 2.0f; d > 1.0f; d /= 2.0f) {
        for (size_t i = 0; i < corners_middle.size(); ++i) {
            corners_middle[i] = corners_near[i] +
                                (corners_far[i] - corners_near[i]) * (d / z_range);
        }
        draw_line(view_projection, corners_middle, { 0.5f, 0.5f, 0.5f, 1.0f }, 1.0f);
    }

    for (size_t i = 0; i < 4; ++i) {
        draw_line(view_projection,
                  corners_near[i],
                  corners_far[i],
                  { 0.0f, 1.0f, 1.0f, 1.0f },
                  2.0f);
    }
}

//--------------------------------------------------------------------------------------------------
// DebugRenderQueue implementation
//--------------------------------------------------------------------------------------------------

using Job = std::function<void(DebugRenderer& renderer, const glm::mat4& view_proj)>;

struct DebugRenderQueueData {
    std::mutex mutex;
    std::vector<Job> jobs;
};

DebugRenderQueue::DebugRenderQueue() = default;
DebugRenderQueue::~DebugRenderQueue() = default;

void DebugRenderQueue::draw_box(DebugRenderer::BoxDrawParams params)
{
    std::lock_guard g{ impl().mutex };

    impl().jobs.emplace_back([params](DebugRenderer& renderer, const glm::mat4& view_proj) {
        renderer.draw_box(view_proj, params);
    });
}

void DebugRenderQueue::draw_ellipsoid(DebugRenderer::EllipsoidDrawParams params)
{
    std::lock_guard g{ impl().mutex };

    impl().jobs.emplace_back([params](DebugRenderer& renderer, const glm::mat4& view_proj) {
        renderer.draw_ellipsoid(view_proj, params);
    });
}

void DebugRenderQueue::draw_line(span<const glm::vec3> points, const glm::vec4& colour, float width)
{
    std::lock_guard g{ impl().mutex };

    impl().jobs.emplace_back([points = Array<glm::vec3>::make_copy(points),
                              colour,
                              width](DebugRenderer& renderer, const glm::mat4& view_proj) {
        renderer.draw_line(view_proj, points, colour, width);
    });
}

void DebugRenderQueue::dispatch(DebugRenderer& renderer, const glm::mat4& view_proj_matrix)
{
    std::lock_guard g{ impl().mutex };

    for (Job& job : impl().jobs) {
        job(renderer, view_proj_matrix);
    }
}

void DebugRenderQueue::clear()
{
    std::lock_guard g{ impl().mutex };
    impl().jobs.clear();
}

} // namespace Mg::gfx
