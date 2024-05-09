#include "catch.hpp"

#include <mg/ecs/mg_entity.h>

#include <utility>
#include <vector>

struct TestComponent : Mg::ecs::BaseComponent<1> {
    uint32_t value = 0;
    std::string string = "init value";
};

struct Position : Mg::ecs::BaseComponent<2> {
    float x = 0.0f;
    float y = 0.0f;
};

struct IndexComponent : Mg::ecs::BaseComponent<3> {
    uint32_t index{ 0 };
};

TEST_CASE("Entity")
{
    constexpr static size_t num_elems = 8192;
    Mg::ecs::EntityCollection entity_collection{ num_elems };
    entity_collection.init<TestComponent, Position, IndexComponent>();

    SECTION("constructible") {}

    SECTION("create_entity")
    {
        for (size_t i = 0; i < 1024; ++i) {
            [[maybe_unused]] auto handle = entity_collection.create_entity();
            REQUIRE(entity_collection.num_entities() - 1 == i);
        }
    }

    SECTION("remove_entity")
    {
        std::vector<Mg::ecs::Entity> handles;

        for (size_t i = 0; i < 1024; ++i) {
            handles.push_back(entity_collection.create_entity());
        }

        for (auto& h : handles) {
            entity_collection.delete_entity(h);
        }

        REQUIRE(entity_collection.num_entities() == 0);
    }

    SECTION("add_component")
    {
        auto handle = entity_collection.create_entity();
        entity_collection.add_component<TestComponent>(handle, 123u, "testing");
        REQUIRE(entity_collection.get_component<TestComponent>(handle).string == "testing");
        REQUIRE(entity_collection.get_component<TestComponent>(handle).value == 123);
    }

    SECTION("remove_component")
    {
        auto handle = entity_collection.create_entity();
        entity_collection.add_component<TestComponent>(handle, 123u, "testing");
        entity_collection.remove_component<TestComponent>(handle);
        REQUIRE(entity_collection.has_component<TestComponent>(handle) == false);
    }

    SECTION("combined_test")
    {
        auto handle0 = entity_collection.create_entity();
        entity_collection.add_component<TestComponent>(handle0, 1u, "testcomponent1");

        auto handle1 = entity_collection.create_entity();
        entity_collection.add_component<TestComponent>(handle1, 2u, "testcomponent2");
        entity_collection.add_component<Position>(handle1, 4.0f, 2.0f);

        auto handle2 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle2, 123.0f, 321.0f);

        auto& component = entity_collection.get_component<TestComponent>(handle0);
        std::string member_copy = component.string;
        entity_collection.delete_entity(handle0);

        auto& component2 = entity_collection.get_component<TestComponent>(handle1);
        member_copy = component2.string;
        REQUIRE(member_copy == "testcomponent2");
        REQUIRE(entity_collection.get_component<TestComponent>(handle1).string == member_copy);

        CHECK(entity_collection.has_component<TestComponent>(handle1));
        CHECK(entity_collection.has_component<Position>(handle1));

        auto& h3p = entity_collection.get_component<Position>(handle2);
        REQUIRE(h3p.x == 123.0f);
        REQUIRE(h3p.y == 321.0f);

        CHECK(entity_collection.has_component<Position>(handle2));
        CHECK(!entity_collection.has_component<TestComponent>(handle2));
    }

    SECTION("component_iteration")
    {
        auto handle0 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle0, 2.0f, 2.0f);
        entity_collection.add_component<TestComponent>(handle0, 0u, "handle0");

        auto handle1 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle1, 3.0f, 3.0f);
        entity_collection.add_component<TestComponent>(handle1, 1u, "handle1");

        auto handle2 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle2, 4.0f, 4.0f);

        auto handle3 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle3, 5.0f, 5.0f);
        entity_collection.add_component<TestComponent>(handle3, 2u, "handle3");

        for (auto [entity, test_component, position] :
             entity_collection.get_with<TestComponent, Position>()) {
            static_assert(std::is_reference_v<decltype(test_component)>);
            static_assert(std::is_reference_v<decltype(position)>);

            CHECK(entity_collection.has_component<TestComponent>(entity));
            CHECK(entity_collection.has_component<Position>(entity));

            auto& str = test_component.string;
            CHECK(str.find("handle") != str.npos);
        }
    }

    SECTION("component_iteration with Not<>")
    {
        auto handle0 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle0, 2.0f, 2.0f);
        entity_collection.add_component<TestComponent>(handle0, 0u, "handle0");

        auto handle1 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle1, 3.0f, 3.0f);
        entity_collection.add_component<TestComponent>(handle1, 1u, "handle1");

        auto handle2 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle2, 4.0f, 4.0f);

        auto handle3 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle3, 5.0f, 5.0f);
        entity_collection.add_component<TestComponent>(handle3, 2u, "handle3");

        for (auto [entity, test_component] :
             entity_collection.get_with<TestComponent, Mg::ecs::Not<Position>>()) {
            static_assert(std::is_reference_v<decltype(test_component)>);

            CHECK(entity_collection.has_component<TestComponent>(entity));
            CHECK(!entity_collection.has_component<Position>(entity));

            auto& str = test_component.string;
            CHECK(str.find("handle") != str.npos);
        }
    }

    SECTION("component_iteration with Maybe<>")
    {
        auto handle0 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle0, 2.0f, 2.0f);
        entity_collection.add_component<TestComponent>(handle0, 0u, "handle0");

        auto handle1 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle1, 3.0f, 3.0f);
        entity_collection.add_component<TestComponent>(handle1, 1u, "handle1");

        auto handle2 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle2, 4.0f, 4.0f);

        auto handle3 = entity_collection.create_entity();
        entity_collection.add_component<Position>(handle3, 5.0f, 5.0f);
        entity_collection.add_component<TestComponent>(handle3, 2u, "handle3");

        size_t num_test_components = 0;

        for (auto [entity, test_component, position] :
             entity_collection.get_with<Mg::ecs::Maybe<TestComponent>, Position>()) {
            CHECK(entity_collection.has_component<Position>(entity));

            static_assert(std::is_pointer_v<decltype(test_component)>);
            static_assert(std::is_reference_v<decltype(position)>);

            if (test_component != nullptr) {
                ++num_test_components;
                auto& str = test_component->string;
                CHECK(str.find("handle") != str.npos);
            }
        }

        CHECK(num_test_components == 3);
    }

    SECTION("maximum capacity")
    {
        std::array<Mg::ecs::Entity, num_elems> es;

        // Fill EntityCollection to its maximum capacity.
        for (uint32_t i = 0; i < num_elems; ++i) {
            es[i] = entity_collection.create_entity();
            entity_collection.add_component<IndexComponent>(es[i], i);
        }

        // Verify each component is reachable through its entity handle.
        for (uint32_t i = 0; i < num_elems; ++i) {
            auto& d = entity_collection.get_component<IndexComponent>(es[i]);
            REQUIRE(d.index == i);
        }

        // Modify each component
        for (auto cs : entity_collection.get_with()) {
            auto entity = std::get<Mg::ecs::Entity>(cs);
            CHECK(entity_collection.has_component<IndexComponent>(entity));

            auto& d = entity_collection.get_component<IndexComponent>(entity);
            d.index = ~d.index;
        }

        // Ensure modifications took place
        for (uint32_t i = 0; i < num_elems; ++i) {
            auto& d = entity_collection.get_component<IndexComponent>(es[i]);
            REQUIRE(d.index == ~i);
        }

        // Check that all entities have the expected component mask.
        constexpr auto expected_mask = Mg::ecs::ComponentMask{ 1 }
                                       << IndexComponent::component_type_id;
        for (auto cs : entity_collection.get_with()) {
            auto entity = std::get<Mg::ecs::Entity>(cs);
            auto mask = entity_collection.component_mask(entity);
            CHECK(mask == expected_mask);
        }
    }
} // TEST_CASE("Entity")
