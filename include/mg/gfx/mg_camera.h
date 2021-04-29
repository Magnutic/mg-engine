//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_camera.h
 * 3D and othographic projection Camera classes.
 */

#pragma once

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "mg/core/mg_transform.h"
#include "mg/utils/mg_angle.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_point_normal_plane.h"

namespace Mg::gfx {

class DepthRange {
public:
    explicit DepthRange(float near, float far) noexcept : m_near(near), m_far(far)
    {
        MG_ASSERT(near > 0.0f && near < far);
    }

    float near() const noexcept { return m_near; }
    float far() const noexcept { return m_far; }

private:
    float m_near, m_far;
};

class ICamera {
public:
    MG_INTERFACE_BOILERPLATE(ICamera);

    virtual glm::mat4 proj_matrix() const = 0;
    virtual glm::mat4 view_matrix() const = 0;
    virtual glm::mat4 view_proj_matrix() const = 0;

    /** Get world space position of projection's origin. */
    virtual glm::vec3 get_position() const = 0;

    /** Get the shortest distance from camera's near plane to world space coordinate. */
    virtual float depth_at_point(glm::vec3 point) const = 0;

    virtual DepthRange depth_range() const = 0;

    virtual float aspect_ratio() const = 0;
};

/** 3D projection camera. */
class Camera final : public ICamera {
public:
    /** Construct a camera.
     * @param fov Field Of View angle, in degrees.
     * @param z_range Depth clipping range. */
    explicit Camera(Angle fov = 75_degrees,
                    float aspect_ratio = 4.0f / 3.0f,
                    DepthRange z_range = DepthRange{ 0.1f, 2000.0f }) noexcept
        : field_of_view(fov), m_depth_range{ z_range }
    {
        set_aspect_ratio(aspect_ratio);
    }

    Angle field_of_view;

    /** Set depth clipping range. */
    void set_depth_range(DepthRange z_range) noexcept { m_depth_range = z_range; }

    /** Get camera's depth range. */
    DepthRange depth_range() const noexcept override { return m_depth_range; }

    /** Set aspect ratio. */
    void set_aspect_ratio(float aspect_ratio) noexcept
    {
        MG_ASSERT(aspect_ratio > 0.0f);
        m_aspect = aspect_ratio;
    }

    float aspect_ratio() const noexcept override { return m_aspect; }

    glm::mat4 proj_matrix() const override
    {
        return glm::perspective(field_of_view.radians(),
                                m_aspect,
                                m_depth_range.near(),
                                m_depth_range.far());
    }

    glm::mat4 view_matrix() const override
    {
        // Camera should look toward rotation's forward vector
        Rotation r{ rotation };
        return glm::inverse(glm::translate(position) * r.pitch(90_degrees).to_matrix());
    }

    glm::mat4 view_proj_matrix() const noexcept override { return proj_matrix() * view_matrix(); }

    glm::vec3 get_position() const noexcept override { return position; }

    float depth_at_point(glm::vec3 point) const noexcept override
    {
        const auto p{ PointNormalPlane::from_point_and_normal(position, rotation.forward()) };
        return signed_distance_to_plane(p, point);
    }

    /** Camera's position. */
    glm::vec3 position{};

    /** Camera's orientation. */
    Rotation rotation{};

private:
    float m_aspect{};
    DepthRange m_depth_range;
};

/** Orthographic projection camera. */
class OrthoCamera final : public ICamera {
public:
    explicit OrthoCamera() noexcept : min{ -0.5f }, max{ 0.5f } {}

    explicit OrthoCamera(glm::vec3 _min, glm::vec3 _max) noexcept : min{ _min }, max{ _max } {}

    glm::mat4 proj_matrix() const noexcept override
    {
        return glm::orthoRH(min.x, max.x, min.y, max.y, min.z, max.z);
    }

    glm::mat4 view_matrix() const noexcept override { return glm::mat4{}; }

    glm::mat4 view_proj_matrix() const noexcept override { return proj_matrix(); }

    DepthRange depth_range() const noexcept override { return DepthRange{ min.z, max.z }; }

    float aspect_ratio() const noexcept override { return (max.x - min.x) / (max.y - min.y); }

    glm::vec3 get_position() const noexcept override
    {
        return { min.x + max.x * 0.5f, min.y + max.y * 0.5f, max.z };
    }

    float depth_at_point(glm::vec3 point) const noexcept override
    {
        return glm::abs(point.z - min.z);
    }

    glm::vec3 min;
    glm::vec3 max;
};

} // namespace Mg::gfx
