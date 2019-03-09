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

#include "mg/gfx/mg_debug_renderer.h"

#include "mg/core/mg_rotation.h"
#include "mg/gfx/mg_shader.h"
#include "mg/utils/mg_assert.h"

#include "mg_gl_debug.h"
#include "mg_opengl_shader.h"
#include "mg_glad.h"

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>

#include <cstdint>
#include <map>
#include <vector>

namespace Mg::gfx {

static const char vs_code[] = R"(
    #version 330 core
    layout(location = 0) in vec3 vert_position;

    uniform mat4 MVP;

    void main()
    {
        gl_Position = MVP * vec4(vert_position, 1.0);
    }
)";

static const char fs_code[] = R"(
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
    GLuint vao_id = 0;
    GLuint vbo_id = 0;
    GLuint ibo_id = 0;

    int32_t num_indices = 0;

    DebugMesh() = default;

    MG_MAKE_NON_COPYABLE(DebugMesh);
    MG_MAKE_NON_MOVABLE(DebugMesh);

    ~DebugMesh()
    {
        glDeleteVertexArrays(1, &vao_id);
        glDeleteBuffers(1, &vbo_id);
        glDeleteBuffers(1, &ibo_id);
    }
};

static DebugMesh generate_mesh(span<const glm::vec3> positions, span<const uint16_t> indices)
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

    glBufferData(GL_ARRAY_BUFFER,
                 GLsizeiptr(positions.size_bytes()),
                 positions.data(),
                 GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 GLsizeiptr(indices.size_bytes()),
                 indices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    MG_CHECK_GL_ERROR();

    return { vao_id, vbo_id, ibo_id, narrow<int32_t>(indices.size()) };
}

static const glm::vec3 box_vertices[] = {
    { -0.5, -0.5, 0.5 },  { 0.5, -0.5, 0.5 },  { 0.5, 0.5, 0.5 },  { -0.5, 0.5, 0.5 },
    { -0.5, -0.5, -0.5 }, { 0.5, -0.5, -0.5 }, { 0.5, 0.5, -0.5 }, { -0.5, 0.5, -0.5 },
};

static const uint16_t box_indices[] = { 0, 1, 2, 2, 3, 0,

                                        4, 5, 1, 1, 0, 4,

                                        5, 6, 2, 2, 1, 5,

                                        6, 7, 3, 3, 2, 6,

                                        7, 4, 0, 0, 3, 7,

                                        7, 6, 5, 5, 4, 7 };

struct EllipsoidData {
    std::vector<glm::vec3> verts;
    std::vector<uint16_t>  indices;
};

