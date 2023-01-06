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
