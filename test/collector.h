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
#ifndef TEST_COLLECTOR_H
#define TEST_COLLECTOR_H

#include "../asrtl/core_proto.h"
#include "./util.h"

#include <stdlib.h>
#include <string.h>
#include <unity.h>

struct data_ll
{
        uint8_t*        data;
        uint32_t        data_size;
        asrtl_chann_id  id;
        struct data_ll* next;
};

void rec_free_data_ll( struct data_ll* p )
{
        if ( p->next )
                rec_free_data_ll( p->next );
        free( p->data );
        free( p );
}

void assert_data_ll_contain_str( char const* expected, struct data_ll* node, uint32_t offset )
{
        TEST_ASSERT( expected && node );
        int32_t node_n = node->data_size - offset;
        int32_t n      = strlen( expected );
        TEST_ASSERT_EQUAL( node_n, n );

        for ( int i = 0; i < n; i++ )
                TEST_ASSERT_EQUAL( expected[i], node->data[i + offset] );
}


enum asrtl_status sender_collect( void* data, asrtl_chann_id id, struct asrtl_span buff )
{
        assert( data );
        struct data_ll** lnode = (struct data_ll**) data;
        struct data_ll*  p     = malloc( sizeof( struct data_ll ) );
        p->data_size           = buff.e - buff.b;
        p->data                = malloc( p->data_size );
        memcpy( p->data, buff.b, p->data_size );
        p->id   = id;
        p->next = *lnode;
        *lnode  = p;
        return ASRTL_SUCCESS;
}

void setup_sender_collector( struct asrtl_sender* s, struct data_ll** ptr )
{
        *s = ( struct asrtl_sender ){
            .ptr = (void*) ptr,
            .cb  = &sender_collect,
        };
}

void clear_top_collected( struct data_ll** data )
{
        assert( data );
        struct data_ll* next = ( *data )->next;
        ( *data )->next      = NULL;
        rec_free_data_ll( *data );
        ( *data ) = next;
}

void clear_single_collected( struct data_ll** data )
{
        assert( data );
        TEST_ASSERT_NULL( ( *data )->next );
        rec_free_data_ll( *data );
        *data = NULL;
}

void assert_collected_hdr( struct data_ll* collected, uint32_t size, enum asrtl_message_id_e mid )
{
        TEST_ASSERT_NOT_NULL( collected );
        TEST_ASSERT_EQUAL( ASRTL_CORE, collected->id );
        assert_u16( mid, collected->data );
        TEST_ASSERT_EQUAL( size, collected->data_size );
}


#endif
