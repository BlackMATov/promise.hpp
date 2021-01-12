/*******************************************************************************
 * This file is part of the "https://github.com/blackmatov/promise.hpp"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018-2021, by Matvey Cherevko (blackmatov@gmail.com)
 ******************************************************************************/

#include <promise.hpp/promise.hpp>
#include "doctest/doctest.h"

#include <array>
#include <thread>
#include <numeric>
#include <cstring>

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

    bool check_empty_aggregate_exception(std::exception_ptr e) {
        try {
            std::rethrow_exception(e);
        } catch (pr::aggregate_exception& ee) {
            return ee.empty();
        } catch (...) {
            return false;
        }
    }

    bool check_two_aggregate_exception(std::exception_ptr e) {
        try {
            std::rethrow_exception(e);
        } catch (pr::aggregate_exception& ee) {
            if ( ee.size() != 2 ) {
                return false;
            }
            return check_hello_fail_exception(ee[0])
                && check_hello_fail_exception(ee[1]);
        } catch (...) {
            return false;
        }
    }

    class auto_thread final {
    public:
        template < typename F, typename... Args >
        auto_thread(F&& f, Args&&... args)
        : thread_(std::forward<F>(f), std::forward<Args>(args)...) {}

        ~auto_thread() noexcept {
            if ( thread_.joinable() ) {
                thread_.join();
            }
        }

        void join() {
            thread_.join();
        }
    private:
        std::thread thread_;
    };
}

