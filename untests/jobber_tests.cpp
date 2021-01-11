/*******************************************************************************
 * This file is part of the "https://github.com/blackmatov/promise.hpp"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018-2021, by Matvey Cherevko (blackmatov@gmail.com)
 ******************************************************************************/

#include <promise.hpp/jobber.hpp>
#include "doctest/doctest.h"

#include <thread>
#include <numeric>
#include <cstring>

namespace jb = jobber_hpp;

TEST_CASE("jobber") {
    {
        jb::jobber j(1);
        auto pv0 = j.async([](){
            throw std::exception();
        });
        REQUIRE_THROWS_AS(pv0.get(), std::exception);
    }
    {
        auto pv0 = jb::promise<int>();
        {
            jb::jobber j{0};
            pv0 = j.async([](){
                return 42;
            });
        }
        REQUIRE_THROWS_AS(pv0.get(), jb::jobber_cancelled_exception);
    }
    {
        int v5 = 5;

        jb::jobber j(1);
        auto pv0 = j.async([](int v){
            REQUIRE(v == 5);
            throw std::exception();
        }, v5);
        REQUIRE_THROWS_AS(pv0.get(), std::exception);

        auto pv1 = j.async([](int& v){
            REQUIRE(v == 5);
            return v != 5
                ? 0
                : throw std::exception();
        }, std::ref(v5));
        REQUIRE_THROWS_AS(pv1.get(), std::exception);

        auto pv3 = j.async([](int& v){
            v = 4;
            return v;
        }, std::ref(v5));
        REQUIRE(pv3.get() == v5);
        REQUIRE(v5 == 4);
    }
    {
        const float pi = 3.14159265358979323846264338327950288f;
        jb::jobber j(1);
        auto p0 = j.async([](float angle){
            return std::sin(angle);
        }, pi);
        auto p1 = j.async([](float angle){
            return std::cos(angle);
        }, pi * 2);
        REQUIRE(p0.get() == doctest::Approx(0.f).epsilon(0.01f));
        REQUIRE(p1.get() == doctest::Approx(1.f).epsilon(0.01f));
    }
    {
        jb::jobber j(1);
        j.pause();
        jb::jobber_priority max_priority = jb::jobber_priority::highest;
        j.async([](){
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        });
        for ( std::size_t i = 0; i < 10; ++i ) {
            jb::jobber_priority p = static_cast<jb::jobber_priority>(
                i % static_cast<std::size_t>(jb::jobber_priority::highest));
            j.async(p, [&max_priority](jb::jobber_priority priority) {
                REQUIRE(priority <= max_priority);
                max_priority = priority;
            }, p);
        }
        j.resume();
        j.wait_all();
    }
    {
        jb::jobber j(1);
        std::atomic<int> counter = ATOMIC_VAR_INIT(0);
        j.pause();
        for ( std::size_t i = 0; i < 10; ++i ) {
            j.async([&counter](){
                ++counter;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
        }

        j.resume();
        REQUIRE(counter < 10);
        j.wait_all();
        REQUIRE(counter == 10);
    }
    {
        jb::jobber j(1);
        std::atomic<int> counter = ATOMIC_VAR_INIT(0);
        j.pause();
        for ( std::size_t i = 0; i < 10; ++i ) {
            j.async([&counter](){
                ++counter;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
        }
        REQUIRE(counter < 10);
        j.active_wait_all();
        REQUIRE(counter == 10);
    }
    {
        jb::jobber j(1);
        std::atomic<int> counter = ATOMIC_VAR_INIT(0);
        j.pause();
        for ( std::size_t i = 0; i < 3; ++i ) {
            j.async([&counter](){
                ++counter;
            });
        }
        REQUIRE(counter == 0);
        {
            auto r = j.active_wait_one();
            REQUIRE(r.first == jb::jobber_wait_status::no_timeout);
            REQUIRE(r.second == 1);
        }
        REQUIRE(counter == 1);
        {
            auto r = j.active_wait_one();
            REQUIRE(r.first == jb::jobber_wait_status::no_timeout);
            REQUIRE(r.second == 1);
        }
        REQUIRE(counter == 2);
        {
            auto r = j.active_wait_one();
            REQUIRE(r.first == jb::jobber_wait_status::no_timeout);
            REQUIRE(r.second == 1);
        }
        REQUIRE(counter == 3);
        {
            auto r = j.active_wait_one();
            REQUIRE(r.first == jb::jobber_wait_status::no_timeout);
            REQUIRE(r.second == 0);
        }
        REQUIRE(counter == 3);
        j.resume();
        REQUIRE(j.wait_all() == jb::jobber_wait_status::no_timeout);
        REQUIRE(j.active_wait_one().first == jb::jobber_wait_status::no_timeout);
        REQUIRE(counter == 3);
    }
    {
        jb::jobber j(1);

        const auto time_now = [](){
            return std::chrono::high_resolution_clock::now();
        };

        REQUIRE(jb::jobber_wait_status::no_timeout == j.wait_all_for(std::chrono::milliseconds(-1)));
        REQUIRE(jb::jobber_wait_status::no_timeout == j.wait_all_until(time_now() + std::chrono::milliseconds(-1)));
        REQUIRE(jb::jobber_wait_status::no_timeout == j.active_wait_all_for(std::chrono::milliseconds(-1)).first);
        REQUIRE(jb::jobber_wait_status::no_timeout == j.active_wait_all_until(time_now() + std::chrono::milliseconds(-1)).first);

        j.pause();
        j.async([]{});

        REQUIRE(jb::jobber_wait_status::timeout == j.wait_all_for(std::chrono::milliseconds(-1)));
        REQUIRE(jb::jobber_wait_status::timeout == j.wait_all_until(time_now() + std::chrono::milliseconds(-1)));
        REQUIRE(jb::jobber_wait_status::timeout == j.active_wait_all_for(std::chrono::milliseconds(-1)).first);
        REQUIRE(jb::jobber_wait_status::timeout == j.active_wait_all_until(time_now() + std::chrono::milliseconds(-1)).first);
    }
    {
        jb::jobber j(1);
        std::atomic<int> counter = ATOMIC_VAR_INIT(0);
        j.pause();
        for ( std::size_t i = 0; i < 10; ++i ) {
            j.async([&counter](){
                ++counter;
            });
        }

        const auto time_now = [](){
            return std::chrono::high_resolution_clock::now();
        };

        j.wait_all_for(std::chrono::milliseconds(10));
        j.wait_all_until(time_now() + std::chrono::milliseconds(10));
        REQUIRE(counter == 0);

        j.active_wait_all_for(std::chrono::milliseconds(10));
        j.active_wait_all_until(time_now() + std::chrono::milliseconds(10));
        REQUIRE(counter > 0);
    }
    {
        jb::jobber j(1);
        std::atomic<int> counter = ATOMIC_VAR_INIT(0);
        j.pause();
        for ( std::size_t i = 0; i < 50; ++i ) {
            j.async([&counter](){
                ++counter;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
        }

        const auto time_now = [](){
            return std::chrono::high_resolution_clock::now();
        };

        const auto b = time_now();

        j.resume();
        j.wait_all_for(std::chrono::milliseconds(100));
        REQUIRE(time_now() - b > std::chrono::milliseconds(50));
        REQUIRE(counter > 2);
        REQUIRE(counter < 50);

        j.wait_all_until(time_now() + std::chrono::seconds(3));
        REQUIRE(counter == 50);
    }
    {
        jb::jobber j(1);
        std::atomic<int> counter = ATOMIC_VAR_INIT(0);
        j.pause();
        for ( std::size_t i = 0; i < 50; ++i ) {
            j.async([&counter](){
                ++counter;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
        }

        const auto time_now = [](){
            return std::chrono::high_resolution_clock::now();
        };

        const auto b = time_now();

        j.wait_all_for(std::chrono::milliseconds(15));
        REQUIRE(time_now() - b > std::chrono::milliseconds(10));
        REQUIRE(counter == 0);

        j.wait_all_until(time_now() + std::chrono::milliseconds(15));
        REQUIRE(time_now() - b > std::chrono::milliseconds(20));
        REQUIRE(counter == 0);

        j.active_wait_all_for(std::chrono::milliseconds(100));
        REQUIRE(time_now() - b > std::chrono::milliseconds(70));
        REQUIRE(counter > 2);
        REQUIRE(counter < 50);

        j.active_wait_all_until(time_now() + std::chrono::seconds(3));
        REQUIRE(counter == 50);
    }
    {
        jb::jobber j(1);
        std::atomic<int> counter = ATOMIC_VAR_INIT(0);
        j.pause();
        for ( std::size_t i = 0; i < 30; ++i ) {
            j.async([&counter](){
                ++counter;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
        }
        j.resume();
        REQUIRE(jb::jobber_wait_status::timeout == j.wait_all_for(std::chrono::milliseconds(50)));
        REQUIRE(counter > 0);
        REQUIRE(jb::jobber_wait_status::no_timeout == j.wait_all_for(std::chrono::seconds(5)));
        REQUIRE(counter == 30);
    }
    {
        jb::jobber j(1);
        std::atomic<int> counter = ATOMIC_VAR_INIT(0);
        j.pause();
        for ( std::size_t i = 0; i < 30; ++i ) {
            j.async([&counter](){
                ++counter;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
        }
        {
            auto r = j.active_wait_all_for(std::chrono::milliseconds(50));
            REQUIRE(jb::jobber_wait_status::timeout == r.first);
            REQUIRE(r.second > 0);
        }
        REQUIRE(counter > 0);
        {
            auto r = j.active_wait_all_for(std::chrono::seconds(3));
            REQUIRE(jb::jobber_wait_status::no_timeout == r.first);
            REQUIRE(r.second > 0);
            REQUIRE(r.second < 30);
        }
        REQUIRE(counter == 30);
    }
    {
        jb::jobber j(2);
        jb::jobber g(2);

        std::vector<jb::promise<float>> jp(50);
        for ( auto& jpi : jp ) {
            jpi = j.async([&g](){
                std::vector<jb::promise<float>> gp(50);
                for ( std::size_t i = 0; i < gp.size(); ++i ) {
                    gp[i] = g.async([](float angle){
                        return std::sin(angle);
                    }, static_cast<float>(i));
                }
                return std::accumulate(gp.begin(), gp.end(), 0.f,
                    [](float r, jb::promise<float>& f){
                        return r + f.get();
                    });
            });
        }
        float r0 = std::accumulate(jp.begin(), jp.end(), 0.f,
            [](float r, jb::promise<float>& f){
                return r + f.get();
            });
        float r1 = 0.f;
        for ( std::size_t i = 0; i < 50; ++i ) {
            r1 += std::sin(static_cast<float>(i));
        }
        REQUIRE(r0 == doctest::Approx(r1 * 50.f).epsilon(0.01f));
    }
}
