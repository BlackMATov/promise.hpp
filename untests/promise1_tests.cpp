/*******************************************************************************
 * This file is part of the "https://github.com/blackmatov/promise.hpp"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018-2023, by Matvey Cherevko (blackmatov@gmail.com)
 ******************************************************************************/

#include <promise.hpp/promise.hpp>
#include <doctest/doctest.h>

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

TEST_CASE("promise1") {
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
}
