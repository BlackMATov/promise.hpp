/*******************************************************************************
 * This file is part of the "https://github.com/blackmatov/promise.hpp"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018-2023, by Matvey Cherevko (blackmatov@gmail.com)
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <cassert>

#include <new>
#include <tuple>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <utility>
#include <iterator>
#include <exception>
#include <stdexcept>
#include <functional>
#include <type_traits>
#include <condition_variable>

namespace promise_hpp
{
    //
    // forward declaration
    //

    template < typename T = void >
    class promise;

    //
    // is_promise
    //

    namespace impl
    {
        template < typename T >
        struct is_promise_impl
        : std::false_type {};

        template < typename R >
        struct is_promise_impl<promise<R>>
        : std::true_type {};
    }

    template < typename T >
    struct is_promise
    : impl::is_promise_impl<std::remove_cv_t<T>> {};

    template < typename T >
    inline constexpr bool is_promise_v = is_promise<T>::value;

    //
    // is_promise_r
    //

    namespace impl
    {
        template < typename R, typename T >
        struct is_promise_r_impl
        : std::false_type {};

        template < typename R, typename PR >
        struct is_promise_r_impl<R, promise<PR>>
        : std::is_convertible<PR, R> {};
    }

    template < typename R, typename T >
    struct is_promise_r
    : impl::is_promise_r_impl<R, std::remove_cv_t<T>> {};

    template < typename R, typename T >
    inline constexpr bool is_promise_r_v = is_promise_r<R, T>::value;

    //
    // promise_wait_status
    //

    enum class promise_wait_status {
        no_timeout,
        timeout
    };

    //
    // aggregate_exception
    //

    class aggregate_exception final : public std::exception {
    private:
        using exceptions_t = std::vector<std::exception_ptr>;
        using internal_state_t = std::shared_ptr<exceptions_t>;
    public:
        aggregate_exception()
        : state_(std::make_shared<exceptions_t>()) {}

        explicit aggregate_exception(exceptions_t exceptions)
        : state_(std::make_shared<exceptions_t>(std::move(exceptions))) {}

        aggregate_exception(const aggregate_exception& other) noexcept
        : state_(other.state_) {}

        aggregate_exception& operator=(const aggregate_exception& other) noexcept {
            if ( this != &other ) {
                state_ = other.state_;
            }
            return *this;
        }

        const char* what() const noexcept override {
            return "Aggregate exception";
        }

        bool empty() const noexcept {
            return (*state_).empty();
        }

        std::size_t size() const noexcept {
            return (*state_).size();
        }

        std::exception_ptr at(std::size_t index) const {
            return (*state_).at(index);
        }

        std::exception_ptr operator[](std::size_t index) const noexcept {
            return (*state_)[index];
        }
    private:
        internal_state_t state_;
    };
}

// -----------------------------------------------------------------------------
//
// detail
//
// -----------------------------------------------------------------------------

namespace promise_hpp::detail
{
    template < typename T >
    void destroy_in_place(T& ref) noexcept {
        ref.~T();
    }

    template < typename T, typename... Args >
    void construct_in_place(T& ref, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        ::new (std::addressof(ref)) T(std::forward<Args>(args)...);
    }

    class noncopyable {
    public:
        noncopyable(const noncopyable&) = delete;
        noncopyable& operator=(const noncopyable&) = delete;
    protected:
        noncopyable() = default;
        ~noncopyable() = default;
    };

    template < typename T >
    class storage final : private noncopyable {
    public:
        storage() = default;

        ~storage() noexcept {
            if ( initialized_ ) {
                destroy_in_place(*ptr_());
            }
        }

        storage& operator=(T&& value)
        noexcept(std::is_nothrow_move_constructible_v<T>) {
            assert(!initialized_);
            construct_in_place(*ptr_(), std::move(value));
            initialized_ = true;
            return *this;
        }

        storage& operator=(const T& value)
        noexcept(std::is_nothrow_copy_constructible_v<T>) {
            assert(!initialized_);
            construct_in_place(*ptr_(), value);
            initialized_ = true;
            return *this;
        }

        T& operator*() noexcept {
            assert(initialized_);
            return *ptr_();
        }

        const T& operator*() const noexcept {
            assert(initialized_);
            return *ptr_();
        }
    private:
        T* ptr_() noexcept {
            return reinterpret_cast<T*>(&data_);
        }

        const T* ptr_() const noexcept {
            return reinterpret_cast<const T*>(&data_);
        }
    private:
        std::aligned_storage_t<sizeof(T), alignof(T)> data_;
        bool initialized_ = false;
    };

    template < typename T >
    class storage<T&> final : private noncopyable {
    public:
        storage() = default;
        ~storage() = default;

        storage& operator=(T& value) noexcept {
            assert(!initialized_);
            value_ = &value;
            initialized_ = true;
            return *this;
        }

        T& operator*() noexcept {
            assert(initialized_);
            return *value_;
        }

        const T& operator*() const noexcept {
            assert(initialized_);
            return *value_;
        }
    private:
        T* value_{nullptr};
        bool initialized_ = false;
    };
}

// -----------------------------------------------------------------------------
//
// promise<T>
//
// -----------------------------------------------------------------------------

namespace promise_hpp
{
    template < typename T >
    class promise final {
    public:
        using value_type = T;

        promise()
        : state_(std::make_shared<state>()) {}

        promise(promise&&) = default;
        promise& operator=(promise&&) = default;

        promise(const promise&) = default;
        promise& operator=(const promise&) = default;

        void swap(promise& other) noexcept {
            state_.swap(other.state_);
        }

        std::size_t hash() const noexcept {
            return std::hash<state*>()(state_.get());
        }

        friend bool operator<(const promise& l, const promise& r) noexcept {
            return l.state_ < r.state_;
        }

        friend bool operator==(const promise& l, const promise& r) noexcept {
            return l.state_ == r.state_;
        }

        friend bool operator!=(const promise& l, const promise& r) noexcept {
            return l.state_ != r.state_;
        }

        //
        // get
        //

        const T& get() const {
            return state_->get();
        }

        template < typename U >
        T get_or_default(U&& def) const {
            try {
                return get();
            } catch (...) {
                return std::forward<U>(def);
            }
        }

        //
        // wait
        //

        void wait() const noexcept {
            state_->wait();
        }

        template < typename Rep, typename Period >
        promise_wait_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const {
            return state_->wait_for(timeout_duration);
        }

        template < typename Clock, typename Duration >
        promise_wait_status wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const {
            return state_->wait_until(timeout_time);
        }

        //
        // resolve/reject
        //

        template < typename U >
        bool resolve(U&& value) {
            return state_->resolve(std::forward<U>(value));
        }

        bool reject(std::exception_ptr e) noexcept {
            return state_->reject(e);
        }

        template < typename E >
        bool reject(E&& e) {
            return state_->reject(std::make_exception_ptr(std::forward<E>(e)));
        }

        //
        // then
        //

        template < typename ResolveF
                 , typename ResolveR = std::invoke_result_t<ResolveF, T> >
        std::enable_if_t<
            is_promise_v<ResolveR>,
            promise<typename ResolveR::value_type>>
        then(ResolveF&& on_resolve) {
            promise<typename ResolveR::value_type> next;

            then([
                n = next,
                f = std::forward<ResolveF>(on_resolve)
            ](auto&& v) mutable {
                auto np = std::invoke(
                    std::forward<decltype(f)>(f),
                    std::forward<decltype(v)>(v));
                std::move(np).then([n](auto&&... nvs) mutable {
                    n.resolve(std::forward<decltype(nvs)>(nvs)...);
                }).except([n](std::exception_ptr e) mutable {
                    n.reject(e);
                });
            }).except([n = next](std::exception_ptr e) mutable {
                n.reject(e);
            });

            return next;
        }

        template < typename ResolveF >
        auto then_all(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ](auto&& v) mutable {
                auto r = std::invoke(
                    std::forward<decltype(f)>(f),
                    std::forward<decltype(v)>(v));
                return make_all_promise(std::move(r));
            });
        }

        template < typename ResolveF >
        auto then_any(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ](auto&& v) mutable {
                auto r = std::invoke(
                    std::forward<decltype(f)>(f),
                    std::forward<decltype(v)>(v));
                return make_any_promise(std::move(r));
            });
        }

        template < typename ResolveF >
        auto then_race(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ](auto&& v) mutable {
                auto r = std::invoke(
                    std::forward<decltype(f)>(f),
                    std::forward<decltype(v)>(v));
                return make_race_promise(std::move(r));
            });
        }

        template < typename ResolveF >
        auto then_tuple(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ](auto&& v) mutable {
                auto r = std::invoke(
                    std::forward<decltype(f)>(f),
                    std::forward<decltype(v)>(v));
                return make_tuple_promise(std::move(r));
            });
        }

        //
        // then
        //

        template < typename ResolveF
                 , typename ResolveR = std::invoke_result_t<ResolveF, T> >
        std::enable_if_t<
            !is_promise_v<ResolveR>,
            promise<ResolveR>>
        then(ResolveF&& on_resolve) {
            promise<ResolveR> next;

            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                [](std::exception_ptr e) -> ResolveR { std::rethrow_exception(e); },
                false);

            return next;
        }

        template < typename ResolveF
                 , typename RejectF
                 , typename ResolveR = std::invoke_result_t<ResolveF, T> >
        std::enable_if_t<
            !is_promise_v<ResolveR>,
            promise<ResolveR>>
        then(ResolveF&& on_resolve, RejectF&& on_reject) {
            promise<ResolveR> next;

            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                std::forward<RejectF>(on_reject),
                true);

            return next;
        }

        //
        // except
        //

        template < typename RejectF >
        promise<T> except(RejectF&& on_reject) {
            return then(
                [](auto&& v) { return std::forward<decltype(v)>(v); },
                std::forward<RejectF>(on_reject));
        }

        //
        // finally
        //

        template < typename FinallyF >
        promise<T> finally(FinallyF&& on_finally) {
            return then([f = on_finally](auto&& v) {
                std::invoke(std::move(f));
                return std::forward<decltype(v)>(v);
            }, [f = on_finally](std::exception_ptr e) -> T {
                std::invoke(std::move(f));
                std::rethrow_exception(e);
            });
        }
    private:
        class state;
        std::shared_ptr<state> state_;
    private:
        class state final : private detail::noncopyable {
        public:
            state() = default;

            const T& get() {
                std::unique_lock lock(mutex_);
                cond_var_.wait(lock, [this](){
                    return status_ != status::pending;
                });
                if ( status_ == status::rejected ) {
                    std::rethrow_exception(exception_);
                }
                assert(status_ == status::resolved);
                return *storage_;
            }

            void wait() const noexcept {
                std::unique_lock lock(mutex_);
                cond_var_.wait(lock, [this](){
                    return status_ != status::pending;
                });
            }

            template < typename Rep, typename Period >
            promise_wait_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const {
                std::unique_lock lock(mutex_);
                return cond_var_.wait_for(lock, timeout_duration, [this](){
                    return status_ != status::pending;
                }) ? promise_wait_status::no_timeout : promise_wait_status::timeout;
            }

            template < typename Clock, typename Duration >
            promise_wait_status wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const {
                std::unique_lock lock(mutex_);
                return cond_var_.wait_until(lock, timeout_time, [this](){
                    return status_ != status::pending;
                }) ? promise_wait_status::no_timeout : promise_wait_status::timeout;
            }

            template < typename U >
            bool resolve(U&& value) {
                std::lock_guard guard(mutex_);
                if ( status_ != status::pending ) {
                    return false;
                }
                storage_ = std::forward<U>(value);
                status_ = status::resolved;
                invoke_resolve_handlers_();
                cond_var_.notify_all();
                return true;
            }

            bool reject(std::exception_ptr e) noexcept {
                std::lock_guard guard(mutex_);
                if ( status_ != status::pending ) {
                    return false;
                }
                exception_ = e;
                status_ = status::rejected;
                invoke_reject_handlers_();
                cond_var_.notify_all();
                return true;
            }
        public:
            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<std::is_void<U>::value, void>
            attach(promise<U>& next, ResolveF&& on_resolve, RejectF&& on_reject, bool has_reject) {
                auto reject_h = [
                    n = next,
                    f = std::forward<RejectF>(on_reject),
                    has_reject
                ](std::exception_ptr e) mutable {
                    if ( has_reject ) {
                        try {
                            std::invoke(
                                std::forward<decltype(f)>(f),
                                e);
                            n.resolve();
                        } catch (...) {
                            n.reject(std::current_exception());
                        }
                    } else {
                        n.reject(e);
                    }
                };

                auto resolve_h = [
                    n = next,
                    f = std::forward<ResolveF>(on_resolve)
                ](auto&& v) mutable {
                    try {
                        std::invoke(
                            std::forward<decltype(f)>(f),
                            std::forward<decltype(v)>(v));
                        n.resolve();
                    } catch (...) {
                        n.reject(std::current_exception());
                    }
                };

                std::lock_guard guard(mutex_);
                add_handlers_(std::move(resolve_h), std::move(reject_h));
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<!std::is_void<U>::value, void>
            attach(promise<U>& next, ResolveF&& on_resolve, RejectF&& on_reject, bool has_reject) {
                auto reject_h = [
                    n = next,
                    f = std::forward<RejectF>(on_reject),
                    has_reject
                ](std::exception_ptr e) mutable {
                    if ( has_reject ) {
                        try {
                            auto r = std::invoke(
                                std::forward<decltype(f)>(f),
                                e);
                            n.resolve(std::move(r));
                        } catch (...) {
                            n.reject(std::current_exception());
                        }
                    } else {
                        n.reject(e);
                    }
                };

                auto resolve_h = [
                    n = next,
                    f = std::forward<ResolveF>(on_resolve)
                ](auto&& v) mutable {
                    try {
                        auto r = std::invoke(
                            std::forward<decltype(f)>(f),
                            std::forward<decltype(v)>(v));
                        n.resolve(std::move(r));
                    } catch (...) {
                        n.reject(std::current_exception());
                    }
                };

                std::lock_guard guard(mutex_);
                add_handlers_(std::move(resolve_h), std::move(reject_h));
            }
        private:
            template < typename ResolveF, typename RejectF >
            void add_handlers_(ResolveF&& resolve, RejectF&& reject) {
                if ( status_ == status::resolved ) {
                    std::invoke(
                        std::forward<ResolveF>(resolve),
                        *storage_);
                } else if ( status_ == status::rejected ) {
                    std::invoke(
                        std::forward<RejectF>(reject),
                        exception_);
                } else {
                    handlers_.push_back({
                        std::forward<ResolveF>(resolve),
                        std::forward<RejectF>(reject)});
                }
            }

            void invoke_resolve_handlers_() noexcept {
                for ( const auto& h : handlers_ ) {
                    h.resolve_(*storage_);
                }
                handlers_.clear();
            }

            void invoke_reject_handlers_() noexcept {
                for ( const auto& h : handlers_ ) {
                    h.reject_(exception_);
                }
                handlers_.clear();
            }
        private:
            enum class status {
                pending,
                resolved,
                rejected
            };

            status status_{status::pending};
            std::exception_ptr exception_{nullptr};

            mutable std::mutex mutex_;
            mutable std::condition_variable cond_var_;

            struct handler {
                using resolve_t = std::function<void(const T&)>;
                using reject_t = std::function<void(std::exception_ptr)>;

                resolve_t resolve_;
                reject_t reject_;
            };

            detail::storage<T> storage_;
            std::vector<handler> handlers_;
        };
    };
}

// -----------------------------------------------------------------------------
//
// promise<void>
//
// -----------------------------------------------------------------------------

namespace promise_hpp
{
    template <>
    class promise<void> final {
    public:
        using value_type = void;

        promise()
        : state_(std::make_shared<state>()) {}

        promise(promise&&) = default;
        promise& operator=(promise&&) = default;

        promise(const promise&) = default;
        promise& operator=(const promise&) = default;

        void swap(promise& other) noexcept {
            state_.swap(other.state_);
        }

        std::size_t hash() const noexcept {
            return std::hash<state*>()(state_.get());
        }

        friend bool operator<(const promise& l, const promise& r) noexcept {
            return l.state_ < r.state_;
        }

        friend bool operator==(const promise& l, const promise& r) noexcept {
            return l.state_ == r.state_;
        }

        friend bool operator!=(const promise& l, const promise& r) noexcept {
            return l.state_ != r.state_;
        }

        //
        // get
        //

        void get() const {
            state_->get();
        }

        void get_or_default() const {
            try {
                return get();
            } catch (...) {
                // nothing
            }
        }

        //
        // wait
        //

        void wait() const noexcept {
            state_->wait();
        }

        template < typename Rep, typename Period >
        promise_wait_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const {
            return state_->wait_for(timeout_duration);
        }

        template < typename Clock, typename Duration >
        promise_wait_status wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const {
            return state_->wait_until(timeout_time);
        }

        //
        // resolve/reject
        //

        bool resolve() {
            return state_->resolve();
        }

        bool reject(std::exception_ptr e) noexcept {
            return state_->reject(e);
        }

        template < typename E >
        bool reject(E&& e) {
            return state_->reject(std::make_exception_ptr(std::forward<E>(e)));
        }

        //
        // then
        //

        template < typename ResolveF
                 , typename ResolveR = std::invoke_result_t<ResolveF> >
        std::enable_if_t<
            is_promise_v<ResolveR>,
            promise<typename ResolveR::value_type>>
        then(ResolveF&& on_resolve) {
            promise<typename ResolveR::value_type> next;

            then([
                n = next,
                f = std::forward<ResolveF>(on_resolve)
            ]() mutable {
                auto np = std::invoke(
                    std::forward<decltype(f)>(f));
                std::move(np).then([n](auto&&... nvs) mutable {
                    n.resolve(std::forward<decltype(nvs)>(nvs)...);
                }).except([n](std::exception_ptr e) mutable {
                    n.reject(e);
                });
            }).except([n = next](std::exception_ptr e) mutable {
                n.reject(e);
            });

            return next;
        }

        template < typename ResolveF >
        auto then_all(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ]() mutable {
                auto r = std::invoke(
                    std::forward<decltype(f)>(f));
                return make_all_promise(std::move(r));
            });
        }

        template < typename ResolveF >
        auto then_any(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ]() mutable {
                auto r = std::invoke(
                    std::forward<decltype(f)>(f));
                return make_any_promise(std::move(r));
            });
        }

        template < typename ResolveF >
        auto then_race(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ]() mutable {
                auto r = std::invoke(
                    std::forward<decltype(f)>(f));
                return make_race_promise(std::move(r));
            });
        }

        template < typename ResolveF >
        auto then_tuple(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ]() mutable {
                auto r = std::invoke(
                    std::forward<decltype(f)>(f));
                return make_tuple_promise(std::move(r));
            });
        }

        //
        // then
        //

        template < typename ResolveF
                 , typename ResolveR = std::invoke_result_t<ResolveF> >
        std::enable_if_t<
            !is_promise_v<ResolveR>,
            promise<ResolveR>>
        then(ResolveF&& on_resolve) {
            promise<ResolveR> next;

            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                [](std::exception_ptr e) -> ResolveR { std::rethrow_exception(e); },
                false);

            return next;
        }

        template < typename ResolveF
                 , typename RejectF
                 , typename ResolveR = std::invoke_result_t<ResolveF> >
        std::enable_if_t<
            !is_promise_v<ResolveR>,
            promise<ResolveR>>
        then(ResolveF&& on_resolve, RejectF&& on_reject) {
            promise<ResolveR> next;

            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                std::forward<RejectF>(on_reject),
                true);

            return next;
        }

        //
        // except
        //

        template < typename RejectF >
        promise<void> except(RejectF&& on_reject) {
            return then(
                [](){},
                std::forward<RejectF>(on_reject));
        }

        //
        // finally
        //

        template < typename FinallyF >
        promise<void> finally(FinallyF&& on_finally) {
            return then([f = on_finally]() {
                std::invoke(std::move(f));
            }, [f = on_finally](std::exception_ptr e) {
                std::invoke(std::move(f));
                std::rethrow_exception(e);
            });
        }
    private:
        class state;
        std::shared_ptr<state> state_;
    private:
        class state final : private detail::noncopyable {
        public:
            state() = default;

            void get() {
                std::unique_lock lock(mutex_);
                cond_var_.wait(lock, [this](){
                    return status_ != status::pending;
                });
                if ( status_ == status::rejected ) {
                    std::rethrow_exception(exception_);
                }
                assert(status_ == status::resolved);
            }

            void wait() const noexcept {
                std::unique_lock lock(mutex_);
                cond_var_.wait(lock, [this](){
                    return status_ != status::pending;
                });
            }

            template < typename Rep, typename Period >
            promise_wait_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const {
                std::unique_lock lock(mutex_);
                return cond_var_.wait_for(lock, timeout_duration, [this](){
                    return status_ != status::pending;
                }) ? promise_wait_status::no_timeout : promise_wait_status::timeout;
            }

            template < typename Clock, typename Duration >
            promise_wait_status wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const {
                std::unique_lock lock(mutex_);
                return cond_var_.wait_until(lock, timeout_time, [this](){
                    return status_ != status::pending;
                }) ? promise_wait_status::no_timeout : promise_wait_status::timeout;
            }

            bool resolve() {
                std::lock_guard guard(mutex_);
                if ( status_ != status::pending ) {
                    return false;
                }
                status_ = status::resolved;
                invoke_resolve_handlers_();
                cond_var_.notify_all();
                return true;
            }

            bool reject(std::exception_ptr e) noexcept {
                std::lock_guard guard(mutex_);
                if ( status_ != status::pending ) {
                    return false;
                }
                exception_ = e;
                status_ = status::rejected;
                invoke_reject_handlers_();
                cond_var_.notify_all();
                return true;
            }
        public:
            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<std::is_void<U>::value, void>
            attach(promise<U>& next, ResolveF&& on_resolve, RejectF&& on_reject, bool has_reject) {
                auto reject_h = [
                    n = next,
                    f = std::forward<RejectF>(on_reject),
                    has_reject
                ](std::exception_ptr e) mutable {
                    if ( has_reject ) {
                        try {
                            std::invoke(
                                std::forward<decltype(f)>(f),
                                e);
                            n.resolve();
                        } catch (...) {
                            n.reject(std::current_exception());
                        }
                    } else {
                        n.reject(e);
                    }
                };

                auto resolve_h = [
                    n = next,
                    f = std::forward<ResolveF>(on_resolve)
                ]() mutable {
                    try {
                        std::invoke(
                            std::forward<decltype(f)>(f));
                        n.resolve();
                    } catch (...) {
                        n.reject(std::current_exception());
                    }
                };

                std::lock_guard guard(mutex_);
                add_handlers_(std::move(resolve_h), std::move(reject_h));
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<!std::is_void<U>::value, void>
            attach(promise<U>& next, ResolveF&& on_resolve, RejectF&& on_reject, bool has_reject) {
                auto reject_h = [
                    n = next,
                    f = std::forward<RejectF>(on_reject),
                    has_reject
                ](std::exception_ptr e) mutable {
                    if ( has_reject ) {
                        try {
                            auto r = std::invoke(
                                std::forward<decltype(f)>(f),
                                e);
                            n.resolve(std::move(r));
                        } catch (...) {
                            n.reject(std::current_exception());
                        }
                    } else {
                        n.reject(e);
                    }
                };

                auto resolve_h = [
                    n = next,
                    f = std::forward<ResolveF>(on_resolve)
                ]() mutable {
                    try {
                        auto r = std::invoke(
                            std::forward<decltype(f)>(f));
                        n.resolve(std::move(r));
                    } catch (...) {
                        n.reject(std::current_exception());
                    }
                };

                std::lock_guard guard(mutex_);
                add_handlers_(std::move(resolve_h), std::move(reject_h));
            }
        private:
            template < typename ResolveF, typename RejectF >
            void add_handlers_(ResolveF&& resolve, RejectF&& reject) {
                if ( status_ == status::resolved ) {
                    std::invoke(
                        std::forward<ResolveF>(resolve));
                } else if ( status_ == status::rejected ) {
                    std::invoke(
                        std::forward<RejectF>(reject),
                        exception_);
                } else {
                    handlers_.push_back({
                        std::forward<ResolveF>(resolve),
                        std::forward<RejectF>(reject)});
                }
            }

            void invoke_resolve_handlers_() noexcept {
                for ( const auto& h : handlers_ ) {
                    h.resolve_();
                }
                handlers_.clear();
            }

            void invoke_reject_handlers_() noexcept {
                for ( const auto& h : handlers_ ) {
                    h.reject_(exception_);
                }
                handlers_.clear();
            }
        private:
            enum class status {
                pending,
                resolved,
                rejected
            };

            status status_{status::pending};
            std::exception_ptr exception_{nullptr};

            mutable std::mutex mutex_;
            mutable std::condition_variable cond_var_;

            struct handler {
                using resolve_t = std::function<void()>;
                using reject_t = std::function<void(std::exception_ptr)>;

                resolve_t resolve_;
                reject_t reject_;
            };

            std::vector<handler> handlers_;
        };
    };
}

namespace promise_hpp
{
    //
    // swap
    //

    template < typename T >
    void swap(promise<T>& l, promise<T>& r) noexcept {
        l.swap(r);
    }

    //
    // make_promise
    //

    template < typename R >
    promise<R> make_promise() {
        return promise<R>();
    }

    template < typename R, typename F >
    promise<R> make_promise(F&& f) {
        promise<R> result;

        auto resolver = [result](auto&& v) mutable {
            return result.resolve(std::forward<decltype(v)>(v));
        };

        auto rejector = [result](auto&& e) mutable {
            return result.reject(std::forward<decltype(e)>(e));
        };

        try {
            std::invoke(
                std::forward<F>(f),
                std::move(resolver),
                std::move(rejector));
        } catch (...) {
            result.reject(std::current_exception());
        }

        return result;
    }

    //
    // make_resolved_promise
    //

    inline promise<void> make_resolved_promise() {
        promise<void> result;
        result.resolve();
        return result;
    }

    template < typename R >
    promise<std::decay_t<R>> make_resolved_promise(R&& v) {
        promise<std::decay_t<R>> result;
        result.resolve(std::forward<R>(v));
        return result;
    }

    //
    // make_rejected_promise
    //

    template < typename E >
    promise<void> make_rejected_promise(E&& e) {
        promise<void> result;
        result.reject(std::forward<E>(e));
        return result;
    }

    template < typename R, typename E >
    promise<R> make_rejected_promise(E&& e) {
        promise<R> result;
        result.reject(std::forward<E>(e));
        return result;
    }

    //
    // make_all_promise
    //

    template < typename Iter
             , typename SubPromise = typename std::iterator_traits<Iter>::value_type
             , typename SubPromiseResult = typename SubPromise::value_type
             , typename ResultPromiseValueType = std::vector<SubPromiseResult> >
    promise<ResultPromiseValueType>
    make_all_promise(Iter begin, Iter end) {
        if ( begin == end ) {
            return make_resolved_promise(ResultPromiseValueType());
        }

        struct context_t {
            std::atomic_size_t success_counter{0u};
            std::vector<detail::storage<SubPromiseResult>> results;
            context_t(std::size_t count)
            : success_counter(count)
            , results(count) {}
        };

        return make_promise<ResultPromiseValueType>(
        [begin, end](auto&& resolver, auto&& rejector){
            std::size_t result_index = 0;
            auto context = std::make_shared<context_t>(std::distance(begin, end));
            for ( Iter iter = begin; iter != end; ++iter, ++result_index ) {
                (*iter).then([context, resolver, result_index](auto&& v) mutable {
                    context->results[result_index] = std::forward<decltype(v)>(v);
                    if ( !--context->success_counter ) {
                        std::vector<SubPromiseResult> results;
                        results.reserve(context->results.size());
                        for ( auto&& r : context->results ) {
                            results.push_back(std::move(*r));
                        }
                        resolver(std::move(results));
                    }
                }).except([rejector](std::exception_ptr e) mutable {
                    rejector(e);
                });
            }
        });
    }

    template < typename Container >
    auto make_all_promise(Container&& container) {
        return make_all_promise(
            std::begin(container),
            std::end(container));
    }

    //
    // make_any_promise
    //

    template < typename Iter
             , typename SubPromise = typename std::iterator_traits<Iter>::value_type
             , typename SubPromiseResult = typename SubPromise::value_type
             , typename ResultPromiseValueType = SubPromiseResult >
    promise<ResultPromiseValueType>
    make_any_promise(Iter begin, Iter end) {
        if ( begin == end ) {
            return make_rejected_promise<ResultPromiseValueType>(aggregate_exception());
        }

        struct context_t {
            std::atomic_size_t failure_counter{0u};
            std::vector<std::exception_ptr> exceptions;
            context_t(std::size_t count)
            : failure_counter(count)
            , exceptions(count) {}
        };

        return make_promise<ResultPromiseValueType>(
        [begin, end](auto&& resolver, auto&& rejector){
            std::size_t exception_index = 0;
            auto context = std::make_shared<context_t>(std::distance(begin, end));
            for ( Iter iter = begin; iter != end; ++iter, ++exception_index ) {
                (*iter).then([resolver](auto&& v) mutable {
                    resolver(std::forward<decltype(v)>(v));
                }).except([context, rejector, exception_index](std::exception_ptr e) mutable {
                    context->exceptions[exception_index] = e;
                    if ( !--context->failure_counter ) {
                        rejector(aggregate_exception(std::move(context->exceptions)));
                    }
                });
            }
        });
    }

    template < typename Container >
    auto make_any_promise(Container&& container) {
        return make_any_promise(
            std::begin(container),
            std::end(container));
    }

    //
    // make_race_promise
    //

    template < typename Iter
             , typename SubPromise = typename std::iterator_traits<Iter>::value_type
             , typename SubPromiseResult = typename SubPromise::value_type
             , typename ResultPromiseValueType = SubPromiseResult >
    promise<ResultPromiseValueType>
    make_race_promise(Iter begin, Iter end) {
        return make_promise<ResultPromiseValueType>(
        [begin, end](auto&& resolver, auto&& rejector){
            for ( Iter iter = begin; iter != end; ++iter ) {
                (*iter)
                .then(resolver)
                .except(rejector);
            }
        });
    }

    template < typename Container >
    auto make_race_promise(Container&& container) {
        return make_race_promise(
            std::begin(container),
            std::end(container));
    }

    //
    // make_tuple_promise
    //

    namespace impl
    {
        template < typename Tuple >
        struct tuple_promise_result_impl {};

        template < typename... Args >
        struct tuple_promise_result_impl<std::tuple<promise<Args>...>> {
            using type = std::tuple<Args...>;
        };

        template < typename Tuple >
        struct tuple_promise_result {
            using type = typename tuple_promise_result_impl<std::remove_cv_t<Tuple>>::type;
        };

        template < typename Tuple >
        using tuple_promise_result_t = typename tuple_promise_result<Tuple>::type;

        template < typename... ResultTypes >
        class tuple_promise_context_t final : private detail::noncopyable {
        public:
            template < std::size_t N, typename T >
            bool apply_result(T&& value) {
                std::get<N>(results_) = std::forward<T>(value);
                return ++counter_ == sizeof...(ResultTypes);
            }

            std::tuple<ResultTypes...> get_results() {
                return get_results_impl(
                    std::make_index_sequence<sizeof...(ResultTypes)>());
            }
        private:
            template < std::size_t... Is >
            std::tuple<ResultTypes...> get_results_impl(std::index_sequence<Is...>) {
                return {std::move(*std::get<Is>(results_))...};
            }
        private:
            std::atomic_size_t counter_{0};
            std::tuple<detail::storage<ResultTypes>...> results_;
        };

        template < typename... ResultTypes >
        using tuple_promise_context_ptr = std::shared_ptr<
            tuple_promise_context_t<ResultTypes...>>;

        template < std::size_t I
                 , typename Tuple
                 , typename Resolver
                 , typename Rejector
                 , typename... ResultTypes >
        promise<void> make_tuple_sub_promise_impl(
            Tuple&& tuple,
            Resolver&& resolver,
            Rejector&& rejector,
            const tuple_promise_context_ptr<ResultTypes...>& context)
        {
            return std::get<I>(tuple).then([
                context,
                resolver
            ](auto&& v) mutable {
                if (context->template apply_result<I>(std::forward<decltype(v)>(v))) {
                    resolver(context->get_results());
                }
            }).except(rejector);
        }

        template < typename Tuple
                 , std::size_t... Is
                 , typename ResultTuple = tuple_promise_result_t<std::decay_t<Tuple>> >
        std::enable_if_t<
            sizeof...(Is) == 0,
            promise<ResultTuple>>
        make_tuple_promise_impl(Tuple&&, std::index_sequence<Is...>) {
            return make_resolved_promise(ResultTuple());
        }

        template < typename Tuple
                 , std::size_t... Is
                 , typename ResultTuple = tuple_promise_result_t<std::decay_t<Tuple>> >
        std::enable_if_t<
            sizeof...(Is) != 0,
            promise<ResultTuple>>
        make_tuple_promise_impl(Tuple&& tuple, std::index_sequence<Is...>) {
            auto result = promise<ResultTuple>();

            auto resolver = [result](auto&& v) mutable {
                return result.resolve(std::forward<decltype(v)>(v));
            };

            auto rejector = [result](auto&& e) mutable {
                return result.reject(std::forward<decltype(e)>(e));
            };

            try {
                auto context = std::make_shared<tuple_promise_context_t<
                    std::tuple_element_t<Is, ResultTuple>...>>();
                auto promises = std::make_tuple(make_tuple_sub_promise_impl<Is>(
                    tuple,
                    resolver,
                    rejector,
                    context)...);
                (void)promises;
            } catch (...) {
                result.reject(std::current_exception());
            }

            return result;
        }
    }

    template < typename Tuple
             , typename ResultTuple = impl::tuple_promise_result_t<std::decay_t<Tuple>> >
    promise<ResultTuple>
    make_tuple_promise(Tuple&& tuple) {
        return impl::make_tuple_promise_impl(
            std::forward<Tuple>(tuple),
            std::make_index_sequence<std::tuple_size_v<ResultTuple>>());
    }
}

namespace std
{
    template < typename T >
    struct hash<promise_hpp::promise<T>> final {
        std::size_t operator()(const promise_hpp::promise<T>& p) const noexcept {
            return p.hash();
        }
    };
}
