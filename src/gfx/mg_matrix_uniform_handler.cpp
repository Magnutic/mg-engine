//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_matrix_uniform_handler.h"

#include "mg/gfx/mg_camera.h"
#include "mg/utils/mg_assert.h"

#include <algorithm>

namespace Mg::gfx {

MatrixUniformHandler::MatrixUniformHandler()
    : m_matrix_ubo(sizeof(glm::mat4) * MATRIX_UBO_ARRAY_SIZE * 2)
{}

size_t MatrixUniformHandler::set_matrices(span<const glm::mat4> m_matrices,
                                          span<const glm::mat4> mvp_matrices)
{
    MG_ASSERT(m_matrices.size() == mvp_matrices.size());

    const size_t num_to_write = std::min<size_t>(MATRIX_UBO_ARRAY_SIZE, m_matrices.size());

    // Write M-matrices to GPU buffer.
    auto m_matrices_to_write = span{ m_matrices }.subspan(0, num_to_write);
    m_matrix_ubo.set_data(as_bytes(m_matrices_to_write));

    // Write MVP-matrices to GPU buffer following the M-matrices.
    constexpr size_t k_mvp_dest_offset = sizeof(glm::mat4) * MATRIX_UBO_ARRAY_SIZE;
    m_matrix_ubo.set_data(as_bytes(span{ mvp_matrices }), k_mvp_dest_offset);
    return num_to_write;
}

} // namespace Mg::gfx
