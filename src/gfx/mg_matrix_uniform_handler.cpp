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
