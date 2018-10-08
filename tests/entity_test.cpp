#include "catch.hpp"

#include <utility>
#include <vector>

#include <mg/core/mg_log.h>
#include <mg/core/mg_root.h>
#include <mg/ecs/mg_entity.h>

MG_DEFINE_COMPONENT(TestComponent)
{
    TestComponent() = default;
    TestComponent(uint32_t val, std::string str) : value(val), string(std::move(str)) {}

    uint32_t    value  = 0;
    std::string string = "Hello I am a string :D";
};

MG_DEFINE_COMPONENT(Position)
{
    Position(float _x, float _y) : x(_x), y(_y) {}
    Position() = default;

    float x = 0.0f, y = 0.0f;
};

MG_DEFINE_COMPONENT(IndexPairComponent)
{
    IndexPairComponent() = default;
    IndexPairComponent(uint32_t _elem1, uint32_t _elem2) : elem1(_elem1), elem2(_elem2) {}

    uint32_t elem1{ 0 }, elem2{ 0 };
};

TEST_CASE("Entity")
{
    constexpr static size_t   num_elems = 8192;
    Mg::ecs::EntityCollection ent_col{ num_elems };

    SECTION("constructible") {}

    SECTION("create_entity")
    {
        for (size_t i = 0; i < 1024; ++i) {
            ent_col.create_entity();
            REQUIRE(ent_col.num_entities() - 1 == i);
        }
    }

    SECTION("remove_entity")
    {
        std::vector<Mg::ecs::Entity> handles;

        for (size_t i = 0; i < 1024; ++i) {
            handles.push_back(ent_col.create_entity());
        }

        for (auto& h : handles) {
            ent_col.delete_entity(h);
        }

        REQUIRE(ent_col.num_entities() == 0);
    }

    SECTION("add_component")
    {
        auto handle = ent_col.create_entity();
        ent_col.add_component<TestComponent>(handle, 42u, "forty-two");
        REQUIRE(ent_col.get_component<TestComponent>(handle).string == "forty-two");
        REQUIRE(ent_col.get_component<TestComponent>(handle).value == 42);
    }

    SECTION("remove_component")
    {
        auto handle = ent_col.create_entity();
        ent_col.add_component<TestComponent>(handle, 42u, "forty-two");
        ent_col.remove_component<TestComponent>(handle);
        REQUIRE(ent_col.has_component<TestComponent>(handle) == false);
    }

    SECTION("combined_test")
    {
        auto handle0 = ent_col.create_entity();
        ent_col.add_component<TestComponent>(handle0, 1u, "I, too, am a string.");

        auto handle1 = ent_col.create_entity();
        ent_col.add_component<TestComponent>(handle1, 2u, "Yes this is string?");
        ent_col.add_component<Position>(handle1, 4.0f, 2.0f);

        auto handle2 = ent_col.create_entity();
        ent_col.add_component<Position>(handle2, 42.0f, 69.0f);

        auto&       component   = ent_col.get_component<TestComponent>(handle0);
        std::string member_copy = component.string;
        ent_col.delete_entity(handle0);

        auto& component2 = ent_col.get_component<TestComponent>(handle1);
        member_copy      = component2.string;
        REQUIRE(member_copy == "Yes this is string?");
        REQUIRE(ent_col.get_component<TestComponent>(handle1).string == member_copy);

        CHECK(ent_col.has_component<TestComponent>(handle1));
        CHECK(ent_col.has_component<Position>(handle1));

        auto& h3p = ent_col.get_component<Position>(handle2);
        REQUIRE(h3p.x == 42.0f);
        REQUIRE(h3p.y == 69.0f);

        CHECK(ent_col.has_component<Position>(handle2));
        CHECK(!ent_col.has_component<TestComponent>(handle2));
    }

    SECTION("component_iteration")
    {
        auto handle0 = ent_col.create_entity();
        ent_col.add_component<Position>(handle0, 2.0f, 2.0f);
        ent_col.add_component<TestComponent>(handle0, 0u, "handle0");

        auto handle1 = ent_col.create_entity();
        ent_col.add_component<Position>(handle1, 3.0f, 3.0f);
        ent_col.add_component<TestComponent>(handle1, 1u, "handle1");

        auto handle2 = ent_col.create_entity();
        ent_col.add_component<Position>(handle2, 4.0f, 4.0f);

        auto handle3 = ent_col.create_entity();
        ent_col.add_component<Position>(handle3, 5.0f, 5.0f);
        ent_col.add_component<TestComponent>(handle3, 2u, "handle3");

        for (auto[entity, test_component, position] :
             ent_col.get_with_components<TestComponent, Position>()) {
            CHECK(ent_col.has_component<TestComponent>(entity));
            CHECK(ent_col.has_component<Position>(entity));

            auto& str = test_component->string;
            CHECK(str.find("handle") != str.npos);
        }
    }

    SECTION("brim_filling_test")
    {
        std::array<Mg::ecs::Entity, num_elems> es;

        for (uint32_t i = 0; i < num_elems; ++i) {
            es[i] = ent_col.create_entity();

            ent_col.add_component<IndexPairComponent>(
                es[i], i, static_cast<uint32_t>(num_elems - i));
        }

        for (uint32_t i = 0; i < num_elems; ++i) {
            auto& d = ent_col.get_component<IndexPairComponent>(es[i]);
            REQUIRE(d.elem1 == i);
            REQUIRE(d.elem2 == num_elems - i);
        }

        for (auto cs : ent_col.get_with_components()) {
            auto entity = std::get<Mg::ecs::Entity>(cs);
            CHECK(ent_col.has_component<IndexPairComponent>(entity));

            auto&    d    = ent_col.get_component<IndexPairComponent>(entity);
            uint32_t temp = d.elem1;
            d.elem1       = d.elem2;
            d.elem2       = temp;
        }

        for (uint32_t i = 0; i < num_elems; ++i) {
            auto& d = ent_col.get_component<IndexPairComponent>(es[i]);
            REQUIRE(d.elem2 == i);
            REQUIRE(d.elem1 == num_elems - i);
        }

        for (auto cs : ent_col.get_with_components()) {
            auto entity = std::get<Mg::ecs::Entity>(cs);
            auto mask   = ent_col.component_mask(entity);
            CHECK(mask[IndexPairComponent::ComponentTypeId]);
        }
    }
} // TEST_CASE("Entity")
