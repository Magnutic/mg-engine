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

#include "mg/gfx/mg_matrix_ubo.h"

#include <sstream>

#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_render_command_list.h"

#include "mg_render_command_data.h"

#include "mg_glad.h"

namespace Mg::gfx {

MatrixUniformHandler::MatrixUniformHandler()
    : m_matrix_storage(MATRIX_UBO_ARRAY_SIZE)
    , m_matrix_ubo(sizeof(Matrices_t) * MATRIX_UBO_ARRAY_SIZE)
{}

void MatrixUniformHandler::set_matrices(const ICamera&           camera,
                                        const RenderCommandList& drawlist,
                                        size_t                   starting_index)
{
    const glm::mat4 VP = camera.view_proj_matrix();

    for (size_t i = 0; i < m_matrix_storage.size() && i + starting_index < drawlist.size(); ++i) {
        auto& command_data      = internal::get_command_data(drawlist[i + starting_index].data);
        m_matrix_storage[i].M   = command_data.M;
        m_matrix_storage[i].MVP = VP * m_matrix_storage[i].M;
    }

    m_matrix_ubo.set_data(as_bytes(span<Matrices_t>{ m_matrix_storage }));
}

} // namespace Mg::gfx
