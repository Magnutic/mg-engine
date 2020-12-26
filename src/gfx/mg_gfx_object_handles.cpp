//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_gfx_object_handles.h"

#include "opengl/mg_glad.h"

namespace Mg::gfx {

namespace {

using DeleteFunction = void (*)(GLsizei, const GLuint*);
using DeleteFunctionAlt = void (*)(GLuint);

void delete_gl_object(DeleteFunction delete_function, const GfxObjectHandleValue value)
{
    auto v = static_cast<GLuint>(value);
    delete_function(1, &v);
}

void delete_gl_object(DeleteFunctionAlt delete_function, const GfxObjectHandleValue value)
{
    delete_function(static_cast<GLuint>(value));
}

} // namespace

template<> void GfxObjectHandle<GfxObjectType::vertex_array>::_free_impl(Value value)
{
    delete_gl_object(glDeleteVertexArrays, value);
}
template<> void GfxObjectHandle<GfxObjectType::buffer>::_free_impl(Value value)
{
    delete_gl_object(glDeleteBuffers, value);
}
template<> void GfxObjectHandle<GfxObjectType::texture>::_free_impl(Value value)
{
    delete_gl_object(glDeleteTextures, value);
}
template<> void GfxObjectHandle<GfxObjectType::uniform_buffer>::_free_impl(Value value)
{
    delete_gl_object(glDeleteBuffers, value);
}
template<> void GfxObjectHandle<GfxObjectType::vertex_shader>::_free_impl(Value value)
{
    delete_gl_object(glDeleteShader, value);
}
template<> void GfxObjectHandle<GfxObjectType::geometry_shader>::_free_impl(Value value)
{
    delete_gl_object(glDeleteShader, value);
}
template<> void GfxObjectHandle<GfxObjectType::fragment_shader>::_free_impl(Value value)
{
    delete_gl_object(glDeleteShader, value);
}
template<> void GfxObjectHandle<GfxObjectType::pipeline>::_free_impl(Value value)
{
    delete_gl_object(glDeleteProgram, value);
}

} // namespace Mg::gfx
