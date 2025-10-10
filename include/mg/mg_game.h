//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_game.h
 * .
 */

#pragma once

#include "mg/components/mg_advance_animations.h"
#include "mg/components/mg_animation_component.h"
#include "mg/components/mg_dynamic_body_component.h"
#include "mg/components/mg_mesh_component.h"
#include "mg/components/mg_static_body_component.h"
#include "mg/components/mg_transform_component.h"
#include "mg/components/mg_update_dynamic_body_transforms.h"
#include "mg/core/ecs/mg_entity.h"
#include "mg/core/gfx/mg_debug_renderer.h"
#include "mg/core/gfx/mg_material_pool.h"
#include "mg/core/gfx/mg_mesh_pool.h"
#include "mg/core/gfx/mg_scene_renderer.h"
#include "mg/core/gfx/mg_texture_pool.h"
#include "mg/core/gfx/render_passes/mg_mesh_pass.h"
#include "mg/core/mg_application_context.h"
#include "mg/core/mg_config.h"
#include "mg/core/mg_file_loader.h"
#include "mg/core/mg_window.h"
#include "mg/core/physics/mg_dynamic_body_handle.h"
#include "mg/core/physics/mg_physics_world.h"
#include "mg/core/resources/mg_mesh_resource.h"

namespace Mg {

struct GameParams {
    std::string window_title;
    std::string_view config_file_path;
    std::vector<std::unique_ptr<IFileLoader>> file_loaders;
    uint32_t max_num_entities = 1024;
};

struct LoadModelParams {
    Identifier mesh_file;
    std::initializer_list<std::pair<Identifier, Identifier>> material_bindings;
};

struct TransformParams {
    glm::vec3 position{ 0.0f };
    Rotation rotation;
    glm::vec3 scale{ 1.0f };
};

class Game : public IApplication {
public:
    template<ecs::Component... AdditionalComponentTs>
    explicit Game(GameParams params)
        : m_config{ params.config_file_path }
        , m_window{ read_display_settings(m_config), std::move(params.window_title) }
        , m_resource_cache{ std::make_shared<ResourceCache>(std::move(params.file_loaders)) }
        , m_mesh_pool{ std::make_shared<gfx::MeshPool>(m_resource_cache) }
        , m_texture_pool{ std::make_shared<gfx::TexturePool>(m_resource_cache) }
        , m_material_pool{ std::make_shared<gfx::MaterialPool>(m_resource_cache, m_texture_pool) }
        , m_entities{ params.max_num_entities }
    {
        m_entities.init<TransformComponent,
                        StaticBodyComponent,
                        DynamicBodyComponent,
                        MeshComponent,
                        AnimationComponent,
                        AdditionalComponentTs...>();

        m_window.set_cursor_lock_mode(CursorLockMode::LOCKED);
        m_window.set_focus_callback(
            [this](bool is_focused) { on_window_focus_change(is_focused); });
    }

    void simulation_step(ApplicationTimeInfo time_info) final
    {
        const auto timer_config = update_timer_config();
        const auto time_step =
            narrow_cast<float>(1.0 / double(timer_config.simulation_steps_per_second));

        gfx::get_debug_render_queue().clear();

        m_window.poll_input_events();

        update_dynamic_body_transforms(m_entities);

        on_simulation_step(time_info);

        m_physics_world->update(time_step);
    }

    void render(double lerp_factor, ApplicationTimeInfo time_info) final
    {
        advance_animations(m_entities, float(time_info.time_since_init));

        on_render(lerp_factor, time_info);

        renderer().render({
            .camera = active_camera(),
            .time_since_init = float(time_info.time_since_init),
        });
        window().swap_buffers();
    }

    void load_model(const LoadModelParams& params, ecs::Entity entity)
    {
        auto& mesh = m_entities.add_component<MeshComponent>(entity);
        mesh.mesh = m_mesh_pool->get_or_load(params.mesh_file);

        if (mesh.mesh->animation_data) {
            auto& animation = m_entities.add_component<AnimationComponent>(entity);
            animation.pose = mesh.mesh->animation_data->skeleton.get_bind_pose();
        }

        mesh.material_bindings.resize(params.material_bindings.size());
        for (auto&& [binding_id, filename] : params.material_bindings) {
            mesh.material_bindings.push_back(
                { .material_binding_id = binding_id,
                  .material = m_material_pool->get_or_load(filename) });
        }

        for (auto&& submesh : mesh.mesh->submeshes) {
            auto has_material = [&](const gfx::MaterialBinding& binding) {
                return binding.material_binding_id == submesh.material_binding_id;
            };
            if (std::ranges::find_if(mesh.material_bindings, has_material) ==
                mesh.material_bindings.end()) {
                log.warning("Submesh '{}' with material_binding_id '{}' has no material assigned.",
                            submesh.name.str_view(),
                            submesh.material_binding_id.str_view());
            }
        }
    }

