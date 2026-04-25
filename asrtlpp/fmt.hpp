/// Permission to use, copy, modify, and/or distribute this software for any
/// purpose with or without fee is hereby granted.
///
/// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
/// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
/// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
/// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
/// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
/// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
/// PERFORMANCE OF THIS SOFTWARE.
#pragma once

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
