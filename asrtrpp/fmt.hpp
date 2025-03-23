#pragma once

#include "../asrtr/status.h"
#include "../asrtr/status_to_str.h"

#include <format>

template <>
struct std::formatter< enum asrtr_status, char >
{
        template < class ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
                return ctx.begin();
        }

        auto format( enum asrtr_status status, auto& ctx ) const
        {
                return std::format_to( ctx.out(), "{}", asrtr_status_to_str( status ) );
        }
};
