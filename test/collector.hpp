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

#include "../asrtl/chann.h"
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


static inline void drain_send_queue_ex(
    struct asrt_send_req_list* list,
    collector*                 coll,
    enum asrt_status           st )
{
        while ( list->head != NULL ) {
                struct asrt_send_req* req = list->head;
                list->head                = req->next;
                if ( list->head == NULL )
                        list->tail = NULL;
                req->next = NULL;  // free the slot

                struct collected_data p;
                p.id = req->chid;
                p.data.insert( p.data.end(), req->buff.b, req->buff.e );
                for ( uint32_t i = 0; i < req->buff.rest_count; i++ )
                        p.data.insert( p.data.end(), req->buff.rest[i].b, req->buff.rest[i].e );
                coll->data.push_back( p );

                if ( req->done_cb )
                        req->done_cb( req->done_ptr, st );
        }
}

static inline void drain_send_queue( struct asrt_send_req_list* list, collector* coll )
{
        drain_send_queue_ex( list, coll, ASRT_SUCCESS );
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