TEST_CASE("is_promise") {
    SUBCASE("positive") {
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
    SUBCASE("negative") {
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
    SUBCASE("positive") {
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
    SUBCASE("negative") {
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
    SUBCASE("basic") {
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();
            REQUIRE_FALSE(p1 == p2);
            REQUIRE(p1 != p2);

            REQUIRE((p1 < p2 || p2 < p1));
            REQUIRE(p1.hash() != p2.hash());
            REQUIRE(p1.hash() == std::hash<pr::promise<int>>()(p1));
        }
        {
            auto p1 = pr::promise<void>();
            auto p2 = pr::promise<void>();
            REQUIRE_FALSE(p1 == p2);
            REQUIRE(p1 != p2);

            REQUIRE((p1 < p2 || p2 < p1));
            REQUIRE(p1.hash() != p2.hash());
            REQUIRE(p1.hash() == std::hash<pr::promise<void>>()(p1));
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();
            auto p3 = p1;
            REQUIRE(p1 == p3);
            p3 = p2;
            REQUIRE(p2 == p3);
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();
            auto p3 = p1;
            p1.swap(p2);
            REQUIRE(p2 == p3);
            REQUIRE_FALSE(p1 == p3);
        }
        {
            auto p1 = pr::promise<void>();
            auto p2 = pr::promise<void>();
            auto p3 = p1;
            p1.swap(p2);
            REQUIRE(p2 == p3);
            REQUIRE_FALSE(p1 == p3);
        }
    }
    SUBCASE("resolved") {
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
    SUBCASE("resolved_ref") {
        {
            int* check_42_int = nullptr;
            auto p = pr::promise<std::reference_wrapper<int>>();
            int i = 42;
            p.resolve(i);
            p.then([&check_42_int](int& value) mutable {
                check_42_int = &value;
            });
            REQUIRE(check_42_int);
            REQUIRE(*check_42_int == 42);
        }
        {
            const int* check_42_int = nullptr;
            auto p = pr::promise<std::reference_wrapper<const int>>();
            const int i = 42;
            p.resolve(i);
            p.then([&check_42_int](const int& value) mutable {
                check_42_int = &value;
            });
            REQUIRE(check_42_int);
            REQUIRE(*check_42_int == 42);
        }
    }
    SUBCASE("rejected") {
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
            p.except([&check_multi_fail](std::exception_ptr e){
                ++check_multi_fail;
                std::rethrow_exception(e);
            }).except([&check_multi_fail](std::exception_ptr){
                ++check_multi_fail;
            });
            REQUIRE(check_multi_fail == 2);
        }
    }
    SUBCASE("unresolved") {
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
    SUBCASE("finally") {
        {
            bool all_is_ok = false;
            auto p = pr::promise<int>();
            p.finally([&all_is_ok](){
                all_is_ok = true;
            });
            REQUIRE_FALSE(all_is_ok);
            p.resolve(1);
            REQUIRE(all_is_ok);
        }
        {
            bool all_is_ok = false;
            auto p = pr::promise<int>();
            p.finally([&all_is_ok](){
                all_is_ok = true;
            });
            REQUIRE_FALSE(all_is_ok);
            p.reject(std::make_exception_ptr(std::logic_error("hello fail")));
            REQUIRE(all_is_ok);
        }
        {
            bool all_is_ok = false;
            pr::make_resolved_promise(1)
            .finally([&all_is_ok](){
                all_is_ok = true;
            });
            REQUIRE(all_is_ok);
        }
        {
            bool all_is_ok = false;
            pr::make_rejected_promise<int>(std::logic_error("hello fail"))
            .finally([&all_is_ok](){
                all_is_ok = true;
            });
            REQUIRE(all_is_ok);
        }
    }
    SUBCASE("after_finally") {
        {
            int check_84_int = 0;
            auto p = pr::promise<>();
            p.finally([&check_84_int](){
                check_84_int = 42;
                return 100500;
            }).then([&check_84_int](){
                check_84_int *= 2;
            });
            REQUIRE(check_84_int == 0);
            p.resolve();
            REQUIRE(check_84_int == 84);
        }
        {
            int check_84_int = 0;
            auto p = pr::promise<>();
            p.finally([&check_84_int](){
                check_84_int = 42;
                return 100500;
            }).except([&check_84_int](std::exception_ptr){
                check_84_int *= 2;
            });
            REQUIRE(check_84_int == 0);
            p.reject(std::make_exception_ptr(std::logic_error("hello fail")));
            REQUIRE(check_84_int == 84);
        }
    }
    SUBCASE("failed_finally") {
        {
            int check_84_int = 0;
            auto p = pr::promise<>();
            p.finally([&check_84_int](){
                check_84_int += 42;
                throw std::logic_error("hello fail");
            }).except([&check_84_int](std::exception_ptr e){
                if ( check_hello_fail_exception(e) ) {
                    check_84_int += 42;
                }
            });
            p.resolve();
            REQUIRE(check_84_int == 84);
        }
        {
            int check_84_int = 0;
            auto p = pr::promise<>();
            p.finally([&check_84_int](){
                check_84_int += 42;
                throw std::logic_error("hello fail");
            }).except([&check_84_int](std::exception_ptr e){
                if ( check_hello_fail_exception(e) ) {
                    check_84_int += 42;
                }
            });
            p.reject(std::make_exception_ptr(std::logic_error("hello")));
            REQUIRE(check_84_int == 84);
        }
        {
            int check_84_int = 0;
            auto p = pr::promise<int>();
            p.finally([&check_84_int](){
                check_84_int += 42;
                throw std::logic_error("hello fail");
            }).except([&check_84_int](std::exception_ptr e) -> int {
                if ( check_hello_fail_exception(e) ) {
                    check_84_int += 42;
                }
                return 0;
            });
            p.resolve(1);
            REQUIRE(check_84_int == 84);
        }
        {
            int check_84_int = 0;
            auto p = pr::promise<int>();
            p.finally([&check_84_int](){
                check_84_int += 42;
                throw std::logic_error("hello fail");
            }).except([&check_84_int](std::exception_ptr e) -> int {
                if ( check_hello_fail_exception(e) ) {
                    check_84_int += 42;
                }
                return 0;
            });
            p.reject(std::make_exception_ptr(std::logic_error("hello")));
            REQUIRE(check_84_int == 84);
        }
    }
    SUBCASE("make_promise") {
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
                return 0;
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
                return 0;
            });
            REQUIRE(call_fail_with_logic_error);
        }
    }
    SUBCASE("make_resolved_promise") {
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
    SUBCASE("make_rejected_promise") {
        {
            bool call_fail_with_logic_error = false;
            pr::make_rejected_promise<int>(std::logic_error("hello fail"))
            .except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
                return 0;
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
    SUBCASE("exceptions") {
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
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(not_call_then_on_reject);
            REQUIRE(call_fail_with_logic_error);
        }
    }
    SUBCASE("multi_then") {
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
    SUBCASE("chaining") {
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
    SUBCASE("lazy_chaining") {
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
    SUBCASE("typed_chaining_fails") {
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_resolved_promise(42);

            p1.then([](int v) -> pr::promise<int> {
                (void)v;
                throw std::logic_error("hello fail");
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
    SUBCASE("void_chaining_fails") {
        {
            bool call_fail_with_logic_error = false;
            auto p1 = pr::make_resolved_promise();

            p1.then([]() -> pr::promise<void> {
                throw std::logic_error("hello fail");
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
    SUBCASE("make_all_promise") {
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
            auto p = pr::make_resolved_promise().then_all([](){
                return std::vector<pr::promise<int>>{
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10)};
            }).then([&all_is_ok](const std::vector<int>& c){
                all_is_ok = (2 == c.size())
                    && c[0] == 32
                    && c[1] == 10;
            });
            REQUIRE(all_is_ok);
        }
        {
            bool all_is_ok = false;
            auto p = pr::make_resolved_promise(1).then_all([](int){
                return std::vector<pr::promise<int>>{
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10)};
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
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();

            int call_then_only_once = 0;
            pr::make_all_promise(std::array<pr::promise<int>, 2>{p1, p2})
            .then([&call_then_only_once](const std::vector<int>& c){
                (void)c;
                ++call_then_only_once;
            });

            p1.resolve(1);
            p2.resolve(2);

            REQUIRE(call_then_only_once == 1);
        }
        {
            class o_t {
            public:
                o_t() = delete;
                o_t(int i) { (void)i; }
            };

            pr::promise<>()
            .then_all([](){
                return std::vector<pr::promise<o_t>>{
                    pr::make_resolved_promise<o_t>(40),
                    pr::make_resolved_promise<o_t>(2)};
            });
        }
    }
    SUBCASE("make_any_promise") {
        {
            bool all_is_ok = false;
            auto p = pr::make_any_promise(std::vector<pr::promise<int>>{});
            p.except([&all_is_ok](std::exception_ptr e){
                all_is_ok = check_empty_aggregate_exception(e);
                return 0;
            });
            REQUIRE(all_is_ok);
        }
        {
            auto p = pr::make_resolved_promise().then_any([](){
               return std::vector<pr::promise<int>>{
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10)};
            }).then([](int i){
                return i;
            });
            REQUIRE(p.get() == 32);
        }
        {
            auto p = pr::make_resolved_promise(1).then_any([](int){
               return std::vector<pr::promise<int>>{
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10)};
            }).then([](int i){
                return i;
            });
            REQUIRE(p.get() == 32);
        }
        {
            auto p = pr::make_any_promise(std::vector<pr::promise<int>>{
                pr::make_resolved_promise(32),
                pr::make_rejected_promise<int>(std::logic_error("hello fail"))
            }).then([](int i){
                return i;
            });
            REQUIRE(p.get() == 32);
        }
        {
            auto p = pr::make_any_promise(std::vector<pr::promise<int>>{
                pr::make_rejected_promise<int>(std::logic_error("hello fail")),
                pr::make_resolved_promise(32)
            }).then([](int i){
                return i;
            });
            REQUIRE(p.get() == 32);
        }
        {
            bool all_is_ok = false;
            auto p = pr::make_any_promise(std::vector<pr::promise<int>>{
                pr::make_rejected_promise<int>(std::logic_error("hello fail")),
                pr::make_rejected_promise<int>(std::logic_error("hello fail"))
            }).except([&all_is_ok](std::exception_ptr e){
                all_is_ok = check_two_aggregate_exception(e);
                return 0;
            });
            REQUIRE(all_is_ok);
        }
    }
    SUBCASE("make_race_promise") {
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();

            int check_42_int = 0;
            int call_then_only_once = 0;
            pr::make_race_promise(std::vector<pr::promise<int>>{p1, p2})
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
            pr::make_race_promise(std::vector<pr::promise<int>>{p1, p2})
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
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();

            int check_42_int = 0;
            int call_then_only_once = 0;
            pr::make_race_promise(std::array<pr::promise<int>,2>{p1, p2})
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
        {
            class o_t {
            public:
                o_t() = delete;
                o_t(int i) { (void)i; }
            };

            pr::promise<>()
            .then_race([](){
                return std::vector<pr::promise<o_t>>{
                    pr::make_resolved_promise<o_t>(40),
                    pr::make_resolved_promise<o_t>(2)};
            });
        }
    }
    SUBCASE("make_tuple_promise") {
        {
            static_assert(
                std::is_same<
                    pr::impl::tuple_promise_result_t<std::tuple<>>,
                    std::tuple<>>::value,
                "unit test fail");
            static_assert(
                std::is_same<
                    pr::impl::tuple_promise_result_t<std::tuple<pr::promise<int>>>,
                    std::tuple<int>>::value,
                "unit test fail");
            static_assert(
                std::is_same<
                    pr::impl::tuple_promise_result_t<std::tuple<pr::promise<int>, pr::promise<float>>>,
                    std::tuple<int, float>>::value,
                "unit test fail");
        }
        {
            auto p = pr::make_tuple_promise(std::make_tuple());
            REQUIRE(p.get() == std::make_tuple());
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::make_tuple_promise(std::make_tuple(p1));
            p1.resolve(42);
            REQUIRE(p2.get_or_default(std::make_tuple(0)) == std::make_tuple(42));
        }
        {
            auto p1 = pr::promise<int>();
            auto t0 = std::make_tuple(p1);
            auto p2 = pr::make_tuple_promise(t0);
            p1.resolve(42);
            REQUIRE(p2.get_or_default(std::make_tuple(0)) == std::make_tuple(42));
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<float>();
            auto p3 = pr::make_tuple_promise(std::make_tuple(p1, p2));
            p1.resolve(42);
            p2.resolve(4.2f);
            REQUIRE(p3.get_or_default(std::make_tuple(0, 0.f)) == std::make_tuple(42, 4.2f));
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<float>();
            auto p3 = pr::promise<int>();
            auto p4 = pr::make_tuple_promise(std::make_tuple(p1, p2, p3));
            p1.resolve(42);
            p2.resolve(4.2f);
            p3.resolve(84);
            REQUIRE(p4.get_or_default(std::make_tuple(0, 0.f, 0)) == std::make_tuple(42, 4.2f, 84));
        }
        {
            class o_t {
            public:
                o_t() = delete;
            };

            pr::promise<>()
            .then_tuple([](){
                auto p1 = pr::promise<o_t>();
                auto p2 = pr::promise<o_t>();
                return std::make_tuple(std::move(p1), std::move(p2));
            });
        }
        {
            auto p1 = pr::promise<std::reference_wrapper<int>>();
            auto p2 = pr::promise<std::reference_wrapper<float>>();
            auto p3 = pr::make_tuple_promise(std::make_tuple(p1, p2));

            int i = 10;
            float f = 0.f;
            p1.resolve(i);
            p2.resolve(f);

            p3.then([&i,&f](const std::tuple<int&, float&>& t){
                REQUIRE(&std::get<0>(t) == &i);
                REQUIRE(&std::get<1>(t) == &f);
            });
        }
    }
    SUBCASE("make_all_promise_fail") {
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
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<int>();

            int call_then_count = 0;
            int call_except_count = 0;
            pr::make_all_promise(std::vector<pr::promise<int>>{p1, p2})
            .then([&call_then_count](const std::vector<int>& c){
                (void)c;
                ++call_then_count;
            }, [&call_except_count](std::exception_ptr){
                ++call_except_count;
            });

            p1.resolve(1);
            REQUIRE(call_then_count == 0);
            REQUIRE(call_except_count == 0);
            p2.reject(std::logic_error("hello fail"));
            REQUIRE(call_then_count == 0);
            REQUIRE(call_except_count == 1);
        }
    }
    SUBCASE("make_race_promise_fail") {
        {
            bool call_fail_with_logic_error = false;
            bool not_call_then_on_reject = true;
            auto p = pr::make_race_promise(std::vector<pr::promise<int>>{
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
    SUBCASE("make_tuple_promise_fail") {
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::make_tuple_promise(std::make_tuple(p1));
            p1.reject(std::logic_error("hello failt"));
            REQUIRE_THROWS_AS(p2.get(), std::logic_error);
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<float>();
            auto p3 = pr::make_tuple_promise(std::make_tuple(p1, p2));
            p1.resolve(42);
            p2.reject(std::logic_error("hello failt"));
            REQUIRE_THROWS_AS(p3.get(), std::logic_error);
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<float>();
            auto p3 = pr::make_tuple_promise(std::make_tuple(p1, p2));
            p1.reject(std::logic_error("hello failt"));
            p2.resolve(4.2f);
            REQUIRE_THROWS_AS(p3.get(), std::logic_error);
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<float>();
            auto p3 = pr::make_tuple_promise(std::make_tuple(p1, p2));
            p1.reject(std::logic_error("hello failt"));
            REQUIRE_THROWS_AS(p3.get(), std::logic_error);
        }
        {
            auto p1 = pr::promise<int>();
            auto p2 = pr::promise<float>();
            auto p3 = pr::make_tuple_promise(std::make_tuple(p1, p2));
            p2.reject(std::logic_error("hello failt"));
            REQUIRE_THROWS_AS(p3.get(), std::logic_error);
        }
    }
    SUBCASE("then_all") {
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
    SUBCASE("then_race") {
        {
            int check_42_int = 0;
            pr::make_resolved_promise()
            .then_race([](){
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
            .then_race([](){
                return std::vector<pr::promise<int>>{
                    pr::make_rejected_promise<int>(std::logic_error("hello fail")),
                    pr::make_resolved_promise(42)};
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
                return 0;
            });
            REQUIRE(call_fail_with_logic_error);
        }
        {
            int check_42_int = 0;
            int check_42_int2 = 0;
            int call_then_only_once = 0;
            pr::make_resolved_promise(42)
            .then_race([&check_42_int](int v){
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
    SUBCASE("then_tuple") {
        {
            float check_42_float = 0.f;
            pr::make_resolved_promise()
            .then_tuple([](){
                return std::make_tuple(
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10.f));
            }).then([&check_42_float](const std::tuple<int, float>& t){
                check_42_float = std::get<0>(t) + std::get<1>(t);
            });
            REQUIRE(check_42_float == doctest::Approx(42.f).epsilon(0.01f));
        }
        {
            float check_42_float = 0.f;
            pr::make_resolved_promise(42)
            .then_tuple([](int){
                return std::make_tuple(
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10.f));
            }).then([&check_42_float](const std::tuple<int, float>& t){
                check_42_float = std::get<0>(t) + std::get<1>(t);
            });
            REQUIRE(check_42_float == doctest::Approx(42.f).epsilon(0.01f));
        }
        {
            bool call_fail_with_logic_error = false;
            pr::make_resolved_promise(42)
            .then_tuple([](int){
                return std::make_tuple(
                    pr::make_resolved_promise(32),
                    pr::make_rejected_promise<float>(std::logic_error("hello fail")));
            }).except([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
                return std::make_tuple(0, 0.f);
            });
            REQUIRE(call_fail_with_logic_error);
        }
    }
}

TEST_CASE("get_and_wait") {
    SUBCASE("get_void_promises") {
        {
            auto p = pr::make_resolved_promise();
            REQUIRE_NOTHROW(p.get());
        }
        {
            auto p = pr::make_rejected_promise<void>(std::logic_error("hello fail"));
            REQUIRE_THROWS_AS(p.get(), std::logic_error);
        }
        {
            auto p = pr::promise<void>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                p.resolve();
            }};
            t.join();
            REQUIRE_NOTHROW(p.get());
        }
        {
            auto p = pr::promise<void>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                p.resolve();
            }};
            REQUIRE_NOTHROW(p.get());
        }
        {
            const auto time_now = [](){
                return std::chrono::high_resolution_clock::now();
            };

            auto p1 = pr::make_resolved_promise();
            REQUIRE_NOTHROW(p1.wait());
            REQUIRE(p1.wait_for(std::chrono::milliseconds(-1)) == pr::promise_wait_status::no_timeout);
            REQUIRE(p1.wait_for(std::chrono::milliseconds(0)) == pr::promise_wait_status::no_timeout);
            REQUIRE(p1.wait_until(time_now() + std::chrono::milliseconds(-1)) == pr::promise_wait_status::no_timeout);
            REQUIRE(p1.wait_until(time_now() + std::chrono::milliseconds(0)) == pr::promise_wait_status::no_timeout);

            auto p2 = pr::make_resolved_promise<int>(5);
            REQUIRE_NOTHROW(p2.wait());
            REQUIRE(p2.wait_for(std::chrono::milliseconds(-1)) == pr::promise_wait_status::no_timeout);
            REQUIRE(p2.wait_for(std::chrono::milliseconds(0)) == pr::promise_wait_status::no_timeout);
            REQUIRE(p2.wait_until(time_now() + std::chrono::milliseconds(-1)) == pr::promise_wait_status::no_timeout);
            REQUIRE(p2.wait_until(time_now() + std::chrono::milliseconds(0)) == pr::promise_wait_status::no_timeout);
        }
        {
            auto p = pr::promise<void>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                p.resolve();
            }};
            REQUIRE(p.wait_for(
                std::chrono::milliseconds(5))
                == pr::promise_wait_status::timeout);
            REQUIRE(p.wait_for(
                std::chrono::milliseconds(200))
                == pr::promise_wait_status::no_timeout);
        }
        {
            auto p = pr::promise<void>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                p.resolve();
            }};
            REQUIRE(p.wait_until(
                std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(5))
                == pr::promise_wait_status::timeout);
            REQUIRE(p.wait_until(
                std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(200))
                == pr::promise_wait_status::no_timeout);
        }
    }
    SUBCASE("get_typed_promises") {
        {
            auto p = pr::make_resolved_promise(42);
            REQUIRE(p.get() == 42);
        }
        {
            auto p = pr::make_rejected_promise<int>(std::logic_error("hello fail"));
            REQUIRE_THROWS_AS(p.get(), std::logic_error);
        }
        {
            auto p = pr::promise<int>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                p.resolve(42);
            }};
            t.join();
            REQUIRE(p.get() == 42);
        }
        {
            auto p = pr::promise<int>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                p.resolve(42);
            }};
            REQUIRE(p.get() == 42);
        }
        {
            auto p = pr::promise<int>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                p.resolve(42);
            }};
            REQUIRE(p.wait_for(
                std::chrono::milliseconds(5))
                == pr::promise_wait_status::timeout);
            REQUIRE(p.wait_for(
                std::chrono::milliseconds(200))
                == pr::promise_wait_status::no_timeout);
            REQUIRE(p.get() == 42);
        }
        {
            auto p = pr::promise<int>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                p.resolve(42);
            }};
            REQUIRE(p.wait_until(
                std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(5))
                == pr::promise_wait_status::timeout);
            REQUIRE(p.wait_until(
                std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(200))
                == pr::promise_wait_status::no_timeout);
            REQUIRE(p.get() == 42);
        }
    }
    SUBCASE("get_or_default") {
        {
            auto p = pr::make_resolved_promise(42);
            REQUIRE(p.get_or_default(84) == 42);
        }
        {
            auto p = pr::make_rejected_promise<int>(std::logic_error("hello fail"));
            REQUIRE(p.get_or_default(84) == 84);
        }
        {
            auto p = pr::promise<int>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                p.resolve(42);
            }};
            REQUIRE(p.get_or_default(84) == 42);
        }
        {
            auto p = pr::promise<int>();
            auto_thread t{[p]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                p.reject(std::logic_error("hello fail"));
            }};
            REQUIRE(p.get_or_default(84) == 84);
        }
        {
            auto p = pr::make_resolved_promise();
            REQUIRE_NOTHROW(p.get_or_default());
        }
        {
            auto p = pr::make_rejected_promise<void>(std::logic_error("hello fail"));
            REQUIRE_NOTHROW(p.get_or_default());
        }
    }
}

