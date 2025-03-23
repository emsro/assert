#pragma once

#include "../asrtc/status.h"
#include "../asrtc/status_to_str.h"

#include <format>

template <>
struct std::formatter< enum asrtc_status, char >
{
        template < class ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
                return ctx.begin();
        }

        auto format( enum asrtc_status status, auto& ctx ) const
        {
                return std::format_to( ctx.out(), "{}", asrtc_status_to_str( status ) );
        }
};
