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

#include "mg/gfx/mg_billboard_renderer.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <unordered_map>

#include <glm/gtx/norm.hpp>
#include <glm/mat4x4.hpp>

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_log.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_gfx_device.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/utils/mg_object_id.h"
#include "mg/utils/mg_stl_helpers.h"

#include "mg_opengl_shader.h"
#include "mg_texture_node.h"
#include "mg_glad.h"

namespace Mg::gfx {

// Binding slot for CameraBlock UniformBufferObject.
static constexpr UniformBufferSlot k_camera_ubo_slot{ 0 };

//--------------------------------------------------------------------------------------------------
// Shader code for billboard rendering
//--------------------------------------------------------------------------------------------------

constexpr auto billboard_vertex_shader = R"(
layout(location = 0) in vec3 v_position;
layout(location = 1) in vec4 v_colour;
layout(location = 2) in float v_radius;

layout(std140) uniform CameraBlock {
    uniform mat4 VP;
    uniform mat4 P;
    uniform vec4 cam_pos_xyz_aspect_ratio_w;
};

#define cam_pos (cam_pos_xyz_aspect_ratio_w.xyz)
#define aspect_ratio (cam_pos_xyz_aspect_ratio_w.w)

out vec4 vs_out_colour;
out vec2 vs_out_size;

void main() {
    vs_out_colour = v_colour;
    float radius = v_radius;
    gl_Position = VP * vec4(v_position, 1.0);

#ifdef FADE_WHEN_CLOSE
    vec3 diff = v_position - cam_pos;
    float depth_sqr = dot(diff, diff);
    float r_sqr = radius * radius * 4.0;
    float fade = smoothstep(0.0, r_sqr, depth_sqr);
    radius *= min(1.0, fade * 2.0);
#else
    const float fade = 1.0;
#endif

    vs_out_colour.a *= fade;

#ifdef FIXED_SIZE
    gl_Position /= gl_Position.w;
    vs_out_size = vec2(radius / aspect_ratio, radius);
#else
    vs_out_size = (P * vec4(radius, radius, 0.0, 1.0)).xy;
#endif
}
)";

constexpr auto billboard_geometry_shader = R"(
layout(points) in;
layout(triangle_strip) out;
layout(max_vertices = 4) out;

in vec4 vs_out_colour[];
in vec2 vs_out_size[];
out vec4 gs_out_colour;
out vec2 tex_coord;

