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

#include "../asrtl/core_proto.h"
#include "./util.h"

#include <assert.h>
#include <deque>
#include <stdlib.h>
#include <string.h>
#include <vector>

struct collected_data
{
        std::vector< uint8_t > data;
        asrtl_chann_id         id;
};

struct collector
{
        std::deque< collected_data > data;
};

static void assert_data_ll_contain_str(
    char const*            expected,
    struct collected_data& node,
    uint32_t               offset )
{
        CHECK( expected );
        int32_t node_n = node.data.size() - offset;
        int32_t n      = strlen( expected );
        CHECK_EQ( node_n, n );

        for ( int i = 0; i < n; i++ )
                CHECK_EQ( expected[i], node.data[i + offset] );
}


static enum asrtl_status sender_collect(
    void*                  data,
    asrtl_chann_id         id,
    struct asrtl_rec_span* buff )
{
        assert( data );
        collector* c     = (collector*) data;
        uint32_t   total = 0;
        for ( struct asrtl_rec_span* seg = buff; seg; seg = seg->next )
                total += (uint32_t) ( seg->e - seg->b );

        struct collected_data p{ .id = id };
        p.data.resize( total );
        uint8_t*          dst = p.data.data();
        struct asrtl_span sp{ .b = dst, .e = dst + total };
        asrtl_rec_span_to_span( &sp, buff );
        c->data.push_back( p );
        return ASRTL_SUCCESS;
}

static void setup_sender_collector( struct asrtl_sender* s, collector* ptr )
{
        *s = (struct asrtl_sender) {
            .ptr = (void*) ptr,
            .cb  = &sender_collect,
        };
}


void assert_collected_core_hdr(
    struct collected_data&  collected,
    uint32_t                size,
    enum asrtl_message_id_e mid )
{
        REQUIRE_NE( &collected, nullptr );
        CHECK_EQ( ASRTL_CORE, collected.id );
        assert_u16( mid, collected.data.data() );
        CHECK_EQ( size, collected.data.size() );
}

void assert_collected_diag_hdr( struct collected_data& collected )
{
        CHECK_EQ( ASRTL_DIAG, collected.id );
}