TEST_CASE("promise_transformations") {
    {
        auto p_v = pr::promise<int>()
            .then([](int){});
        static_assert(
            std::is_same<decltype(p_v)::value_type, void>::value,
            "unit test fail");

        auto p_f = pr::promise<int>()
            .then([](int){return 1.f;});
        static_assert(
            std::is_same<decltype(p_f)::value_type, float>::value,
            "unit test fail");

        auto p_d = pr::promise<int>()
            .then([](int){return 1.f;})
            .then([](float){return 1.0;});
        static_assert(
            std::is_same<decltype(p_d)::value_type, double>::value,
            "unit test fail");
    }
    {
        auto p_v = pr::promise<void>()
            .then([](){});
        static_assert(
            std::is_same<decltype(p_v)::value_type, void>::value,
            "unit test fail");

        auto p_f = pr::promise<void>()
            .then([](){return 1.f;});
        static_assert(
            std::is_same<decltype(p_f)::value_type, float>::value,
            "unit test fail");

        auto p_d = pr::promise<void>()
            .then([](){return 1.f;})
            .then([](float){return 1.0;});
        static_assert(
            std::is_same<decltype(p_d)::value_type, double>::value,
            "unit test fail");
    }
    SUBCASE("after_except") {
        {
            auto p_v = pr::promise<int>()
                .then([](int)->int{
                    throw std::logic_error("hello fail");
                })
                .except([](std::exception_ptr){
                    return 0;
                });
            static_assert(
                std::is_same<decltype(p_v)::value_type, int>::value,
                "unit test fail");
        }
        {
            auto p_v = pr::promise<int>()
                .then([](int)->int{
                    throw std::logic_error("hello fail");
                })
                .except([](std::exception_ptr){
                    return 0;
                });
            static_assert(
                std::is_same<decltype(p_v)::value_type, int>::value,
                "unit test fail");
        }
    }
}

