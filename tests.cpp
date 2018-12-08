#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "promise.hpp"
namespace pr = promise_hpp;

namespace
{
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

TEST_CASE("promise") {
    SECTION("resolved") {
        {
            int check_42_int = 0;
            pr::promise<int>()
            .resolve(42)
            .then([&check_42_int](int value){
                check_42_int = value;
            });
            REQUIRE(check_42_int == 42);
        }
        {
            int check_42_int = 0;
            pr::promise<int>()
            .resolve(42)
            .fail([](std::exception_ptr){
            }).then([&check_42_int](int value){
                check_42_int = value;
            });
            REQUIRE(check_42_int == 42);
        }
        {
            int check_42_int = 0;
            pr::promise<int>()
            .resolve(42)
            .then([&check_42_int](int value){
                check_42_int = value;
            }, [](std::exception_ptr){
            });
            REQUIRE(check_42_int == 42);
        }
        {
            int check_84_int = 0;
            bool check_void_call = false;
            int check_100500_transform = 0;
            pr::promise<int>()
            .resolve(42)
            .then([](int value){
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
            pr::promise<int>()
            .reject(std::logic_error("hello fail"))
            .then([&not_call_then_on_reject](int value) {
                (void)value;
                not_call_then_on_reject = false;
            }).fail([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(not_call_then_on_reject);
            REQUIRE(call_fail_with_logic_error);
        }
        {
            std::logic_error ee("hello fail");
            bool call_fail_with_logic_error = false;
            pr::promise<int>()
            .reject(ee)
            .then([](int){
            }).fail([&call_fail_with_logic_error](const std::exception_ptr& e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
        {
            std::logic_error ee("hello fail");
            bool call_fail_with_logic_error = false;
            pr::promise<int>()
            .reject(std::make_exception_ptr(ee))
            .then([](int){
            }).fail([&call_fail_with_logic_error](const std::exception_ptr& e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
        {
            int check_multi_fail = 0;
            pr::promise<>()
            .reject(std::logic_error("hello fail"))
            .fail([&check_multi_fail](std::exception_ptr){
                ++check_multi_fail;
            }).fail([&check_multi_fail](std::exception_ptr){
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
            }).fail([&call_fail_with_logic_error](std::exception_ptr e){
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
            p.fail([&call_fail_with_logic_error](std::exception_ptr e){
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
            p.fail([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
            });
            REQUIRE(call_fail_with_logic_error);
        }
    }
    SECTION("exceptions") {
        {
            bool not_call_then_on_reject = true;
            bool call_fail_with_logic_error = false;
            pr::promise<int>()
            .resolve(42)
            .then([](int){
                throw std::logic_error("hello fail");
            }).then([&not_call_then_on_reject](){
                not_call_then_on_reject = false;
            }).fail([&call_fail_with_logic_error](std::exception_ptr e){
                call_fail_with_logic_error = check_hello_fail_exception(e);
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
                }).fail([&pa_value](std::exception_ptr e){
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
}
