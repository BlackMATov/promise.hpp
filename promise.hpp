#pragma once

#include <cstdint>
#include <cassert>

#include <new>
#include <mutex>
#include <memory>
#include <vector>
#include <utility>
#include <exception>
#include <stdexcept>
#include <functional>
#include <type_traits>

#include "invoke.hpp"

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

    //
    // promise<T>
    //

    template < typename T >
    class promise final {
    public:
        using value_type = T;

        enum class status : std::uint8_t {
            pending,
            resolved,
            rejected
        };
    public:
        promise()
        : state_(std::make_shared<state>()) {}

        template < typename ResolveF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF,T> >
        promise<ResolveFR> then(ResolveF&& on_resolve) {
            promise<ResolveFR> next;
            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                [](std::exception_ptr){});
            return next;
        }

        template < typename ResolveF
                 , typename RejectF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF,T> >
        promise<ResolveFR> then(ResolveF&& on_resolve, RejectF&& on_reject) {
            promise<ResolveFR> next;
            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                std::forward<RejectF>(on_reject));
            return next;
        }

        template < typename RejectF >
        promise<T> fail(RejectF&& on_reject) {
            promise<T> next;
            state_->attach(
                next,
                [](const T& value) { return value; },
                std::forward<RejectF>(on_reject));
            return next;
        }

        template < typename U >
        promise& resolve(U&& value) {
            state_->resolve(std::forward<U>(value));
            return *this;
        }

        promise& reject(std::exception_ptr e) {
            state_->reject(e);
            return *this;
        }

        template < typename E >
        promise& reject(E&& e) {
            state_->reject(std::make_exception_ptr(std::forward<E>(e)));
            return *this;
        }
    private:
        class state;
        std::shared_ptr<state> state_;
    private:
        class storage final {
        public:
            storage() = default;

            storage(const storage&) = delete;
            storage& operator=(const storage&) = delete;

            ~storage() noexcept(std::is_nothrow_destructible<T>::value) {
                if ( initialized_ ) {
                    ptr_()->~T();
                }
            }

            template < typename U >
            void set(U&& value) {
                assert(!initialized_);
                ::new(ptr_()) T(std::forward<U>(value));
                initialized_ = true;
            }

            const T& value() const noexcept {
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

        class state final {
        public:
            state() = default;

            state(const state&) = delete;
            state& operator=(const state&) = delete;

            template < typename U >
            void resolve(U&& value) {
                std::lock_guard<std::mutex> guard(mutex_);
                if ( status_ != status::pending ) {
                    throw std::logic_error("do not try to resolve a resolved or rejected promise");
                }
                storage_.set(std::forward<U>(value));
                status_ = status::resolved;
                invoke_resolve_handlers_();
            }

            void reject(std::exception_ptr e) {
                std::lock_guard<std::mutex> guard(mutex_);
                if ( status_ != status::pending ) {
                    throw std::logic_error("do not try to reject a resolved or rejected promise");
                }
                exception_ = e;
                status_ = status::rejected;
                invoke_reject_handlers_();
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<!std::is_void<U>::value, void>
            attach(promise<U>& other, ResolveF&& resolve, RejectF&& reject) {
                auto resolve_h = std::bind([](promise<U>& p, const ResolveF& f, const T& v){
                    try {
                        p.resolve(invoke_hpp::invoke(f, v));
                    } catch (...) {
                        p.reject(std::current_exception());
                    }
                }, other, std::forward<ResolveF>(resolve), std::placeholders::_1);

                auto reject_h = std::bind([](promise<U>& p, const RejectF& f, std::exception_ptr e){
                    try {
                        invoke_hpp::invoke(f, e);
                        p.reject(e);
                    } catch (...) {
                        p.reject(std::current_exception());
                    }
                }, other, std::forward<RejectF>(reject), std::placeholders::_1);

                std::lock_guard<std::mutex> guard(mutex_);

                if ( status_ == status::resolved ) {
                    resolve_h(storage_.value());
                } else if ( status_ == status::rejected ) {
                    reject_h(exception_);
                } else {
                    handlers_.emplace_back(
                        std::move(resolve_h),
                        std::move(reject_h));
                }
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<std::is_void<U>::value, void>
            attach(promise<U>& other, ResolveF&& resolve, RejectF&& reject) {
                auto resolve_h = std::bind([](promise<U>& p, const ResolveF& f, const T& v){
                    try {
                        invoke_hpp::invoke(f, v);
                        p.resolve();
                    } catch (...) {
                        p.reject(std::current_exception());
                    }
                }, other, std::forward<ResolveF>(resolve), std::placeholders::_1);

                auto reject_h = std::bind([](promise<U>& p, const RejectF& f, std::exception_ptr e){
                    try {
                        invoke_hpp::invoke(f, e);
                        p.reject(e);
                    } catch (...) {
                        p.reject(std::current_exception());
                    }
                }, other, std::forward<RejectF>(reject), std::placeholders::_1);

                std::lock_guard<std::mutex> guard(mutex_);

                if ( status_ == status::resolved ) {
                    resolve_h(storage_.value());
                } else if ( status_ == status::rejected ) {
                    reject_h(exception_);
                } else {
                    handlers_.emplace_back(
                        std::move(resolve_h),
                        std::move(reject_h));
                }
            }
        private:
            void invoke_resolve_handlers_() noexcept {
                const T& value = storage_.value();
                for ( const auto& h : handlers_ ) {
                    h.resolve_(value);
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
            storage storage_;
            status status_ = status::pending;
            std::exception_ptr exception_ = nullptr;

            std::mutex mutex_;

            struct handler {
                using resolve_t = std::function<void(const T&)>;
                using reject_t = std::function<void(std::exception_ptr)>;

                resolve_t resolve_;
                reject_t reject_;

                template < typename ResolveF, typename RejectF >
                handler(ResolveF&& resolve, RejectF&& reject)
                : resolve_(std::forward<ResolveF>(resolve))
                , reject_(std::forward<RejectF>(reject)) {}
            };

            std::vector<handler> handlers_;
        };
    };

    //
    // promise<void>
    //

    template <>
    class promise<void> final {
    public:
        using value_type = void;

        enum class status : std::uint8_t {
            pending,
            resolved,
            rejected
        };
    public:
        promise()
        : state_(std::make_shared<state>()) {}

        template < typename ResolveF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF> >
        promise<ResolveFR> then(ResolveF&& on_resolve) {
            promise<ResolveFR> next;
            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                [](std::exception_ptr){});
            return next;
        }

        template < typename ResolveF
                 , typename RejectF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF> >
        promise<ResolveFR> then(ResolveF&& on_resolve, RejectF&& on_reject) {
            promise<ResolveFR> next;
            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                std::forward<RejectF>(on_reject));
            return next;
        }

        template < typename RejectF >
        promise<void> fail(RejectF&& on_reject) {
            promise<void> next;
            state_->attach(
                next,
                []{},
                std::forward<RejectF>(on_reject));
            return next;
        }

        promise& resolve() {
            state_->resolve();
            return *this;
        }

        promise& reject(std::exception_ptr e) {
            state_->reject(e);
            return *this;
        }

        template < typename E >
        promise& reject(E&& e) {
            state_->reject(std::make_exception_ptr(std::forward<E>(e)));
            return *this;
        }
    private:
        class state;
        std::shared_ptr<state> state_;
    private:
        class state final {
        public:
            state() = default;

            state(const state&) = delete;
            state& operator=(const state&) = delete;

            void resolve() {
                std::lock_guard<std::mutex> guard(mutex_);
                if ( status_ != status::pending ) {
                    throw std::logic_error("do not try to resolve a resolved or rejected promise");
                }
                status_ = status::resolved;
                invoke_resolve_handlers_();
            }

            void reject(std::exception_ptr e) {
                std::lock_guard<std::mutex> guard(mutex_);
                if ( status_ != status::pending ) {
                    throw std::logic_error("do not try to reject a resolved or rejected promise");
                }
                exception_ = e;
                status_ = status::rejected;
                invoke_reject_handlers_();
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<!std::is_void<U>::value, void>
            attach(promise<U>& other, ResolveF&& resolve, RejectF&& reject) {
                auto resolve_h = std::bind([](promise<U>& p, const ResolveF& f){
                    try {
                        p.resolve(invoke_hpp::invoke(f));
                    } catch (...) {
                        p.reject(std::current_exception());
                    }
                }, other, std::forward<ResolveF>(resolve));

                auto reject_h = std::bind([](promise<U>& p, const RejectF& f, std::exception_ptr e){
                    try {
                        invoke_hpp::invoke(f, e);
                        p.reject(e);
                    } catch (...) {
                        p.reject(std::current_exception());
                    }
                }, other, std::forward<RejectF>(reject), std::placeholders::_1);

                std::lock_guard<std::mutex> guard(mutex_);

                if ( status_ == status::resolved ) {
                    resolve_h();
                } else if ( status_ == status::rejected ) {
                    reject_h(exception_);
                } else {
                    handlers_.emplace_back(
                        std::move(resolve_h),
                        std::move(reject_h));
                }
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<std::is_void<U>::value, void>
            attach(promise<U>& other, ResolveF&& resolve, RejectF&& reject) {
                auto resolve_h = std::bind([](promise<U>& p, const ResolveF& f){
                    try {
                        invoke_hpp::invoke(f);
                        p.resolve();
                    } catch (...) {
                        p.reject(std::current_exception());
                    }
                }, other, std::forward<ResolveF>(resolve));

                auto reject_h = std::bind([](promise<U>& p, const RejectF& f, std::exception_ptr e){
                    try {
                        invoke_hpp::invoke(f, e);
                        p.reject(e);
                    } catch (...) {
                        p.reject(std::current_exception());
                    }
                }, other, std::forward<RejectF>(reject), std::placeholders::_1);

                std::lock_guard<std::mutex> guard(mutex_);

                if ( status_ == status::resolved ) {
                    resolve_h();
                } else if ( status_ == status::rejected ) {
                    reject_h(exception_);
                } else {
                    handlers_.emplace_back(
                        std::move(resolve_h),
                        std::move(reject_h));
                }
            }
        private:
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
            status status_ = status::pending;
            std::exception_ptr exception_ = nullptr;

            std::mutex mutex_;

            struct handler {
                using resolve_t = std::function<void()>;
                using reject_t = std::function<void(std::exception_ptr)>;

                resolve_t resolve_;
                reject_t reject_;

                template < typename ResolveF, typename RejectF >
                handler(ResolveF&& resolve, RejectF&& reject)
                : resolve_(std::forward<ResolveF>(resolve))
                , reject_(std::forward<RejectF>(reject)) {}
            };

            std::vector<handler> handlers_;
        };
    };

    //
    // make_promise
    //

    template < typename R, typename F >
    promise<R> make_promise(F&& f) {
        promise<R> result;

        auto resolver = std::bind([](promise<R>& p, auto&& v){
            p.resolve(std::forward<decltype(v)>(v));
        }, result, std::placeholders::_1);

        auto rejector = std::bind([](promise<R>& p, auto&& e){
            p.reject(std::forward<decltype(e)>(e));
        }, result, std::placeholders::_1);

        try {
            invoke_hpp::invoke(
                std::forward<F>(f),
                std::move(resolver),
                std::move(rejector));
        } catch (...) {
            result.reject(std::current_exception());
        }

        return result;
    }

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
}
