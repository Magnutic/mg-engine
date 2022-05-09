//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_physics_debug_renderer.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_debug_renderer.h"

namespace Mg::physics {

namespace {

glm::mat4 transform_bullet_to_glm(const btTransform& transform)
{
    glm::mat4 result;
    transform.getOpenGLMatrix(&result[0][0]);
    return result;
}

glm::vec3 vector_bullet_to_glm(const btVector3& vector)
{
    return { vector.x(), vector.y(), vector.z() };
}

gfx::DebugRenderer::BoxDrawParams
box_params(const btVector3& bbMin, const btVector3& bbMax, const btVector3& color)
{
    gfx::DebugRenderer::BoxDrawParams params = {};
    params.centre = vector_bullet_to_glm((bbMin + bbMax) / 2.0f);
    params.dimensions = { params.centre.x - bbMin.x(),
                          params.centre.y - bbMin.y(),
                          params.centre.z - bbMin.z() };
    params.colour = { color.x(), color.y(), color.z(), 1.0f };
    params.wireframe = true;
    return params;
}

} // namespace

void PhysicsDebugRenderer::drawLine(const btVector3& from,
                                    const btVector3& to,
                                    const btVector3& color)
{
    // This is staggeringly inefficient for large debug geometries. It will do for now, but
    // debugging complex scenes may be difficult if it turns into a slide show.
    m_debug_renderer->draw_line(*m_render_target,
                                m_view_proj,
                                vector_bullet_to_glm(from),
                                vector_bullet_to_glm(to),
                                { color.x(), color.y(), color.z(), 1.0f });
}

void PhysicsDebugRenderer::drawSphere(const btVector3& p, btScalar radius, const btVector3& color)
{
    gfx::DebugRenderer::EllipsoidDrawParams params = {};
    params.centre = vector_bullet_to_glm(p);
    params.dimensions = { radius, radius, radius };
    params.colour = { color.x(), color.y(), color.z(), 1.0f };
    params.wireframe = true;
    m_debug_renderer->draw_ellipsoid(*m_render_target, m_view_proj, params);
}

void PhysicsDebugRenderer::drawBox(const btVector3& bbMin,
                                   const btVector3& bbMax,
                                   const btVector3& color)
{
    m_debug_renderer->draw_box(*m_render_target, m_view_proj, box_params(bbMin, bbMax, color));
}

void PhysicsDebugRenderer::drawBox(const btVector3& bbMin,
                                   const btVector3& bbMax,
                                   const btTransform& trans,
                                   const btVector3& color)
{
    m_debug_renderer->draw_box(*m_render_target,
                               m_view_proj * transform_bullet_to_glm(trans),
                               box_params(bbMin, bbMax, color));
}

void PhysicsDebugRenderer::drawTriangle(const btVector3& v0,
                                        const btVector3& v1,
                                        const btVector3& v2,
                                        const btVector3& color,
                                        btScalar /*alpha*/)
{
    std::array<glm::vec3, 4> vertices = {
        vector_bullet_to_glm(v0),
        vector_bullet_to_glm(v1),
        vector_bullet_to_glm(v2),
        vector_bullet_to_glm(v0),
    };

    m_debug_renderer->draw_line(*m_render_target,
                                m_view_proj,
                                vertices,
                                { color.x(), color.y(), color.z(), 1.0f });
}

void PhysicsDebugRenderer::reportErrorWarning(const char* warningString)
{
    log.warning("PhysicsDebugRenderer received warning: {}", warningString);
}

} // namespace Mg::physics
