#pragma once

#include <memory>

// https://stackoverflow.com/questions/13238050/convert-stdbind-to-function-pointer

template <typename BindFunctor, typename FuncWrapper> class scoped_raw_bind
{
public:

    typedef scoped_raw_bind<BindFunctor, FuncWrapper> this_type;

    // Make it Move-Constructible only
    scoped_raw_bind(const this_type&) = delete;
    this_type& operator=(const this_type&) = delete;
    this_type& operator=(this_type&& rhs) = delete;

    scoped_raw_bind(this_type&& rhs) : m_owning(rhs.m_owning)
    {
        rhs.m_owning = false;
    }

    scoped_raw_bind(BindFunctor b) : m_owning(false)
    {
        // Precondition - check that we don't override static data for another raw bind instance
        if (get_bind_ptr() != nullptr)
        {
            assert(false);
            return;
        }
        // Smart pointer is required because bind expression is copy-constructible but not copy-assignable
        get_bind_ptr().reset(new BindFunctor(b));
        m_owning = true;
    }

    ~scoped_raw_bind()
    {
        if (m_owning)
        {
            assert(get_bind_ptr() != nullptr);
            get_bind_ptr().reset();
        }
    }

    decltype(&FuncWrapper::call) get_raw_ptr()
    {
        return &FuncWrapper::call;
    }

    static BindFunctor& get_bind()
    {
        return *get_bind_ptr();
    }

private:

    bool m_owning;

    static std::unique_ptr<BindFunctor>& get_bind_ptr()
    {
        static std::unique_ptr<BindFunctor> s_funcPtr;
        return s_funcPtr;
    }
};

#define RAW_BIND(W, B) std::move(scoped_raw_bind<decltype(B), W<decltype(B), __COUNTER__>>(B));