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
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include "../asrtl/util.h"

#include <stdlib.h>
#include <unity.h>

void assert_u16( uint16_t val, uint8_t const* data )
{
        uint16_t tmp;
        asrtl_u8d2_to_u16( data, &tmp );
        TEST_ASSERT_EQUAL( val, tmp );
}
void assert_u32( uint32_t val, uint8_t const* data )
{
        uint32_t tmp;
        asrtl_u8d4_to_u32( data, &tmp );
        TEST_ASSERT_EQUAL( val, tmp );
}

#endif