TEST_CASE("life_after_except") {
    {
        int check_42_int = 0;
        bool call_fail_with_logic_error = false;
        auto p = pr::make_rejected_promise<int>(std::logic_error("hello fail"));
        p.then([](int v){
            return v;
        })
        .except([&call_fail_with_logic_error](std::exception_ptr e){
            call_fail_with_logic_error = check_hello_fail_exception(e);
            return 42;
        })
        .then([&check_42_int](int value){
            check_42_int = value;
        });
        REQUIRE(check_42_int == 42);
        REQUIRE(call_fail_with_logic_error);
    }
    {
        int check_42_int = 0;
        bool call_fail_with_logic_error = false;
        auto p = pr::make_rejected_promise<int>(std::logic_error("hello fail"));
        p.then([](int v){
            return v;
        })
        .except([&call_fail_with_logic_error](std::exception_ptr e) -> int {
            call_fail_with_logic_error = check_hello_fail_exception(e);
            std::rethrow_exception(e);
        })
        .except([&call_fail_with_logic_error](std::exception_ptr e){
            call_fail_with_logic_error = call_fail_with_logic_error && check_hello_fail_exception(e);
            return 42;
        })
        .then([&check_42_int](int value){
            check_42_int = value;
        });
        REQUIRE(check_42_int == 42);
        REQUIRE(call_fail_with_logic_error);
    }
    {
        bool call_then_after_except = false;
        bool call_fail_with_logic_error = false;
        auto p = pr::make_rejected_promise(std::logic_error("hello fail"));
        p.then([](){})
        .except([&call_fail_with_logic_error](std::exception_ptr e){
            call_fail_with_logic_error = check_hello_fail_exception(e);
        })
        .then([&call_then_after_except](){
            call_then_after_except = true;
        });
        REQUIRE(call_then_after_except);
        REQUIRE(call_fail_with_logic_error);
    }
    {
        bool call_fail_with_logic_error = false;
        bool call_then_after_multi_except = false;
        auto p = pr::make_rejected_promise(std::logic_error("hello fail"));
        p.then([](){})
        .except([&call_fail_with_logic_error](std::exception_ptr e){
            call_fail_with_logic_error = check_hello_fail_exception(e);
            std::rethrow_exception(e);
        })
        .except([&call_fail_with_logic_error](std::exception_ptr e){
            call_fail_with_logic_error = call_fail_with_logic_error && check_hello_fail_exception(e);
        })
        .then([&call_then_after_multi_except](){
            call_then_after_multi_except = true;
        });
        REQUIRE(call_fail_with_logic_error);
        REQUIRE(call_then_after_multi_except);
    }
}
