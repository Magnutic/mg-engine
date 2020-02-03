//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_matrix_uniform_handler.h
 * Utility for renderers; passing transformation matrices to shader using a uniform buffer object.
 */

#pragma once

#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/utils/mg_gsl.h"

#include <glm/fwd.hpp>

namespace Mg::gfx {

#ifndef MATRIX_UBO_ARRAY_SIZE
// Corresponds to guaranteed lower bound of GL_MAX_UNIFORM_BLOCK_SIZE.
#    define MATRIX_UBO_ARRAY_SIZE 128
#endif

class ICamera;

/** Handles UBO for transformation matrices.
 * Passes transformation matrices to a shader.
 *
 * The shader must declare a uniform block consisting of an two arrays of length
 * MATRIX_UBO_ARRAY_SIZE of matrix structs, e.g.:
 * @code
 *     layout(std140) uniform MatrixBlock {
 *         mat4 m_matrices[MATRIX_UBO_ARRAY_SIZE];
 *         mat4 mvp_matri:es[MATRIX_UBO_ARRAY_SIZE];
 *     } mat_block;
 * @endcode
 */
class MatrixUniformHandler {
public:
    /** Constructs new UBO for matrices. */
    MatrixUniformHandler();

    /** Set matrix UBO data to hold transformation matrices.
     * m_matrices and mvp_matrices should be equally long.
     * Note that UBO size may be limited -- in this case, as much of the input as possible is set.
     * @return The number of matrices passed into the UBO (i.e. min(MATRIX_UBO_ARRAY_SIZE,
     * m_matrices.size())
     */
    size_t set_matrices(span<const glm::mat4> m_matrices, span<const glm::mat4> mvp_matrices);

    /** Get matrix UBO. */
    const UniformBuffer& ubo() const noexcept { return m_matrix_ubo; }

private:
    UniformBuffer m_matrix_ubo;
};

} // namespace Mg::gfx
