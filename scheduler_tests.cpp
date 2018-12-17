/*******************************************************************************
 * This file is part of the "https://github.com/blackmatov/promise.hpp"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018 Matvey Cherevko
 ******************************************************************************/

#define CATCH_CONFIG_FAST_COMPILE
#include "catch.hpp"

#include <thread>
#include <numeric>
#include <cstring>

#include "scheduler.hpp"
namespace sd = scheduler_hpp;

TEST_CASE("scheduler") {
    {
        sd::scheduler s;
        auto pv0 = s.schedule([](){
            throw std::exception();
        });
        s.process_all_tasks();
        REQUIRE_THROWS_AS(pv0.get(), std::exception);
    }
    {
        auto pv0 = sd::promise<int>();
        {
            sd::scheduler s;
            pv0 = s.schedule([](){
                return 42;
            });
        }
        REQUIRE_THROWS_AS(pv0.get(), sd::scheduler_cancelled_exception);
    }
    {
        sd::scheduler s;
        int counter = 0;
        s.schedule([&counter](){ ++counter; });
        REQUIRE(counter == 0);
        REQUIRE(s.process_all_tasks() == std::make_pair(
            sd::scheduler_processing_status::done,
            std::size_t(1u)));
        REQUIRE(counter == 1);
        s.schedule([&counter](){ ++counter; });
        s.schedule([&counter](){ ++counter; });
        REQUIRE(counter == 1);
        REQUIRE(s.process_all_tasks() == std::make_pair(
            sd::scheduler_processing_status::done,
            std::size_t(2u)));
        REQUIRE(counter == 3);
        REQUIRE(s.process_all_tasks() == std::make_pair(
            sd::scheduler_processing_status::done,
            std::size_t(0u)));
    }
    {
        sd::scheduler s;
        int counter = 0;
        s.schedule([&counter](){ ++counter; });
        s.schedule([&counter](){ ++counter; });
        s.schedule([&counter](){ ++counter; });
        REQUIRE(counter == 0);
        REQUIRE(s.process_one_task() == std::make_pair(
            sd::scheduler_processing_status::done,
            std::size_t(1u)));
        REQUIRE(counter == 1);
        REQUIRE(s.process_one_task() == std::make_pair(
            sd::scheduler_processing_status::done,
            std::size_t(1u)));
        REQUIRE(counter == 2);
        REQUIRE(s.process_one_task() == std::make_pair(
            sd::scheduler_processing_status::done,
            std::size_t(1u)));
        REQUIRE(counter == 3);
        REQUIRE(s.process_one_task() == std::make_pair(
            sd::scheduler_processing_status::done,
            std::size_t(0u)));
        REQUIRE(counter == 3);
    }
    {
        sd::scheduler s;
        int counter = 0;
        for ( std::size_t i = 0; i < 50; ++i ) {
            s.schedule([&counter](){
                ++counter;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
        }
        REQUIRE(s.process_tasks_for(std::chrono::milliseconds(-1)) == std::make_pair(
            sd::scheduler_processing_status::timeout,
            std::size_t(0u)));
        REQUIRE(s.process_tasks_for(std::chrono::milliseconds(0)) == std::make_pair(
            sd::scheduler_processing_status::timeout,
            std::size_t(0u)));
        REQUIRE(counter == 0);
        {
            auto r = s.process_tasks_for(std::chrono::milliseconds(100));
            REQUIRE(r.first == sd::scheduler_processing_status::timeout);
            REQUIRE(r.second > 2);
            REQUIRE(r.second < 50);
            REQUIRE(counter > 2);
            REQUIRE(counter < 50);
        }
        {
            auto r = s.process_tasks_for(std::chrono::seconds(3));
            REQUIRE(r.first == sd::scheduler_processing_status::done);
            REQUIRE(r.second > 0);
            REQUIRE(r.second < 50);
            REQUIRE(counter == 50);
        }
    }
    {
        sd::scheduler s;
        int counter = 0;
        for ( std::size_t i = 0; i < 50; ++i ) {
            s.schedule([&counter](){
                ++counter;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
        }

        const auto time_now = [](){
            return std::chrono::high_resolution_clock::now();
        };

        const auto b = time_now();

        REQUIRE(s.process_tasks_until(time_now() - std::chrono::milliseconds(1)) == std::make_pair(
            sd::scheduler_processing_status::timeout,
            std::size_t(0u)));
        REQUIRE(s.process_tasks_until(time_now()) == std::make_pair(
            sd::scheduler_processing_status::timeout,
            std::size_t(0u)));
        REQUIRE(counter == 0);
        {
            auto r = s.process_tasks_until(time_now() + std::chrono::milliseconds(100));
            REQUIRE(time_now() - b > std::chrono::milliseconds(50));
            REQUIRE(r.first == sd::scheduler_processing_status::timeout);
            REQUIRE(r.second > 2);
            REQUIRE(r.second < 50);
            REQUIRE(counter > 2);
            REQUIRE(counter < 50);
        }
        {
            auto r = s.process_tasks_until(time_now() + std::chrono::seconds(3));
            REQUIRE(r.first == sd::scheduler_processing_status::done);
            REQUIRE(r.second > 0);
            REQUIRE(r.second < 50);
            REQUIRE(counter == 50);
        }
    }
    {
        sd::scheduler s;
        std::string accumulator;
        s.schedule(sd::scheduler_priority::lowest, [](std::string& acc){
            acc.append("o");
        }, std::ref(accumulator));
        s.schedule(sd::scheduler_priority::below_normal, [](std::string& acc){
            acc.append("l");
        }, std::ref(accumulator));
        s.schedule(sd::scheduler_priority::highest, [](std::string& acc){
            acc.append("h");
        }, std::ref(accumulator));
        s.schedule(sd::scheduler_priority::above_normal, [](std::string& acc){
            acc.append("e");
        }, std::ref(accumulator));
        s.schedule(sd::scheduler_priority::normal, [](std::string& acc){
            acc.append("l");
        }, std::ref(accumulator));
        REQUIRE(s.process_all_tasks() == std::make_pair(
            sd::scheduler_processing_status::done,
            std::size_t(5u)));
        REQUIRE(accumulator == "hello");
    }
}