static EllipsoidData generate_ellipsoid_verts(size_t steps)
{
    MG_ASSERT(steps > 3);
    EllipsoidData data;

    // Bottom cap vertex
    data.verts.emplace_back(0.0f, 0.0f, -1.0f);

    const size_t v_steps = steps / 2 - 1;
    const size_t h_steps = steps;

    // Vertical step
    for (size_t i = 0; i < v_steps; ++i) {
        float z_offset = (i + 1) * glm::pi<float>() / (v_steps + 1) - glm::half_pi<float>();

        float z = glm::sin(z_offset);
        float r = glm::cos(z_offset);

        // Horizontal step (flat circle)
        for (size_t u = 0; u < h_steps; ++u) {
            float h_offset = u * (2.0f * glm::pi<float>() / h_steps);
            float x        = glm::cos(h_offset) * r;
            float y        = glm::sin(h_offset) * r;
            data.verts.emplace_back(x, y, z);
        }
    }

    // Top cap vertex
    data.verts.emplace_back(0.0f, 0.0f, 1.0f);

    const size_t bottom_cap_vert_index = 0;
    const size_t top_cap_vert_index    = data.verts.size() - 1;

    // Index of first vertex of strip
    auto strip_vertex_index = [&](size_t strip_index) {
        MG_ASSERT_DEBUG(strip_index < v_steps);
        return 1 + strip_index * h_steps;
    };

    // Create a triangle by adding three vertex indices
    auto add_tri = [&](size_t fst, size_t snd, size_t thd) {
        data.indices.push_back(gsl::narrow_cast<uint16_t>(fst));
        data.indices.push_back(gsl::narrow_cast<uint16_t>(snd));
        data.indices.push_back(gsl::narrow_cast<uint16_t>(thd));
    };

    // Generate triangles for the caps at bottom and top of sphere
    auto gen_cap_tris = [&](size_t cap_vert_index, size_t vert_strip_begin, bool top) {
        for (size_t i = 0; i < h_steps; ++i) {
            size_t fst = cap_vert_index;
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
    auto gen_strip_tris = [&](size_t vert_strip_index) {
        size_t vert_strip_begin = strip_vertex_index(vert_strip_index);
        size_t next_strip_begin = strip_vertex_index(vert_strip_index + 1);

        for (size_t i = 0; i < h_steps; ++i) {
            size_t fst  = vert_strip_begin + i;
            size_t snd  = vert_strip_begin + (i + 1) % h_steps;
            size_t thd  = next_strip_begin + i;
            size_t frth = next_strip_begin + (i + 1) % h_steps;

            add_tri(fst, snd, thd);
            add_tri(thd, snd, frth);
        }
    };

    for (size_t i = 0; i < v_steps - 1; ++i) { gen_strip_tris(i); }

    gen_cap_tris(top_cap_vert_index, strip_vertex_index(v_steps - 1), true);

    for (auto i : data.indices) { MG_ASSERT(i < data.verts.size()); }

    return data;
}

//--------------------------------------------------------------------------------------------------
// DebugRenderer implementation
//--------------------------------------------------------------------------------------------------

struct Sphere {
    Sphere(const EllipsoidData& data) : mesh{ generate_mesh(data.verts, data.indices) } {}
    DebugMesh mesh;
};

struct DebugRendererData {
    ShaderProgram program = [] {
        ShaderOwner vs = compile_vertex_shader(vs_code).value();
        ShaderOwner fs = compile_fragment_shader(fs_code).value();
        return ShaderProgram::make(vs.shader_handle(), fs.shader_handle()).value();
    }();

    DebugMesh                box = generate_mesh(box_vertices, box_indices);
    std::map<size_t, Sphere> spheres;
};

DebugRenderer::DebugRenderer()  = default;
DebugRenderer::~DebugRenderer() = default;

static void draw_primitive(ShaderProgram&                      program,
                           const ICamera&                      camera,
                           const DebugMesh&                    mesh,
                           DebugRenderer::PrimitiveDrawParams& params)
{
    glm::mat4 M = glm::translate({}, params.centre) * params.orientation.to_matrix() *
                  glm::scale(params.dimensions);

    opengl::use_program(program);
    opengl::set_uniform(opengl::uniform_location(program, "colour"), params.colour);
    opengl::set_uniform(opengl::uniform_location(program, "MVP"), camera.view_proj_matrix() * M);

    glBindVertexArray(mesh.vao_id);

    GLint     old_poly_mode = 0;
    GLboolean old_culling   = 0;

    if (params.wireframe) {
        glGetIntegerv(GL_POLYGON_MODE, &old_poly_mode);
        glGetBooleanv(GL_CULL_FACE, &old_culling);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_CULL_FACE);
    }

    glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_SHORT, nullptr);

    if (params.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, uint32_t(old_poly_mode));
        glEnable(GL_CULL_FACE);
    }

    glBindVertexArray(0);
}

void DebugRenderer::draw_box(const ICamera& camera, BoxDrawParams params)
{
    draw_primitive(data().program, camera, data().box, params);
}

void DebugRenderer::draw_ellipsoid(const ICamera& camera, EllipsoidDrawParams params)
{
    auto it = data().spheres.find(params.steps);

    // If no sphere mesh with the required amount of steps was found, create it.
    if (it == data().spheres.end()) {
        auto p = data().spheres.emplace(params.steps, generate_ellipsoid_verts(params.steps));
        it     = p.first;
    }

    Sphere& sphere = it->second;
    draw_primitive(data().program, camera, sphere.mesh, params);
}

} // namespace Mg::gfx
