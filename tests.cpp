#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "promise.hpp"
namespace pr = promise_hpp;

namespace
{
    struct obj_t {
    };

    bool check_hello_fail_exception(std::exception_ptr e) {
        try {
            std::rethrow_exception(e);
        } catch (std::logic_error& ee) {
            return 0 == std::strcmp(ee.what(), "hello fail");
        } catch (...) {
            return false;
        }
    }

    bool check_hello_fail2_exception(std::exception_ptr e) {
        try {
            std::rethrow_exception(e);
        } catch (std::logic_error& ee) {
            return 0 == std::strcmp(ee.what(), "hello fail2");
        } catch (...) {
            return false;
        }
    }
}

TEST_CASE("is_promise") {
    SECTION("positive") {
        static_assert(
            pr::is_promise<pr::promise<void>>::value,
            "unit test fail");
        static_assert(
            pr::is_promise<const pr::promise<void>>::value,
            "unit test fail");
        static_assert(
            pr::is_promise<const volatile pr::promise<void>>::value,
            "unit test fail");

        static_assert(
            pr::is_promise<pr::promise<int>>::value,
            "unit test fail");
        static_assert(
            pr::is_promise<const pr::promise<int>>::value,
            "unit test fail");
        static_assert(
            pr::is_promise<const volatile pr::promise<int>>::value,
            "unit test fail");
    }
    SECTION("negative") {
        static_assert(
            !pr::is_promise<pr::promise<void>&>::value,
            "unit test fail");
        static_assert(
            !pr::is_promise<const pr::promise<void>*>::value,
            "unit test fail");
        static_assert(
            !pr::is_promise<const volatile pr::promise<int>&>::value,
            "unit test fail");

        static_assert(
            !pr::is_promise<int>::value,
            "unit test fail");
        static_assert(
            !pr::is_promise<void>::value,
            "unit test fail");
        static_assert(
            !pr::is_promise<const volatile int>::value,
            "unit test fail");
    }
}

TEST_CASE("is_promise_r") {
    SECTION("positive") {
        static_assert(
            pr::is_promise_r<void, pr::promise<void>>::value,
            "unit test fail");
        static_assert(
            pr::is_promise_r<int, const pr::promise<int>>::value,
            "unit test fail");
        static_assert(
            pr::is_promise_r<double, const pr::promise<int>>::value,
            "unit test fail");
    }
    SECTION("negative") {
        static_assert(
            !pr::is_promise_r<void, pr::promise<int>>::value,
            "unit test fail");
        static_assert(
            !pr::is_promise_r<void, const pr::promise<int>>::value,
            "unit test fail");
        static_assert(
            !pr::is_promise_r<int, pr::promise<obj_t>>::value,
            "unit test fail");
        static_assert(
            !pr::is_promise_r<double, int>::value,
            "unit test fail");
    }
}

