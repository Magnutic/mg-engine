//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_physics_debug_renderer.h
 * Adapter for Mg::gfx::DebugRenderer to render physics debug visualization.
 */

#pragma once

#include <LinearMath/btIDebugDraw.h>

#include <glm/mat4x4.hpp>

namespace Mg::gfx {
class DebugRenderQueue;
} // namespace Mg::gfx

namespace Mg::physics {

/** This is an implementation of Bullet's btIDebugDraw interface, and acts as a proxy for Mg
 * Engine's DebugRenderer.
 * @note should be short-lived (created and used on function stack), since it keeps raw pointers to
 * the render target, debug renderer, and camera.
 */
class PhysicsDebugRenderer : public btIDebugDraw {
public:
    explicit PhysicsDebugRenderer(gfx::DebugRenderQueue& render_queue)
        : m_render_queue(&render_queue)
    {}

    void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;

    void drawSphere(const btVector3& p, btScalar radius, const btVector3& color) override;

    void drawBox(const btVector3& bbMin, const btVector3& bbMax, const btVector3& color) override;

    void drawBox(const btVector3& bbMin,
                 const btVector3& bbMax,
                 const btTransform& trans,
                 const btVector3& color) override;

    void drawTriangle(const btVector3& v0,
                      const btVector3& v1,
                      const btVector3& v2,
                      const btVector3& color,
                      btScalar /*alpha*/) override;

    void drawContactPoint(const btVector3& /*PointOnB*/,
                          const btVector3& /*normalOnB*/,
                          btScalar /*distance*/,
                          int /*lifeTime*/,
                          const btVector3& /*color*/) override
    {
        // Unimplemented
    }

    void reportErrorWarning(const char* warningString) override;

    void draw3dText(const btVector3& /*location*/, const char* /*textString*/) override
    {
        // Unimplemented
    }

    void setDebugMode(int debugMode) override { m_debug_mode = debugMode; }

    int getDebugMode() const override { return m_debug_mode; }

private:
    gfx::DebugRenderQueue* m_render_queue;
    int m_debug_mode = btIDebugDraw::DBG_DrawAabb | btIDebugDraw::DBG_DrawWireframe;
};

} // namespace Mg::physics
