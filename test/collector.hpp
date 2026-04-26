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
#include "../asrtl/diag_proto.h"
#include "./util.h"

#include <assert.h>
#include <deque>
#include <stdlib.h>
#include <string.h>
#include <vector>

struct collected_data
{
        std::vector< uint8_t > data;
        asrt_chann_id          id;
};

struct collector
{
        std::deque< collected_data > data;
};

static inline void assert_data_ll_contain_str(
    char const*            expected,
    struct collected_data& node,
    uint32_t               offset )
{
        CHECK( expected );
        uint32_t node_n = node.data.size() - offset;
        uint32_t n      = strlen( expected );
        CHECK_EQ( node_n, n );

        for ( uint32_t i = 0; i < n; i++ )
                CHECK_EQ( expected[i], node.data[i + offset] );
}


static inline enum asrt_status sender_collect(
    void*                 data,
    asrt_chann_id         id,
    struct asrt_rec_span* buff,
    asrt_send_done_cb     done_cb,
    void*                 done_ptr )
{
        assert( data );
        collector* c     = (collector*) data;
        uint32_t   total = 0;
        for ( struct asrt_rec_span* seg = buff; seg; seg = seg->next )
                total += (uint32_t) ( seg->e - seg->b );

        struct collected_data p
        {
                .id = id
        };
        p.data.resize( total );
        uint8_t* dst = p.data.data();
        struct asrt_span sp
        {
                .b = dst, .e = dst + total
        };
        asrt_rec_span_to_span( &sp, buff );
        c->data.push_back( p );
        if ( done_cb )
                done_cb( done_ptr, ASRT_SUCCESS );
        return ASRT_SUCCESS;
}

static inline void setup_sender_collector( struct asrt_sender* s, collector* ptr )
{
        *s = ( struct asrt_sender ){
            .ptr = (void*) ptr,
            .cb  = &sender_collect,
        };
}


inline void assert_collected_core_hdr(
    struct collected_data& collected,
    uint32_t               size,
    enum asrt_message_id_e mid )
{
        REQUIRE_NE( &collected, nullptr );
        CHECK_EQ( ASRT_CORE, collected.id );
        assert_u16( mid, collected.data.data() );
        CHECK_EQ( size, collected.data.size() );
}

inline void assert_collected_diag_hdr(
    struct collected_data&      collected,
    enum asrt_diag_message_id_e type )
{
        REQUIRE_NE( &collected, nullptr );
        CHECK_EQ( ASRT_DIAG, collected.id );
        CHECK_EQ( type, collected.data[0] );
}
