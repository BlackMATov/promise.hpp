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
}

TEST_CASE("promise3") {
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
            double check_42_double = 0.0;
            pr::make_resolved_promise()
            .then_tuple([](){
                return std::make_tuple(
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10.0));
            }).then([&check_42_double](const std::tuple<int, double>& t){
                check_42_double = std::get<0>(t) + std::get<1>(t);
            });
            REQUIRE(check_42_double == doctest::Approx(42.0).epsilon(0.01));
        }
        {
            double check_42_double = 0.0;
            pr::make_resolved_promise(42)
            .then_tuple([](int){
                return std::make_tuple(
                    pr::make_resolved_promise(32),
                    pr::make_resolved_promise(10.0));
            }).then([&check_42_double](const std::tuple<int, double>& t){
                check_42_double = static_cast<double>(std::get<0>(t)) + std::get<1>(t);
            });
            REQUIRE(check_42_double == doctest::Approx(42.0).epsilon(0.01));
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