TEST_CASE("promise") {
    SECTION("resolved") {
        {
            int check_42_int = 0;
            auto p = pr::promise<int>();
            p.resolve(42);
            p.then([&check_42_int](int value){
                check_42_int = value;
            });
            REQUIRE(check_42_int == 42);
        }
        {
            int check_42_int = 0;
            auto p = pr::promise<int>();
            p.resolve(42);
            p.except([](std::exception_ptr){
            }).then([&check_42_int](int value){
                check_42_int = value;
            });
            REQUIRE(check_42_int == 42);
        }
        {
            int check_84_int = 0;
            bool check_void_call = false;
            int check_100500_transform = 0;
            auto p = pr::promise<int>();
            p.resolve(42);
            p.then([](int value){
                return value * 2;
            }).then([&check_84_int](int value){
                check_84_int = value;
            }).then([&check_void_call](){
                check_void_call = true;
            }).then([](){
                return 100500;
            }).then([&check_100500_transform](int value){
                check_100500_transform = value;
            });
            REQUIRE(check_84_int == 84);
            REQUIRE(check_void_call);
            REQUIRE(check_100500_transform == 100500);
        }
    }
    SECTION("rejected") {
        {
            bool call_fail_with_logic_error = false;
            bool not_call_then_on_reject = true;
            auto p = pr::promise<int>();
            p.reject(std::logic_error("hello fail"));
            p.then([&not_call_then_on_reject](int value) {
                (void)value;
                not_call_then_on_reject = false;
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(not_call_then_on_reject);
            REQUIRE(call_fail_with_logic_error);
        }
        {
            std::logic_error ee("hello fail");
            bool call_fail_with_logic_error = false;
            auto p = pr::promise<int>();
            p.reject(ee);
            p.then([](int){
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
        {
            std::logic_error ee("hello fail");
            bool call_fail_with_logic_error = false;
            auto p = pr::promise<int>();
            p.reject(std::make_exception_ptr(ee));
            p.then([](int){
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
        {
            int check_multi_fail = 0;
            auto p = pr::promise<>();
            p.reject(std::logic_error("hello fail"));
            p.except([&check_multi_fail](std::exception_ptr){
                ++check_multi_fail;
            }).except([&check_multi_fail](std::exception_ptr){
                ++check_multi_fail;
            });
            REQUIRE(check_multi_fail == 2);
        }
    }
    SECTION("unresolved") {
        {
            int check_42_int = 0;
            bool not_call_before_resolve = true;
            auto p = pr::promise<int>();
            p.then([&not_call_before_resolve](int value){
                not_call_before_resolve = false;
                return value * 2;
            }).then([&check_42_int, &not_call_before_resolve](int value){
                not_call_before_resolve = false;
                check_42_int = value;
            });
            REQUIRE(check_42_int == 0);
            REQUIRE(not_call_before_resolve);
            p.resolve(42);
            REQUIRE(check_42_int == 84);
            REQUIRE_FALSE(not_call_before_resolve);
        }
        {
            bool not_call_then_on_reject = true;
            bool call_fail_with_logic_error = false;
            auto p = pr::promise<int>();
            p.then([&not_call_then_on_reject](int){
                not_call_then_on_reject = false;
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(not_call_then_on_reject);
            REQUIRE_FALSE(call_fail_with_logic_error);
            p.reject(std::make_exception_ptr(std::logic_error("hello fail")));
            REQUIRE(not_call_then_on_reject);
            REQUIRE(call_fail_with_logic_error);
        }
    }
    SECTION("make_promise") {
        {
            int check_84_int = 0;
            auto p = pr::make_promise<int>([](auto resolve, auto reject){
                (void)reject;
                resolve(42);
            });
            p.then([](int value){
                return value * 2;
            }).then([&check_84_int](int value){
                check_84_int = value;
            });
            REQUIRE(check_84_int == 84);
        }
        {
            bool call_fail_with_logic_error = false;
            auto p = pr::make_promise<int>([](auto resolve, auto reject){
                (void)resolve;
                reject(std::logic_error("hello fail"));
            });
            p.except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
        {
            bool call_fail_with_logic_error = false;
            auto p = pr::make_promise<int>([](auto resolve, auto reject){
                (void)resolve;
                (void)reject;
                throw std::logic_error("hello fail");
            });
            p.except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
    }
    SECTION("make_resolved_promise") {
        {
            bool call_check = false;
            pr::make_resolved_promise()
            .then([&call_check]{
                call_check = true;
            });
            REQUIRE(call_check);
        }
        {
            int check_42_int = 0;
            pr::make_resolved_promise(42)
            .then([&check_42_int](int value){
                check_42_int = value;
            });
            REQUIRE(check_42_int == 42);
        }
    }
    SECTION("make_rejected_promise") {
        {
            bool call_fail_with_logic_error = false;
            pr::make_rejected_promise<int>(std::logic_error("hello fail"))
            .except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
        {
            bool call_fail_with_logic_error = false;
            pr::make_rejected_promise(std::logic_error("hello fail"))
            .except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
    }
    SECTION("exceptions") {
        {
            bool not_call_then_on_reject = true;
            bool call_fail_with_logic_error = false;
            auto p = pr::promise<int>();
            p.resolve(42);
            p.then([](int){
                throw std::logic_error("hello fail");
            }).then([&not_call_then_on_reject](){
                not_call_then_on_reject = false;
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(not_call_then_on_reject);
            REQUIRE(call_fail_with_logic_error);
        }
        {
            bool not_call_then_on_reject = true;
            bool call_fail_with_logic_error = false;
            auto p = pr::promise<int>();
            p.resolve(42);
            p.then([](int){
                throw std::logic_error("hello fail");
            }, [](std::exception_ptr){
                throw std::logic_error("hello fail2");
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail2_exception(e);
            });
            REQUIRE(not_call_then_on_reject);
            REQUIRE(call_fail_with_logic_error);
        }
    }
    SECTION("multi_then") {
        {
            auto p = pr::promise<int>();

            int pa_value = 0;
            {
                auto pa = p.then([](int value){
                    return value * 2;
                }).then([&pa_value](int value){
                    pa_value = value;
                });
            }

            int pb_value = 0;
            {
                auto pb = p.then([](int value){
                    return value / 2;
                }).then([&pb_value](int value){
                    pb_value = value;
                });
            }

            REQUIRE(pa_value == 0);
            REQUIRE(pb_value == 0);

            p.resolve(42);

            REQUIRE(pa_value == 84);
            REQUIRE(pb_value == 21);
        }
        {
            auto p = pr::promise<int>();

            int pa_value = 0;
            {
                auto pa = p.then([](int){
                    throw std::logic_error("hello fail");
                }).except([&pa_value](std::exception_ptr e){
                    if ( check_hello_fail_exception(e) ) {
                        pa_value = 84;
                    }
                });
            }

            int pb_value = 0;
            {
                auto pb = p.then([](int value){
                    return value / 2;
                }).then([&pb_value](int value){
                    pb_value = value;
                });
            }

            REQUIRE(pa_value == 0);
            REQUIRE(pb_value == 0);

            p.resolve(42);

            REQUIRE(pa_value == 84);
            REQUIRE(pb_value == 21);
        }
    }
    SECTION("chaining") {
        {
            int check_84_int = 0;
            auto p1 = pr::make_resolved_promise(42);
            auto p2 = pr::make_resolved_promise(84);

            p1.then([&p2](int v){
                (void)v;
                return p2;
            }).then([&check_84_int](int v2){
                check_84_int = v2;
            });

            REQUIRE(check_84_int == 84);
        }
        {
            int check_84_int = 0;
            auto p1 = pr::make_resolved_promise();
            auto p2 = pr::make_resolved_promise(84);

            p1.then([&p2](){
                return p2;
            }).then([&check_84_int](int v){
                check_84_int = v;
            });

            REQUIRE(check_84_int == 84);
        }
        {
            int check_84_int = 0;
            auto p1 = pr::make_resolved_promise(42);
            auto p2 = pr::make_resolved_promise();

            p1.then([&p2](int v){
                (void)v;
                return p2;
            }).then([&check_84_int](){
                check_84_int = 84;
            });

            REQUIRE(check_84_int == 84);
        }
        {
            int check_84_int = 0;
            auto p1 = pr::make_resolved_promise();
            auto p2 = pr::make_resolved_promise();

            p1.then([&p2](){
                return p2;
            }).then([&check_84_int](){
                check_84_int = 84;
            });

            REQUIRE(check_84_int == 84);
        }
    }
    SECTION("lazy_chaining") {
        {
            int check_84_int = 0;
            auto p1 = pr::make_promise<int>();
            auto p2 = pr::make_promise<int>();

            p1.then([&p2](int v){
                (void)v;
                return p2;
            }).then([&check_84_int](int v2){
                check_84_int = v2;
            });

            REQUIRE(check_84_int == 0);
            p1.resolve(42);
            REQUIRE(check_84_int == 0);
            p2.resolve(84);
            REQUIRE(check_84_int == 84);
        }
    }
    SECTION("typed_chaining_fails") {
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_resolved_promise(42);
            auto p2 = pr::make_resolved_promise(84);

            p1.then([&p2](int v){
                (void)v;
                throw std::logic_error("hello fail");
                return p2;
            }).then([](int v2){
                (void)v2;
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });

            REQUIRE(call_fail_with_logic_error);
        }
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_resolved_promise(42);
            auto p2 = pr::make_resolved_promise(84);

            p1.then([&p2](int v){
                (void)v;
                return p2;
            }).then([](int v2){
                (void)v2;
                throw std::logic_error("hello fail");
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });

            REQUIRE(call_fail_with_logic_error);
        }
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_rejected_promise<int>(std::logic_error("hello fail"));
            auto p2 = pr::make_resolved_promise(84);

            p1.then([&p2](int v){
                (void)v;
                return p2;
            }).then([](int v2){
                (void)v2;
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });

            REQUIRE(call_fail_with_logic_error);
        }
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_resolved_promise(42);
            auto p2 = pr::make_rejected_promise<int>(std::logic_error("hello fail"));

            p1.then([&p2](int v){
                (void)v;
                return p2;
            }).then([](int v2){
                (void)v2;
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });

            REQUIRE(call_fail_with_logic_error);
        }
    }
    SECTION("void_chaining_fails") {
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_resolved_promise();
            auto p2 = pr::make_resolved_promise();

            p1.then([&p2](){
                throw std::logic_error("hello fail");
                return p2;
            }).then([](){
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });

            REQUIRE(call_fail_with_logic_error);
        }
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_resolved_promise();
            auto p2 = pr::make_resolved_promise();

            p1.then([&p2](){
                return p2;
            }).then([](){
                throw std::logic_error("hello fail");
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });

            REQUIRE(call_fail_with_logic_error);
        }
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_rejected_promise<void>(std::logic_error("hello fail"));
            auto p2 = pr::make_resolved_promise();

            p1.then([&p2](){
                return p2;
            }).then([](){
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });

            REQUIRE(call_fail_with_logic_error);
        }
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_resolved_promise();
            auto p2 = pr::make_rejected_promise<void>(std::logic_error("hello fail"));

            p1.then([&p2](){
                return p2;
            }).then([](){
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });

            REQUIRE(call_fail_with_logic_error);
        }
    }
    SECTION("make_all_promise") {
        {
            bool all_is_ok = false;
            pr::make_all_promise(std::vector<pr::promise<int>>())
                .then([&all_is_ok](const std::vector<int>& c){
                    all_is_ok = c.empty();
                });
            REQUIRE(all_is_ok);
        }
        {
            bool all_is_ok = false;
            auto p = pr::make_all_promise(std::vector<pr::promise<int>>{
                pr::make_resolved_promise(32),
                pr::make_resolved_promise(10)
            }).then([&all_is_ok](const std::vector<int>& c){
                all_is_ok = (2 == c.size())
                    && c[0] == 32
                    && c[1] == 10;
            });
            REQUIRE(all_is_ok);
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();

            int call_then_only_once = 0;
            pr::make_all_promise(std::vector<pr::promise<int>>{p1, p2})
            .then([&call_then_only_once](const std::vector<int>& c){
                (void)c;
                ++call_then_only_once;
            });

            p1.resolve(1);
            p2.resolve(2);

            REQUIRE(call_then_only_once == 1);
        }
    }
    SECTION("make_any_promise") {
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();

            int check_42_int = 0;
            int call_then_only_once = 0;
            pr::make_any_promise(std::vector<pr::promise<int>>{p1, p2})
            .then([&check_42_int, &call_then_only_once](const int& v){
                check_42_int = v;
                ++call_then_only_once;
            });

            p1.resolve(42);
            REQUIRE(check_42_int == 42);
            REQUIRE(call_then_only_once == 1);

            p2.resolve(84);
            REQUIRE(check_42_int == 42);
            REQUIRE(call_then_only_once == 1);
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();

            int check_42_int = 0;
            int call_then_only_once = 0;
            pr::make_any_promise(std::vector<pr::promise<int>>{p1, p2})
            .then([&check_42_int, &call_then_only_once](const int& v){
                check_42_int = v;
                ++call_then_only_once;
            });

            p2.resolve(42);
            REQUIRE(check_42_int == 42);
            REQUIRE(call_then_only_once == 1);

            p1.resolve(84);
            REQUIRE(check_42_int == 42);
            REQUIRE(call_then_only_once == 1);
        }
    }
    SECTION("make_all_promise_fail") {
        {
            bool call_fail_with_logic_error = false;
            bool not_call_then_on_reject = true;
            auto p = pr::make_all_promise(std::vector<pr::promise<int>>{
                pr::make_rejected_promise<int>(std::logic_error("hello fail")),
                pr::make_resolved_promise(10)
            }).then([&not_call_then_on_reject](const std::vector<int>& c){
                (void)c;
                not_call_then_on_reject = false;
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(not_call_then_on_reject);
            REQUIRE(call_fail_with_logic_error);
        }
    }
    SECTION("make_any_promise_fail") {
        REQUIRE_THROWS_AS(
            pr::make_any_promise(std::vector<pr::promise<int>>{}),
            std::logic_error);
        {
            bool call_fail_with_logic_error = false;
            bool not_call_then_on_reject = true;
            auto p = pr::make_any_promise(std::vector<pr::promise<int>>{
                pr::make_rejected_promise<int>(std::logic_error("hello fail")),
                pr::make_resolved_promise(10)
            }).then([&not_call_then_on_reject](const int& c){
                (void)c;
                not_call_then_on_reject = false;
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(not_call_then_on_reject);
            REQUIRE(call_fail_with_logic_error);
        }
    }
    SECTION("then_all") {
        {
            int check_42_int = 0;
            pr::make_resolved_promise()
            .then_all([](){
                return std::vector<pr::promise<int>>{
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10)};
            }).then([&check_42_int](const std::vector<int>& v){
                if ( v.size() == 2) {
                    check_42_int = v[0] + v[1];
                }
            });
            REQUIRE(check_42_int == 42);
        }
        {
            int check_42_int = 0;
            int check_42_int2 = 0;
            pr::make_resolved_promise(42)
            .then_all([&check_42_int](int v){
                check_42_int = v;
                return std::vector<pr::promise<int>>{
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10)};
            }).then([&check_42_int2](const std::vector<int>& v){
                if ( v.size() == 2) {
                    check_42_int2 = v[0] + v[1];
                }
            });
            REQUIRE(check_42_int == 42);
            REQUIRE(check_42_int2 == 42);
        }
    }
    SECTION("then_any") {
        {
            int check_42_int = 0;
            pr::make_resolved_promise()
            .then_any([](){
                return std::vector<pr::promise<int>>{
                    pr::make_resolved_promise(42),
                    pr::make_rejected_promise<int>(std::logic_error("hello fail"))};
            }).then([&check_42_int](const int& v){
                check_42_int = v;
            });
            REQUIRE(check_42_int == 42);
        }
        {
            bool call_fail_with_logic_error = false;
            pr::make_resolved_promise()
            .then_any([](){
                return std::vector<pr::promise<int>>{
                    pr::make_rejected_promise<int>(std::logic_error("hello fail")),
                    pr::make_resolved_promise(42)};
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
        {
            int check_42_int = 0;
            int check_42_int2 = 0;
            int call_then_only_once = 0;
            pr::make_resolved_promise(42)
            .then_any([&check_42_int](int v){
                check_42_int = v;
                return std::vector<pr::promise<int>>{
                    pr::make_resolved_promise(42),
                    pr::make_resolved_promise(10)};
            }).then([&call_then_only_once, &check_42_int2](const int& v){
                ++call_then_only_once;
                check_42_int2 = v;
            });
            REQUIRE(check_42_int == 42);
            REQUIRE(check_42_int2 == 42);
            REQUIRE(call_then_only_once == 1);
        }
    }
}
