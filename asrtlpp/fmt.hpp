#pragma once

#include "../asrtl/ecode.h"
#include "../asrtl/ecode_to_str.h"
#include "../asrtl/source.h"
#include "../asrtl/source_to_str.h"
#include "../asrtl/status.h"
#include "../asrtl/status_to_str.h"

#include <format>

template <>
struct std::formatter< enum asrtl_status, char >
{
        template < class ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
                return ctx.begin();
        }

        auto format( enum asrtl_status status, auto& ctx ) const
        {
                return std::format_to( ctx.out(), "{}", asrtl_status_to_str( status ) );
        }
};

template <>
struct std::formatter< enum asrtl_source, char >
{
        template < class ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
                return ctx.begin();
        }

        auto format( enum asrtl_source st, auto& ctx ) const
        {
                return std::format_to( ctx.out(), "{}", asrtl_source_to_str( st ) );
        }
};

template <>
struct std::formatter< enum asrtl_ecode, char >
{
        template < class ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
                return ctx.begin();
        }

        auto format( enum asrtl_ecode ec, auto& ctx ) const
        {
                return std::format_to( ctx.out(), "{}", asrtl_ecode_to_str( ec ) );
        }
};
