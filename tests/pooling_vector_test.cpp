#include <mg/containers/mg_pooling_vector.h>
#include <mg/utils/mg_instance_counter.h>

#include "catch.hpp"

#include <random>
#include <stdexcept>

struct Type {
    Type() = default;
    Type(size_t v) : value(v) {}

    Type(const Type&) = default;
    Type(Type&&)      = default;
    Type& operator=(const Type&) = default;
    Type& operator=(Type&&) = default;

    size_t& get_value_checked()
    {
        if (counter.is_moved_from()) {
            throw std::runtime_error("Type::get_value_checked(): is moved-from.");
        }
        if (counter.is_destroyed()) {
            throw std::runtime_error("Type::get_value_checked(): is destroyed.");
        }

        return value;
    }

    size_t value = 0;

    Mg::InstanceCounter<Type, false> counter;
};

// These allow for pseudorandom test. Return expected new number of elements after operation.
void insert(Mg::PoolingVector<Type>& pv, uint32_t)
{
    auto num_elems_before = Mg::InstanceCounter<Type>::get_counter();

    auto [index, ptr] = pv.construct();
    ptr->value        = index;

    CHECK(Mg::InstanceCounter<Type>::get_counter() == num_elems_before + 1);
}

void destroy(Mg::PoolingVector<Type>& pv, uint32_t index)
{
    auto num_elems_before = Mg::InstanceCounter<Type>::get_counter();

    if (!pv.index_valid(index)) { return; }
    pv.destroy(index);

    CHECK(Mg::InstanceCounter<Type>::get_counter() == num_elems_before - 1);
}

void access(Mg::PoolingVector<Type>& pv, uint32_t index)
{
    auto num_elems_before = Mg::InstanceCounter<Type>::get_counter();

    if (!pv.index_valid(index)) { return; }
    CHECK(pv[index].get_value_checked() == index);

    CHECK(Mg::InstanceCounter<Type>::get_counter() == num_elems_before);
}

using PVOperation = void (*)(Mg::PoolingVector<Type>&, uint32_t);

PVOperation operations[] = { &insert, &destroy, &access };

TEST_CASE("PoolingVector basic")
{
    Mg::PoolingVector<Type> pv1(1);
    Mg::PoolingVector<Type> pv2(10);
    Mg::PoolingVector<Type> pv3(1000);

    SECTION("constructible") {}

    SECTION("insert")
    {
        pv1.construct(1);
        pv2.construct(2);
        pv3.construct(3);
    }

    SECTION("read")
    {
        auto [index1, ptr1] = pv1.construct(1);
        auto [index2, ptr2] = pv2.construct(2);
        auto [index3, ptr3] = pv3.construct(3);

        CHECK(index1 == 0);
        CHECK(index2 == 0);
        CHECK(index3 == 0);

        CHECK(ptr1->get_value_checked() == 1);
        CHECK(ptr2->get_value_checked() == 2);
        CHECK(ptr3->get_value_checked() == 3);

        CHECK(pv1[0].get_value_checked() == 1);
        CHECK(pv2[0].get_value_checked() == 2);
        CHECK(pv3[0].get_value_checked() == 3);

        CHECK(Mg::InstanceCounter<Type>::get_counter() == 3);
    }

    SECTION("remove")
    {
        {
            auto [index1, ptr1] = pv1.construct(1);
            auto [index2, ptr2] = pv2.construct(2);
            auto [index3, ptr3] = pv3.construct(3);

            CHECK(Mg::InstanceCounter<Type>::get_counter() == 3);

            pv1.destroy(0);
            pv2.destroy(0);
            pv3.destroy(0);
        }

        CHECK(Mg::InstanceCounter<Type>::get_counter() == 0);

        auto [index1, ptr1] = pv1.construct(1);
        auto [index2, ptr2] = pv2.construct(2);
        auto [index3, ptr3] = pv3.construct(3);

        // Indices should be re-used.
        CHECK(index1 == 0);
        CHECK(index2 == 0);
        CHECK(index3 == 0);

        CHECK(Mg::InstanceCounter<Type>::get_counter() == 3);
    }

    SECTION("large")
    {
        static constexpr size_t num_elems = 10000;
        for (size_t i = 0; i < num_elems; ++i) {
            pv1.construct(i);
            pv2.construct(i);
            pv3.construct(i);
        }

        CHECK(Mg::InstanceCounter<Type>::get_counter() == 3 * num_elems);

        for (uint32_t i = 0; i < num_elems; ++i) {
            CHECK(pv1[i].get_value_checked() == i);
            CHECK(pv2[i].get_value_checked() == i);
            CHECK(pv3[i].get_value_checked() == i);
        }
    }

    SECTION("destroy")
    {
        {
            Mg::PoolingVector<Type> pv(256);

            static constexpr size_t num_elems = 10000;
            for (size_t i = 0; i < num_elems; ++i) { pv.construct(i); }

            CHECK(Mg::InstanceCounter<Type>::get_counter() == num_elems);

            for (uint32_t i = 0; i < num_elems; ++i) { CHECK(pv[i].get_value_checked() == i); }
        }

        CHECK(Mg::InstanceCounter<Type>::get_counter() == 0);
    }


    SECTION("pseudrandom")
    {
        std::mt19937                            g{ 75571296 };
        std::uniform_int_distribution<uint32_t> dist(static_cast<uint32_t>(g()));

        static constexpr size_t num_iterations = 10000;
        for (size_t i = 0; i < num_iterations; ++i) {
            const auto count = static_cast<uint32_t>(Mg::InstanceCounter<Type>::get_counter());
            auto       op    = dist(g) % std::size(operations);
            auto       index = dist(g) % (count > 0 ? count : 1);

            operations[op](pv2, index);
        }
    }
}