void main() {
    if (vs_out_colour[0].a == 0) { return; }

    gs_out_colour = vs_out_colour[0];

    gl_Position = gl_in[0].gl_Position + vec4(-vs_out_size[0].x, -vs_out_size[0].y, 0.0, 0.0);
    tex_coord = vec2(0.0, 1.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(vs_out_size[0].x, -vs_out_size[0].y, 0.0, 0.0);
    tex_coord = vec2(1.0, 1.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(-vs_out_size[0].x, vs_out_size[0].y, 0.0, 0.0);
    tex_coord = vec2(0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(vs_out_size[0].x, vs_out_size[0].y, 0.0, 0.0);
    tex_coord = vec2(1.0, 0.0);
    EmitVertex();

    EndPrimitive();
}
)";

constexpr auto billboard_fragment_shader = R"(
layout (location = 0) out vec4 frag_colour;
uniform sampler2D sampler_diffuse;

in vec4 gs_out_colour;
in vec2 tex_coord;

void main() {
   frag_colour = gs_out_colour * texture2D(sampler_diffuse, tex_coord);

#ifdef A_TEST
   if (frag_colour.a < 0.5) { discard; }
#else
   if (frag_colour.a == 0.0) { discard; }
#endif
}
)";


static ShaderProgram make_billboard_shader_program(span<std::string_view> defines)
{
    std::ostringstream oss;

    oss << "#version 330 core\n";
    for (auto&& define : defines) { oss << "#define " << define << "\n"; }
    oss << "#line 1\n";

    std::string prologue = oss.str();

    g_log.write_message("Compiling billboard vertex shader");
    oss << billboard_vertex_shader;
    auto vs = VertexShader::make(oss.str()).value();

    oss.str("");
    g_log.write_message("Compiling billboard geometry shader");
    oss << prologue << billboard_geometry_shader;
    auto gs = GeometryShader::make(oss.str()).value();

    oss.str("");
    g_log.write_message("Compiling billboard fragment shader");
    oss << prologue << billboard_fragment_shader;
    auto fs = FragmentShader::make(oss.str()).value();

    g_log.write_message("Linking billboard shader program");
    auto program = ShaderProgram::make(vs, gs, fs).value();

    opengl::use_program(program);
    opengl::set_uniform(opengl::uniform_location(program, "sampler_diffuse"), 0);

    return program;
}

//--------------------------------------------------------------------------------------------------
// BillboardRendererList implementation
//--------------------------------------------------------------------------------------------------

void BillboardRenderList::sort_farthest_first(const ICamera& camera)
{
    glm::vec3 cam_pos = camera.get_position();

    sort(m_billboards, [&](const Billboard& l, const Billboard& r) {
        return glm::distance2(cam_pos, l.pos) > glm::distance2(cam_pos, r.pos);
    });
}

//--------------------------------------------------------------------------------------------------
// BillboardRenderer implementation
//--------------------------------------------------------------------------------------------------

/** Uniform block for passing camera parameters to shader. */
struct CameraBlock {
    glm::mat4 VP;
    glm::mat4 P;
    glm::vec4 cam_pos_xyz_aspect_ratio_w;
};

/** Internal data for BillboardRenderer. */
struct BillboardRendererData {
    UniformBuffer camera_ubo{ sizeof(CameraBlock) };

    // Shader program -- mapped by options-bitflag
    std::unordered_map<uint8_t, ShaderProgram> programs;

    // Currently active shader
    ShaderProgram* shader = nullptr;

    // Size of VBO
    size_t vertex_buffer_size = 0;

    // OpenGL object ids
    ObjectId vbo;
    ObjectId vao;
};

// Update vertex buffer to match the new set of billboards
inline void update_buffer(BillboardRendererData& data, span<const Billboard> billboards)
{
    glBindBuffer(GL_ARRAY_BUFFER, data.vbo.value);

    // According to the following source, this should help reduce synchronisation overhead.
    // TODO: investigate further.
    // https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(data.vertex_buffer_size), nullptr, GL_STREAM_DRAW);

    data.vertex_buffer_size = billboards.size() * sizeof(Billboard);
    glBufferData(
        GL_ARRAY_BUFFER, GLsizeiptr(data.vertex_buffer_size), billboards.data(), GL_STREAM_DRAW);
}

// Set shader as required by the flags.
inline void set_shader(BillboardRendererData& data, uint8_t settings_flags)
{
    auto it = data.programs.find(settings_flags);

    if (it == data.programs.end()) {
        small_vector<std::string_view, 8> defines;

        if ((settings_flags & BillboardSetting::A_TEST) != 0) { defines.emplace_back("A_TEST"); }
        if ((settings_flags & BillboardSetting::FADE_WHEN_CLOSE) != 0) {
            defines.emplace_back("FADE_WHEN_CLOSE");
        }
        if ((settings_flags & BillboardSetting::FIXED_SIZE) != 0) {
            defines.emplace_back("FIXED_SIZE");
        }

        it = data.programs.emplace(settings_flags, make_billboard_shader_program(defines)).first;
        opengl::set_uniform_block_binding(it->second, "CameraBlock", k_camera_ubo_slot);
    }

    data.shader = &it->second;
    opengl::use_program(*data.shader);
}

BillboardRenderer::BillboardRenderer()
{
    // Create and configure vertex buffer.
    glGenVertexArrays(1, &data().vao.value);
    glBindVertexArray(data().vao.value);

    glGenBuffers(1, &data().vbo.value);
    glBindBuffer(GL_ARRAY_BUFFER, data().vbo.value);

    uint32_t      index  = 0;
    intptr_t      offset = 0;
    const int32_t stride = sizeof(Billboard);

    // Tell OpenGL how to interpret the vertex buffer.
    auto set_attrib_ptr = [&](GLint size) {
        glVertexAttribPointer(
            index, size, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<GLvoid*>(offset)); // NOLINT
        glEnableVertexAttribArray(index);
        offset += size * 4;
        ++index;
    };

    set_attrib_ptr(3); // pos
    set_attrib_ptr(4); // colour
    set_attrib_ptr(1); // radius

    glBindVertexArray(0);
}

BillboardRenderer::~BillboardRenderer()
{
    if (data().vao.value != 0) { glDeleteVertexArrays(1, &data().vao.value); }
    if (data().vbo.value != 0) { glDeleteBuffers(1, &data().vbo.value); }
}

void BillboardRenderer::render(const ICamera&             camera,
                               const BillboardRenderList& render_list,
                               TextureHandle              texture_handle,
                               BillboardSetting::Value    settings_flags)
{
    if (render_list.view().empty()) { return; }

    const auto& billboards = render_list.view();
    update_buffer(data(), billboards);

    set_shader(data(), settings_flags);
    internal::texture_node(texture_handle).texture.bind_to(TextureUnit(0));

    {
        CameraBlock camera_block;
        camera_block.VP = camera.view_proj_matrix();
        camera_block.P  = camera.proj_matrix();
        camera_block.cam_pos_xyz_aspect_ratio_w =
            glm::vec4(camera.get_position(), camera.aspect_ratio());

        data().camera_ubo.set_data(byte_representation(camera_block));
        data().camera_ubo.bind_to(k_camera_ubo_slot);
    }

    glBindVertexArray(data().vao.value);
    glDrawArrays(GL_POINTS, 0, narrow<GLint>(billboards.size()));
}

} // namespace Mg::gfx
