#include "catch.hpp"

#include <mg/utils/mg_observer.h>

#include <memory>
#include <string>

// Event
struct E {
    int index;
    std::string_view value;
};

// Subject
using S = Mg::Subject<E>;

// Observer
struct O : public Mg::Observer<E> {
    explicit O(int index, std::string_view expected) : index(index), expected(expected) {}

    int index = 0;
    std::string expected{};

    int num_notifications_received = 0;
    int num_notifications_for_this = 0;

    void on_notify(const E& e) override
    {
        ++num_notifications_received;

        if (e.index != index) {
            return;
        }

        ++num_notifications_for_this;

        if (e.value != expected) {
            throw -1;
        }
    }
};

TEST_CASE("Basic observer test")
{
    S s;
    O o1{ 1, "o1" };
    O o2{ 2, "o2" };
    O o3{ 3, "o3" };

    s.add_observer(o1);
    s.add_observer(o2);
    s.add_observer(o3);

    s.notify(E{ 1, "o1" });
    s.notify(E{ 2, "o2" });
    s.notify(E{ 3, "o3" });

    REQUIRE(o1.num_notifications_received == 3);
    REQUIRE(o2.num_notifications_received == 3);
    REQUIRE(o3.num_notifications_received == 3);

    REQUIRE(o1.num_notifications_for_this == 1);
    REQUIRE(o2.num_notifications_for_this == 1);
    REQUIRE(o3.num_notifications_for_this == 1);

    REQUIRE_THROWS_AS(s.notify(E{ 1, "o2" }), int);
}

TEST_CASE("Remove_observer")
{
    S s;
    auto po1 = std::make_unique<O>(1, "po1");
    auto po2 = std::make_unique<O>(2, "po2");
    auto po3 = std::make_unique<O>(3, "po3");

    s.add_observer(*po1);
    s.add_observer(*po2);
    s.add_observer(*po3);

    po2.reset();

    s.notify(E{ 1, "po1" });
    s.notify(E{ 2, "po2" });
    s.notify(E{ 3, "po3" });

    REQUIRE(po1->num_notifications_received == 3);
    REQUIRE(po3->num_notifications_received == 3);

    REQUIRE(po1->num_notifications_for_this == 1);
    REQUIRE(po3->num_notifications_for_this == 1);
}

TEST_CASE("Move observer test")
{
    S s;
    O o1{ 1, "o1" };
    O o2{ 2, "o2" };
    O o3{ 3, "o3" };

    s.add_observer(o1);
    s.add_observer(o2);
    s.add_observer(o3);

    O o2_new = std::move(o2);

    s.notify(E{ 1, "o1" });
    s.notify(E{ 2, "o2" });
    s.notify(E{ 3, "o3" });

    REQUIRE(o1.num_notifications_received == 3);
    REQUIRE(o2_new.num_notifications_received == 3);
    REQUIRE(o3.num_notifications_received == 3);

    REQUIRE(o1.num_notifications_for_this == 1);
    REQUIRE(o2_new.num_notifications_for_this == 1);
    REQUIRE(o3.num_notifications_for_this == 1);
}

TEST_CASE("Obsever: Remove during notification")
{
    struct SelfDestructObserver : Mg::Observer<int> {
        explicit SelfDestructObserver(int i) : id(i) {}

        int id = 0;
        int num_notifications_received = 0;
        int num_notifications_for_this = 0;

        void on_notify(const int& i) override
        {
            ++num_notifications_received;

            if (i != id) {
                return;
            }

            ++num_notifications_for_this;
            detach();
        }
    };

    Mg::Subject<int> s;

    SelfDestructObserver o1{ 1 };
    SelfDestructObserver o2{ 2 };
    SelfDestructObserver o3{ 3 };

    s.add_observer(o1);
    s.add_observer(o2);
    s.add_observer(o3);

    s.notify(2);

    REQUIRE(o1.num_notifications_received == 1);
    REQUIRE(o2.num_notifications_received == 1);
    REQUIRE(o3.num_notifications_received == 1);

    REQUIRE(o1.num_notifications_for_this == 0);
    REQUIRE(o2.num_notifications_for_this == 1);
    REQUIRE(o3.num_notifications_for_this == 0);

    s.notify(2);

    REQUIRE(o2.num_notifications_for_this == 1); // Should not receive 2nd notification
    REQUIRE(o2.num_notifications_received == 1);

    REQUIRE(o1.num_notifications_received == 2);
    REQUIRE(o3.num_notifications_received == 2);
}

TEST_CASE("Observer: Subject movable test")
{
    S s;
    O o1{ 1, "o1" };
    O o2{ 2, "o2" };

    s.add_observer(o1);
    s.add_observer(o2);

    S s_new(std::move(s));

    s_new.notify(E{ 1, "o1" });
    s_new.notify(E{ 2, "o2" });

    REQUIRE(o1.num_notifications_received == 2);
    REQUIRE(o2.num_notifications_received == 2);

    REQUIRE(o1.num_notifications_for_this == 1);
    REQUIRE(o2.num_notifications_for_this == 1);
}
