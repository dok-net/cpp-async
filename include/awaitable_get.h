// © Microsoft Corporation. All rights reserved.

#pragma once

#include <coroutine>
#include <exception>
#include "awaitable_result.h"
#include "awaitable_resume_t.h"
#include "event_signal.h"

namespace details
{
    template<typename T>
    struct get_task_promise;

    template<typename T>
    struct get_task final
    {
        using promise_type = get_task_promise<T>;

        explicit get_task(std::coroutine_handle<get_task_promise<T>> handle) : m_handle{ handle } {}

        get_task(const get_task&) = delete;
        get_task(get_task&&) noexcept = delete;

        ~get_task() noexcept { m_handle.destroy(); }

        get_task& operator=(const get_task&) = delete;
        get_task& operator=(get_task&&) noexcept = delete;

        T get() const;

    private:
        std::coroutine_handle<get_task_promise<T>> m_handle;
    };

    template<typename T>
    struct get_task_promise final
    {
        get_task_promise() : m_result{}, m_done{} {}

        get_task<T> get_return_object()
        {
            return get_task<T>{ std::coroutine_handle<get_task_promise<T>>::from_promise(*this) };
        }

        constexpr std::suspend_never initial_suspend() const noexcept { return {}; }

        std::suspend_always final_suspend() noexcept
        {
            m_done.set();
            return {};
        }

        void unhandled_exception() noexcept { m_result.set_exception(std::current_exception()); }

        constexpr void return_value(T value) noexcept { m_result.set_value(std::forward<T>(value)); }

        T get()
        {
            m_done.wait();
            return m_result();
        }

    private:
        awaitable_result<T> m_result;
        event_signal m_done;
    };

    template<>
    struct get_task_promise<void> final
    {
        get_task<void> get_return_object()
        {
            return get_task<void>{ std::coroutine_handle<get_task_promise<void>>::from_promise(*this) };
        }

        constexpr std::suspend_never initial_suspend() const noexcept { return {}; }

        std::suspend_always final_suspend() noexcept
        {
            m_done.set();
            return {};
        }

        void unhandled_exception() noexcept { m_resultException = std::current_exception(); }

        constexpr void return_void() const noexcept {}

        void get() const
        {
            m_done.wait();

            if (m_resultException)
            {
                std::rethrow_exception(m_resultException);
            }
        }

    private:
        std::exception_ptr m_resultException;
        event_signal m_done;
    };

    template<typename T>
    T get_task<T>::get() const
    {
        return m_handle.promise().get();
    }
}

template<typename Awaitable>
inline awaitable_resume_t<Awaitable> awaitable_get(Awaitable awaitable)
{
    using T = awaitable_resume_t<Awaitable>;

    struct factory final
    {
        static details::get_task<T> create(Awaitable awaitable) { co_return co_await std::move(awaitable); }
    };

    return factory::create(std::move(awaitable)).get();
}
