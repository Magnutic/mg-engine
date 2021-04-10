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

class ICamera;

/** Handles a uniform buffer for matrices, for efficiently passing such data to a shader. */
class MatrixUniformHandler {
public:
    /** Constructs new UBO for matrices. The parameters define the layout of matrices in the shader.
     * @param num_matrices_per_array Number of matrices in each matrix array.
     * @param num_matrix_arrays Number of matrix arrays in the uniform block.
     *
     * For example, given the following GLSL uniform block definition, `num_matrices_per_array`
     * should be 128, since that is the size of `m_matrices` and `mvp_matrices`, and
     * `num_matrix_arrays` should be 2, since there are two arrays:
     *
     * @code
     *     layout(std140) uniform MatrixBlock {
     *         mat4 m_matrices[128];
     *         mat4 mvp_matrices[128];
     *     } mat_block;
     * @endcode
     */
    explicit MatrixUniformHandler(size_t num_matrices_per_array, size_t num_matrix_arrays);

    /** Set matrix UBO data to hold given transformation matrix arrays.
     * All matrix arrays should be equally long.
     * Note that UBO size may be limited -- in this case, as much of the input as possible is set.
     * @param matrix_array span of spans of matrices; number of arrays should match
     * num_matrix_arrays().
     * @return The number of matrices passed into the UBO.
     */
    size_t set_matrix_arrays(span<const span<const glm::mat4>> matrix_arrays);

    /** Set matrix UBO data to hold given transformation matrix array.
     * Single-array overload for the case of num_matrix_arrays == 1.
     * Note that UBO size may be limited -- in this case, as much of the input as possible is set.
     * @return The number of matrices passed into the UBO.
     */
    size_t set_matrix_array(span<const glm::mat4> matrix_array)
    {
        MG_ASSERT(m_num_matrix_arrays == 1);
        return set_matrix_arrays({ &matrix_array, 1 });
    }

    /** Get matrix UBO. */
    const UniformBuffer& ubo() const noexcept { return m_matrix_ubo; }

    size_t num_matrices_per_array() const noexcept { return m_num_matrices_per_array; }
    size_t num_matrix_arrays() const noexcept { return m_num_matrix_arrays; }

private:
    UniformBuffer m_matrix_ubo;
    size_t m_num_matrices_per_array;
    size_t m_num_matrix_arrays;
};

} // namespace Mg::gfx
