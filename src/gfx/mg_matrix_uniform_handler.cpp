//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_matrix_uniform_handler.h"

#include "mg/gfx/mg_camera.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_math_utils.h"

namespace Mg::gfx {

MatrixUniformHandler::MatrixUniformHandler(const size_t num_matrices_per_array,
                                           const size_t num_matrix_arrays)
    : m_matrix_ubo(sizeof(glm::mat4) * num_matrices_per_array * num_matrix_arrays)
    , m_num_matrices_per_array(num_matrices_per_array)
    , m_num_matrix_arrays(num_matrix_arrays)
{}

size_t
MatrixUniformHandler::set_matrix_arrays(const std::span<const std::span<const glm::mat4>> matrix_arrays)
{
    MG_ASSERT(matrix_arrays.size() == m_num_matrix_arrays);

    size_t destination_offset = 0;
    const size_t num_to_write = min(m_num_matrices_per_array, matrix_arrays[0].size());

    for (const auto& matrix_array : matrix_arrays) {
        MG_ASSERT(matrix_array.size() >= num_to_write);
        const auto matrices_to_write = matrix_array.subspan(0, num_to_write);
        m_matrix_ubo.set_data(as_bytes(matrices_to_write), destination_offset);
        destination_offset += sizeof(glm::mat4) * m_num_matrices_per_array;
    }

    return num_to_write;
}

} // namespace Mg::gfx
