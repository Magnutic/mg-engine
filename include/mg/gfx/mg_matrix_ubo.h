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

/** @file mg_matrix_ubo.h
 * Utility for renderers; passing transformation matrices to shader using a Uniform Buffer Object.
 */

#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

#include "mg/gfx/mg_uniform_buffer.h"

namespace Mg::gfx {

class RenderCommandList;

#ifndef MATRIX_UBO_ARRAY_SIZE
// Corresponds to guaranteed lower bound of GL_MAX_UNIFORM_BLOCK_SIZE.
#    define MATRIX_UBO_ARRAY_SIZE 128
#endif

class ICamera;

/** Handles UBO for transformation matrices.
 * Constructs transformation matrices from draw calls.
 *
 * The shader must declare a uniform block consisting of an array of length MATRIX_UBO_ARRAY_SIZE of
 * matrix structs, e.g.:
 * @code
 *     struct matrices_t { mat4 MVP; mat4 M; };
 *     layout(std140) uniform MatrixBlock { matrices_t matrices[MATRIX_UBO_ARRAY_SIZE]; } mat_block;
 * @endcode
 */
class MatrixUniformHandler {
public:
    /** Constructs new UBO for matrices. */
    explicit MatrixUniformHandler();

    /** Set matrix ubo data to hold transformation matrices for the given camera and drawcall list.
     * Note that UBO size may be limited -- in this case, as much of the input as possible is set
     * (starting at starting_index).
     */
    void
    set_matrices(const ICamera& camera, const RenderCommandList& drawlist, size_t starting_index);

    /** Get matrix UBO. */
    const UniformBuffer& ubo() const { return m_matrix_ubo; }

private:
    /** Transform-matrices layout. */
    struct Matrices_t {
        glm::mat4 MVP;
        glm::mat4 M;
    };

    std::vector<Matrices_t> m_matrix_storage;
    UniformBuffer           m_matrix_ubo;
};

} // namespace Mg::gfx
