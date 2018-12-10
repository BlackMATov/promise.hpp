/*******************************************************************************
 * This file is part of the "promise.hpp"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018 Matvey Cherevko
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <cassert>

#include <new>
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>
#include <utility>
#include <iterator>
#include <exception>
#include <stdexcept>
#include <functional>
#include <type_traits>

//
// invoke.hpp
// https://github.com/BlackMATov/invoke.hpp
//

namespace promise_hpp
{
    #define INVOKE_HPP_NOEXCEPT_RETURN(...) \
        noexcept(noexcept(__VA_ARGS__)) { return __VA_ARGS__; }

    #define INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(...) \
        noexcept(noexcept(__VA_ARGS__)) -> decltype (__VA_ARGS__) { return __VA_ARGS__; }

    //
    // void_t
    //

    namespace invoke_hpp
    {
        namespace impl
        {
            template < typename... Args >
            struct make_void {
                using type = void;
            };
        }

        template < typename... Args >
        using void_t = typename impl::make_void<Args...>::type;
    }

    //
    // is_reference_wrapper
    //

    namespace invoke_hpp
    {
        namespace impl
        {
            template < typename T >
            struct is_reference_wrapper_impl
            : std::false_type {};

            template < typename U >
            struct is_reference_wrapper_impl<std::reference_wrapper<U>>
            : std::true_type {};
        }

        template < typename T >
        struct is_reference_wrapper
        : impl::is_reference_wrapper_impl<std::remove_cv_t<T>> {};
    }

    //
    // invoke
    //

    namespace invoke_hpp
    {
        namespace impl
        {
            //
            // invoke_member_object_impl
            //

            template
            <
                typename Base, typename F, typename Derived,
                typename std::enable_if_t<std::is_base_of<Base, std::decay_t<Derived>>::value, int> = 0
            >
            constexpr auto invoke_member_object_impl(F Base::* f, Derived&& ref)
            INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(
                std::forward<Derived>(ref).*f)

            template
            <
                typename Base, typename F, typename RefWrap,
                typename std::enable_if_t<is_reference_wrapper<std::decay_t<RefWrap>>::value, int> = 0
            >
            constexpr auto invoke_member_object_impl(F Base::* f, RefWrap&& ref)
            INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(
                ref.get().*f)

            template
            <
                typename Base, typename F, typename Pointer,
                typename std::enable_if_t<
                    !std::is_base_of<Base, std::decay_t<Pointer>>::value &&
                    !is_reference_wrapper<std::decay_t<Pointer>>::value
                , int> = 0
            >
            constexpr auto invoke_member_object_impl(F Base::* f, Pointer&& ptr)
            INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(
                (*std::forward<Pointer>(ptr)).*f)

            //
            // invoke_member_function_impl
            //

            template
            <
                typename Base, typename F, typename Derived, typename... Args,
                typename std::enable_if_t<std::is_base_of<Base, std::decay_t<Derived>>::value, int> = 0
            >
            constexpr auto invoke_member_function_impl(F Base::* f, Derived&& ref, Args&&... args)
            INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(
                (std::forward<Derived>(ref).*f)(std::forward<Args>(args)...))

            template
            <
                typename Base, typename F, typename RefWrap, typename... Args,
                typename std::enable_if_t<is_reference_wrapper<std::decay_t<RefWrap>>::value, int> = 0
            >
            constexpr auto invoke_member_function_impl(F Base::* f, RefWrap&& ref, Args&&... args)
            INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(
                (ref.get().*f)(std::forward<Args>(args)...))

            template
            <
                typename Base, typename F, typename Pointer, typename... Args,
                typename std::enable_if_t<
                    !std::is_base_of<Base, std::decay_t<Pointer>>::value &&
                    !is_reference_wrapper<std::decay_t<Pointer>>::value
                , int> = 0
            >
            constexpr auto invoke_member_function_impl(F Base::* f, Pointer&& ptr, Args&&... args)
            INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(
                ((*std::forward<Pointer>(ptr)).*f)(std::forward<Args>(args)...))
        }

        template
        <
            typename F, typename... Args,
            typename std::enable_if_t<!std::is_member_pointer<std::decay_t<F>>::value, int> = 0
        >
        constexpr auto invoke(F&& f, Args&&... args)
        INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(
            std::forward<F>(f)(std::forward<Args>(args)...))

        template
        <
            typename F, typename T,
            typename std::enable_if_t<std::is_member_object_pointer<std::decay_t<F>>::value, int> = 0
        >
        constexpr auto invoke(F&& f, T&& t)
        INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(
            impl::invoke_member_object_impl(std::forward<F>(f), std::forward<T>(t)))

        template
        <
            typename F, typename... Args,
            typename std::enable_if_t<std::is_member_function_pointer<std::decay_t<F>>::value, int> = 0
        >
        constexpr auto invoke(F&& f, Args&&... args)
        INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN(
            impl::invoke_member_function_impl(std::forward<F>(f), std::forward<Args>(args)...))
    }

    //
    // invoke_result
    //

    namespace invoke_hpp
    {
        namespace impl
        {
            struct invoke_result_impl_tag {};

            template < typename Void, typename F, typename... Args >
            struct invoke_result_impl {};

            template < typename F, typename... Args >
            struct invoke_result_impl<void_t<invoke_result_impl_tag, decltype(invoke_hpp::invoke(std::declval<F>(), std::declval<Args>()...))>, F, Args...> {
                using type = decltype(invoke_hpp::invoke(std::declval<F>(), std::declval<Args>()...));
            };
        }

        template < typename F, typename... Args >
        struct invoke_result
        : impl::invoke_result_impl<void, F, Args...> {};

        template < typename F, typename... Args >
        using invoke_result_t = typename invoke_result<F, Args...>::type;
    }

    //
    // is_invocable
    //

    namespace invoke_hpp
    {
        namespace impl
        {
            struct is_invocable_r_impl_tag {};

            template < typename Void, typename R, typename F, typename... Args >
            struct is_invocable_r_impl
            : std::false_type {};

            template < typename R, typename F, typename... Args >
            struct is_invocable_r_impl<void_t<is_invocable_r_impl_tag, invoke_result_t<F, Args...>>, R, F, Args...>
            : std::conditional_t<
                std::is_void<R>::value,
                std::true_type,
                std::is_convertible<invoke_result_t<F, Args...>, R>> {};
        }

        template < typename R, typename F, typename... Args >
        struct is_invocable_r
        : impl::is_invocable_r_impl<void, R, F, Args...> {};

        template < typename F, typename... Args >
        using is_invocable = is_invocable_r<void, F, Args...>;
    }

    //
    // apply
    //

    namespace invoke_hpp
    {
        namespace impl
        {
            template < typename F, typename Tuple, std::size_t... I >
            constexpr decltype(auto) apply_impl(F&& f, Tuple&& args, std::index_sequence<I...>)
            INVOKE_HPP_NOEXCEPT_RETURN(
                invoke_hpp::invoke(
                    std::forward<F>(f),
                    std::get<I>(std::forward<Tuple>(args))...))
        }

        template < typename F, typename Tuple >
        constexpr decltype(auto) apply(F&& f, Tuple&& args)
        INVOKE_HPP_NOEXCEPT_RETURN(
            impl::apply_impl(
                std::forward<F>(f),
                std::forward<Tuple>(args),
                std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>()))
    }

    #undef INVOKE_HPP_NOEXCEPT_RETURN
    #undef INVOKE_HPP_NOEXCEPT_DECLTYPE_RETURN
}

//
// promise.hpp
// https://github.com/BlackMATov/promise.hpp
//

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
    // detail
    //

    namespace detail
    {
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

            ~storage() noexcept(std::is_nothrow_destructible<T>::value) {
                if ( initialized_ ) {
                    ptr_()->~T();
                }
            }

            template < typename U >
            void set(U&& value) noexcept(std::is_nothrow_constructible<T,U&&>::value) {
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
    }

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

        promise(const promise&) noexcept = default;
        promise& operator=(const promise&) noexcept = default;

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

        template < typename ResolveF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF,T> >
        std::enable_if_t<
            is_promise<ResolveFR>::value && std::is_void<typename ResolveFR::value_type>::value,
            promise<typename ResolveFR::value_type>>
        then(ResolveF&& on_resolve) {
            promise<typename ResolveFR::value_type> next;

            then([
                n = next,
                f = std::forward<ResolveF>(on_resolve)
            ](const T& v) mutable {
                auto np = invoke_hpp::invoke(
                    std::forward<decltype(f)>(f),
                    v);
                np.then([n]() mutable {
                    n.resolve();
                }, [n](std::exception_ptr e) mutable {
                    n.reject(e);
                });
            }, [n = next](std::exception_ptr e) mutable {
                n.reject(e);
            });

            return next;
        }

        template < typename ResolveF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF,T> >
        std::enable_if_t<
            is_promise<ResolveFR>::value && !std::is_void<typename ResolveFR::value_type>::value,
            promise<typename ResolveFR::value_type>>
        then(ResolveF&& on_resolve) {
            promise<typename ResolveFR::value_type> next;

            then([
                n = next,
                f = std::forward<ResolveF>(on_resolve)
            ](const T& v) mutable {
                auto np = invoke_hpp::invoke(
                    std::forward<decltype(f)>(f),
                    v);
                np.then([n](const typename ResolveFR::value_type& nv) mutable {
                    n.resolve(nv);
                }, [n](std::exception_ptr e) mutable {
                    n.reject(e);
                });
            }, [n = next](std::exception_ptr e) mutable {
                n.reject(e);
            });

            return next;
        }

        template < typename ResolveF >
        auto then_all(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ](const T& v) mutable {
                auto r = invoke_hpp::invoke(
                    std::forward<decltype(f)>(f),
                    v);
                return make_all_promise(std::move(r));
            });
        }

        template < typename ResolveF >
        auto then_any(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ](const T& v) mutable {
                auto r = invoke_hpp::invoke(
                    std::forward<decltype(f)>(f),
                    v);
                return make_any_promise(std::move(r));
            });
        }

        template < typename ResolveF
                 , typename RejectF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF,T> >
        std::enable_if_t<
            !is_promise<ResolveFR>::value,
            promise<ResolveFR>>
        then(ResolveF&& on_resolve, RejectF&& on_reject) {
            promise<ResolveFR> next;
            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                std::forward<RejectF>(on_reject));
            return next;
        }

        template < typename ResolveF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF,T> >
        std::enable_if_t<
            !is_promise<ResolveFR>::value,
            promise<ResolveFR>>
        then(ResolveF&& on_resolve) {
            return then(
                std::forward<ResolveF>(on_resolve),
                [](std::exception_ptr){});
        }

        template < typename RejectF >
        promise<T> except(RejectF&& on_reject) {
            return then(
                [](const T& value) { return value; },
                std::forward<RejectF>(on_reject));
        }

        template < typename U >
        bool resolve(U&& value) {
            return state_->resolve(
                std::forward<U>(value));
        }

        bool reject(std::exception_ptr e) noexcept {
            return state_->reject(e);
        }

        template < typename E >
        bool reject(E&& e) {
            return state_->reject(
                std::make_exception_ptr(std::forward<E>(e)));
        }
    private:
        class state;
        std::shared_ptr<state> state_;
    private:
        class state final : private detail::noncopyable {
        public:
            state() = default;

            template < typename U >
            bool resolve(U&& value) {
                std::lock_guard<std::mutex> guard(mutex_);
                if ( status_ != status::pending ) {
                    return false;
                }
                storage_.set(std::forward<U>(value));
                status_ = status::resolved;
                invoke_resolve_handlers_();
                return true;
            }

            bool reject(std::exception_ptr e) noexcept {
                std::lock_guard<std::mutex> guard(mutex_);
                if ( status_ != status::pending ) {
                    return false;
                }
                exception_ = e;
                status_ = status::rejected;
                invoke_reject_handlers_();
                return true;
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<std::is_void<U>::value, void>
            attach(promise<U>& next, ResolveF&& resolve, RejectF&& reject) {
                auto reject_h = [
                    n = next,
                    f = std::forward<RejectF>(reject)
                ](std::exception_ptr e) mutable {
                    try {
                        invoke_hpp::invoke(
                            std::forward<decltype(f)>(f),
                            e);
                        n.reject(e);
                    } catch (...) {
                        n.reject(std::current_exception());
                    }
                };

                auto resolve_h = [
                    n = next,
                    f = std::forward<ResolveF>(resolve),
                    j = reject_h
                ](const T& v) mutable {
                    try {
                        invoke_hpp::invoke(
                            std::forward<decltype(f)>(f),
                            v);
                        n.resolve();
                    } catch (...) {
                        invoke_hpp::invoke(
                            std::move(j),
                            std::current_exception());
                    }
                };

                std::lock_guard<std::mutex> guard(mutex_);
                add_handlers_(std::move(resolve_h), std::move(reject_h));
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<!std::is_void<U>::value, void>
            attach(promise<U>& next, ResolveF&& resolve, RejectF&& reject) {
                auto reject_h = [
                    n = next,
                    f = std::forward<RejectF>(reject)
                ](std::exception_ptr e) mutable {
                    try {
                        invoke_hpp::invoke(
                            std::forward<decltype(f)>(f),
                            e);
                        n.reject(e);
                    } catch (...) {
                        n.reject(std::current_exception());
                    }
                };

                auto resolve_h = [
                    n = next,
                    f = std::forward<ResolveF>(resolve),
                    j = reject_h
                ](const T& v) mutable {
                    try {
                        auto r = invoke_hpp::invoke(
                            std::forward<decltype(f)>(f),
                            v);
                        n.resolve(std::move(r));
                    } catch (...) {
                        invoke_hpp::invoke(
                            std::move(j),
                            std::current_exception());
                    }
                };

                std::lock_guard<std::mutex> guard(mutex_);
                add_handlers_(std::move(resolve_h), std::move(reject_h));
            }
        private:
            template < typename ResolveF, typename RejectF >
            void add_handlers_(ResolveF&& resolve, RejectF&& reject) {
                if ( status_ == status::resolved ) {
                    invoke_hpp::invoke(
                        std::forward<ResolveF>(resolve),
                        storage_.value());
                } else if ( status_ == status::rejected ) {
                    invoke_hpp::invoke(
                        std::forward<RejectF>(reject),
                        exception_);
                } else {
                    handlers_.emplace_back(
                        std::forward<ResolveF>(resolve),
                        std::forward<RejectF>(reject));
                }
            }

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
            detail::storage<T> storage_;
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

        promise(const promise&) noexcept = default;
        promise& operator=(const promise&) noexcept = default;

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

        template < typename ResolveF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF> >
        std::enable_if_t<
            is_promise<ResolveFR>::value && std::is_void<typename ResolveFR::value_type>::value,
            promise<typename ResolveFR::value_type>>
        then(ResolveF&& on_resolve) {
            promise<typename ResolveFR::value_type> next;

            then([
                n = next,
                f = std::forward<ResolveF>(on_resolve)
            ]() mutable {
                auto np = invoke_hpp::invoke(
                    std::forward<decltype(f)>(f));
                np.then([n]() mutable {
                    n.resolve();
                }, [n](std::exception_ptr e) mutable {
                    n.reject(e);
                });
            }, [n = next](std::exception_ptr e) mutable {
                n.reject(e);
            });

            return next;
        }

        template < typename ResolveF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF> >
        std::enable_if_t<
            is_promise<ResolveFR>::value && !std::is_void<typename ResolveFR::value_type>::value,
            promise<typename ResolveFR::value_type>>
        then(ResolveF&& on_resolve) {
            promise<typename ResolveFR::value_type> next;

            then([
                n = next,
                f = std::forward<ResolveF>(on_resolve)
            ]() mutable {
                auto np = invoke_hpp::invoke(
                    std::forward<decltype(f)>(f));
                np.then([n](const typename ResolveFR::value_type& nv) mutable {
                    n.resolve(nv);
                }, [n](std::exception_ptr e) mutable {
                    n.reject(e);
                });
            }, [n = next](std::exception_ptr e) mutable {
                n.reject(e);
            });

            return next;
        }

        template < typename ResolveF >
        auto then_all(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ]() mutable {
                auto r = invoke_hpp::invoke(
                    std::forward<decltype(f)>(f));
                return make_all_promise(std::move(r));
            });
        }

        template < typename ResolveF >
        auto then_any(ResolveF&& on_resolve) {
            return then([
                f = std::forward<ResolveF>(on_resolve)
            ]() mutable {
                auto r = invoke_hpp::invoke(
                    std::forward<decltype(f)>(f));
                return make_any_promise(std::move(r));
            });
        }

        template < typename ResolveF
                 , typename RejectF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF> >
        std::enable_if_t<
            !is_promise<ResolveFR>::value,
            promise<ResolveFR>>
        then(ResolveF&& on_resolve, RejectF&& on_reject) {
            promise<ResolveFR> next;
            state_->attach(
                next,
                std::forward<ResolveF>(on_resolve),
                std::forward<RejectF>(on_reject));
            return next;
        }

        template < typename ResolveF
                 , typename ResolveFR = invoke_hpp::invoke_result_t<ResolveF> >
        std::enable_if_t<
            !is_promise<ResolveFR>::value,
            promise<ResolveFR>>
        then(ResolveF&& on_resolve) {
            return then(
                std::forward<ResolveF>(on_resolve),
                [](std::exception_ptr){});
        }

        template < typename RejectF >
        promise<void> except(RejectF&& on_reject) {
            return then(
                []{},
                std::forward<RejectF>(on_reject));
        }

        bool resolve() {
            return state_->resolve();
        }

        bool reject(std::exception_ptr e) noexcept {
            return state_->reject(e);
        }

        template < typename E >
        bool reject(E&& e) {
            return state_->reject(
                std::make_exception_ptr(std::forward<E>(e)));
        }
    private:
        class state;
        std::shared_ptr<state> state_;
    private:
        class state final : private detail::noncopyable {
        public:
            state() = default;

            bool resolve() {
                std::lock_guard<std::mutex> guard(mutex_);
                if ( status_ != status::pending ) {
                    return false;
                }
                status_ = status::resolved;
                invoke_resolve_handlers_();
                return true;
            }

            bool reject(std::exception_ptr e) noexcept {
                std::lock_guard<std::mutex> guard(mutex_);
                if ( status_ != status::pending ) {
                    return false;
                }
                exception_ = e;
                status_ = status::rejected;
                invoke_reject_handlers_();
                return true;
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<std::is_void<U>::value, void>
            attach(promise<U>& next, ResolveF&& resolve, RejectF&& reject) {
                auto reject_h = [
                    n = next,
                    f = std::forward<RejectF>(reject)
                ](std::exception_ptr e) mutable {
                    try {
                        invoke_hpp::invoke(
                            std::forward<decltype(f)>(f),
                            e);
                        n.reject(e);
                    } catch (...) {
                        n.reject(std::current_exception());
                    }
                };

                auto resolve_h = [
                    n = next,
                    f = std::forward<ResolveF>(resolve),
                    j = reject_h
                ]() mutable {
                    try {
                        invoke_hpp::invoke(
                            std::forward<decltype(f)>(f));
                        n.resolve();
                    } catch (...) {
                        invoke_hpp::invoke(
                            std::move(j),
                            std::current_exception());
                    }
                };

                std::lock_guard<std::mutex> guard(mutex_);
                add_handlers_(std::move(resolve_h), std::move(reject_h));
            }

            template < typename U, typename ResolveF, typename RejectF >
            std::enable_if_t<!std::is_void<U>::value, void>
            attach(promise<U>& next, ResolveF&& resolve, RejectF&& reject) {
                auto reject_h = [
                    n = next,
                    f = std::forward<RejectF>(reject)
                ](std::exception_ptr e) mutable {
                    try {
                        invoke_hpp::invoke(
                            std::forward<decltype(f)>(f),
                            e);
                        n.reject(e);
                    } catch (...) {
                        n.reject(std::current_exception());
                    }
                };

                auto resolve_h = [
                    n = next,
                    f = std::forward<ResolveF>(resolve),
                    j = reject_h
                ]() mutable {
                    try {
                        auto r = invoke_hpp::invoke(
                            std::forward<decltype(f)>(f));
                        n.resolve(std::move(r));
                    } catch (...) {
                        invoke_hpp::invoke(
                            std::move(j),
                            std::current_exception());
                    }
                };

                std::lock_guard<std::mutex> guard(mutex_);
                add_handlers_(std::move(resolve_h), std::move(reject_h));
            }
        private:
            template < typename ResolveF, typename RejectF >
            void add_handlers_(ResolveF&& resolve, RejectF&& reject) {
                if ( status_ == status::resolved ) {
                    invoke_hpp::invoke(
                        std::forward<ResolveF>(resolve));
                } else if ( status_ == status::rejected ) {
                    invoke_hpp::invoke(
                        std::forward<RejectF>(reject),
                        exception_);
                } else {
                    handlers_.emplace_back(
                        std::forward<ResolveF>(resolve),
                        std::forward<RejectF>(reject));
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
            invoke_hpp::invoke(
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

    template < typename Iter >
    auto make_all_promise(Iter begin, Iter end) {
        using child_promise_t = typename Iter::value_type;
        using child_promise_value_t = typename child_promise_t::value_type;
        using promise_out_container_t = std::vector<child_promise_value_t>;

        struct context_t {
            promise_out_container_t results;
            std::atomic_size_t counter = ATOMIC_VAR_INIT(0);

            context_t(std::size_t count)
            : results(count) {}

            bool apply_result(std::size_t index, const child_promise_value_t& value) {
                results[index] = value;
                return ++counter == results.size();
            }
        };

        if ( begin == end ) {
            return make_resolved_promise(promise_out_container_t());
        }

        return make_promise<promise_out_container_t>([begin, end](auto&& resolver, auto&& rejector){
            std::size_t result_index = 0;
            auto context = std::make_shared<context_t>(std::distance(begin, end));
            for ( auto iter = begin; iter != end; ++iter, ++result_index ) {
                (*iter).then([
                    context,
                    resolver,
                    result_index
                ](const child_promise_value_t& v) mutable {
                    if ( context->apply_result(result_index, v) ) {
                        resolver(std::move(context->results));
                    }
                }, rejector);
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

    template < typename Iter >
    auto make_any_promise(Iter begin, Iter end) {
        using child_promise_t = typename Iter::value_type;
        using child_promise_value_t = typename child_promise_t::value_type;

        if ( begin == end ) {
            throw std::logic_error("at least one input promise must be provided for make_any_promise");
        }

        return make_promise<child_promise_value_t>([begin, end](auto&& resolver, auto&& rejector){
            for ( auto iter = begin; iter != end; ++iter ) {
                (*iter).then(resolver, rejector);
            }
        });
    }

    template < typename Container >
    auto make_any_promise(Container&& container) {
        return make_any_promise(
            std::begin(container),
            std::end(container));
    }
}

namespace std
{
    template < typename T >
    struct hash<promise_hpp::promise<T>>
        : std::unary_function<promise_hpp::promise<T>, std::size_t>
    {
        std::size_t operator()(const promise_hpp::promise<T>& p) const noexcept {
            return p.hash();
        }
    };
}
