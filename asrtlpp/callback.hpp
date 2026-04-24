#pragma once

#include <concepts>
#include <type_traits>

namespace asrt
{

/// A type-erasing wrapper for C-style callback + void* pairs.
///
/// Specialised for function-pointer types of the form `R(*)(void*, Args...)`.
/// Accepts either:
///   - a raw (fn_ptr, void*) pair, or
///   - any invocable whose signature matches `R(Args...)`.
///
template < typename FnPtr >
struct callback;

template < typename R, typename... Args >
struct callback< R ( * )( void*, Args... ) >
{
        using fn_type = R ( * )( void*, Args... );

        fn_type fn  = nullptr;
        void*   ptr = nullptr;

        callback() = default;

        callback( fn_type f, void* p )
          : fn( f )
          , ptr( p )
        {
        }

        template < typename CB >
                requires( !std::is_same_v< std::remove_cvref_t< CB >, callback > &&
                          std::invocable< CB&, Args... > )
        callback( CB& cb )
          : fn( &trampoline< CB > )
          , ptr( &cb )
        {
        }

        R operator()( Args... args ) const
        {
                return fn( ptr, args... );
        }

        explicit operator bool() const
        {
                return fn != nullptr;
        }

private:
        template < typename CB >
        static R trampoline( void* p, Args... args )
        {
                return ( *reinterpret_cast< CB* >( p ) )( args... );
        }
};

}  // namespace asrt