    ecs::Entity add_static_object(const LoadModelParams& params,
                                  const TransformParams& transform_params)
    {
        const auto entity = m_entities.create_entity();
        load_model(params, entity);

        const ResourceAccessGuard access =
            m_resource_cache->access_resource<MeshResource>(params.mesh_file);

        physics::Shape* shape = m_physics_world->create_mesh_shape(access->data_view());

        glm::mat4 M = glm::translate(transform_params.position) *
                      transform_params.rotation.to_matrix() * glm::scale(transform_params.scale);

        auto static_body = m_physics_world->create_static_body(params.mesh_file, *shape, M);
        m_entities.add_component<StaticBodyComponent>(entity, static_body);
        m_entities.add_component<TransformComponent>(entity, M, M);

        return entity;
    }

    ecs::Entity add_dynamic_object(Identifier object_id,
                                   const LoadModelParams& model_params,
                                   const TransformParams& transform_params,
                                   Opt<physics::DynamicBodyParameters> physics_params)
    {
        const auto entity = m_entities.create_entity();
        load_model(model_params, entity);

        auto& mesh = m_entities.get_component<MeshComponent>(entity);
        auto& transform = m_entities.add_component<TransformComponent>(entity);

        const glm::mat4 M = glm::translate(transform_params.position) *
                            transform_params.rotation.to_matrix();

        if (physics_params.has_value()) {
            const ResourceAccessGuard access =
                m_resource_cache->access_resource<MeshResource>(model_params.mesh_file);

            const glm::vec3 centre = access->bounding_sphere().centre;

            physics::Shape* shape = m_physics_world->create_convex_hull(access->vertices(),
                                                                        centre,
                                                                        transform_params.scale);

            m_entities.add_component<DynamicBodyComponent>(
                entity,
                m_physics_world->create_dynamic_body(object_id, *shape, physics_params.value(), M));

            // Add visualization translation relative to centre of mass.
            // Note unusual order: for once we translate before the scale, since the translation is
            // in model space, not world space.
            mesh.mesh_transform = glm::scale(transform_params.scale) * glm::translate(-centre);
        }
        else {
            transform.transform = M;
            transform.previous_transform = M;
            mesh.mesh_transform = glm::scale(transform_params.scale);
        }

        return entity;
    }

    Window& window() { return m_window; }
    const Window& window() const { return m_window; }
    Config& config() { return m_config; }
    const Config& config() const { return m_config; }

    physics::PhysicsWorld& physics_world() { return *m_physics_world; }
    const physics::PhysicsWorld& physics_world() const { return *m_physics_world; }

    const std::shared_ptr<ResourceCache>& resource_cache() const { return m_resource_cache; }
    const std::shared_ptr<gfx::MeshPool>& mesh_pool() const { return m_mesh_pool; }
    const std::shared_ptr<gfx::TexturePool>& texture_pool() const { return m_texture_pool; }
    const std::shared_ptr<gfx::MaterialPool>& material_pool() const { return m_material_pool; }

    ecs::EntityCollection& entities() { return m_entities; }
    const ecs::EntityCollection& entities() const { return m_entities; }

    gfx::SceneLights& scene_lights() { return *m_scene_lights; }
    const gfx::SceneLights& scene_lights() const { return *m_scene_lights; }

    virtual const gfx::ICamera& active_camera() const = 0;
    virtual gfx::SceneRenderer& renderer() = 0;

    virtual void on_simulation_step(const ApplicationTimeInfo& time_info) = 0;
    virtual void on_render(double lerp_factor, const ApplicationTimeInfo& time_info) = 0;

private:
    void on_window_focus_change(const bool is_focused)
    {
        if (is_focused) {
            m_resource_cache->refresh();
        }
    }

    Config m_config;
    Window m_window;

    std::shared_ptr<ResourceCache> m_resource_cache;
    std::shared_ptr<gfx::MeshPool> m_mesh_pool;
    std::shared_ptr<gfx::TexturePool> m_texture_pool;
    std::shared_ptr<gfx::MaterialPool> m_material_pool;

    std::unique_ptr<physics::PhysicsWorld> m_physics_world =
        std::make_unique<physics::PhysicsWorld>();

    std::shared_ptr<gfx::SceneLights> m_scene_lights = std::make_shared<gfx::SceneLights>();

    ecs::EntityCollection m_entities;
};

} // namespace Mg
