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

TEST_CASE("promise2") {
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
}
