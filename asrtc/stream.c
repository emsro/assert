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
#include "./stream.h"

#include "../asrtl/log.h"

#include <string.h>


static void asrt_stream_clear_schema(
    struct asrt_allocator*     alloc,
    struct asrt_stream_schema* schema )
{
        // Free records
        struct asrt_stream_record* rec = schema->first;
        while ( rec ) {
                struct asrt_stream_record* next = rec->next;
                if ( rec->data )
                        asrt_free( alloc, (void**) &rec->data );
                asrt_free( alloc, (void**) &rec );
                rec = next;
        }

        // Free fields array
        if ( schema->fields )
                asrt_free( alloc, (void**) &schema->fields );
}

static void asrt_stream_free_schema(
    struct asrt_allocator*     alloc,
    struct asrt_stream_schema* schema )
{
        if ( !schema )
                return;
        asrt_stream_clear_schema( alloc, schema );
        asrt_free( alloc, (void**) &schema );
}

static enum asrt_status asrt_stream_server_handle_define(
    struct asrt_stream_server* server,
    struct asrt_span*          buff )
{
        // Need at least schema_id(1) + field_count(1)
        if ( asrt_span_unfit_for( buff, 2 ) )
                return ASRT_RECV_ERR;

        uint8_t schema_id   = *buff->b++;
        uint8_t field_count = *buff->b++;

        if ( schema_id >= ASRT_STREAM_MAX_SCHEMAS || field_count == 0 ) {
                ASRT_ERR_LOG( "asrt_stream", "define: invalid, schema_id: %u", schema_id );
                if ( asrt_send_is_req_used( server->node.send_queue, &server->err_msg.req ) )
                        return ASRT_SUCCESS;
                asrt_send_enque(
                    &server->node,
                    asrt_msg_ctor_strm_error( &server->err_msg, ASRT_STRM_ERR_INVALID_DEFINE ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }

        if ( server->lookup[schema_id] ) {
                ASRT_ERR_LOG( "asrt_stream", "define: duplicate schema_id: %u", schema_id );
                if ( asrt_send_is_req_used( server->node.send_queue, &server->err_msg.req ) )
                        return ASRT_SUCCESS;
                asrt_send_enque(
                    &server->node,
                    asrt_msg_ctor_strm_error( &server->err_msg, ASRT_STRM_ERR_DUPLICATE_SCHEMA ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }

        // Allocate schema
        struct asrt_stream_schema* schema = asrt_alloc( &server->alloc, sizeof( *schema ) );
        if ( !schema ) {
                ASRT_ERR_LOG( "asrt_stream", "define: schema alloc failed" );
                if ( asrt_send_is_req_used( server->node.send_queue, &server->err_msg.req ) )
                        return ASRT_SUCCESS;
                asrt_send_enque(
                    &server->node,
                    asrt_msg_ctor_strm_error( &server->err_msg, ASRT_STRM_ERR_ALLOC_FAILURE ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }
        *schema = ( struct asrt_stream_schema ){
            .schema_id   = schema_id,
            .field_count = field_count,
            .record_size = 0,
            .fields      = NULL,
            .first       = NULL,
            .last        = NULL,
            .count       = 0,
        };

        // Allocate fields array
        schema->fields = asrt_alloc(
            &server->alloc, (uint32_t) field_count * (uint32_t) sizeof( *schema->fields ) );
        if ( !schema->fields ) {
                ASRT_ERR_LOG( "asrt_stream", "define: fields alloc failed" );
                asrt_free( &server->alloc, (void**) &schema );
                if ( asrt_send_is_req_used( server->node.send_queue, &server->err_msg.req ) )
                        return ASRT_SUCCESS;
                asrt_send_enque(
                    &server->node,
                    asrt_msg_ctor_strm_error( &server->err_msg, ASRT_STRM_ERR_ALLOC_FAILURE ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }
        memset( schema->fields, 0, (size_t) field_count * sizeof( *schema->fields ) );

        // Parse each field: type_tag(1)
        uint16_t record_size = 0;
        if ( asrt_span_unfit_for( buff, field_count ) ) {
                ASRT_ERR_LOG( "asrt_stream", "define: truncated field tags" );
                asrt_free( &server->alloc, (void**) &schema->fields );
                asrt_free( &server->alloc, (void**) &schema );
                if ( asrt_send_is_req_used( server->node.send_queue, &server->err_msg.req ) )
                        return ASRT_SUCCESS;
                asrt_send_enque(
                    &server->node,
                    asrt_msg_ctor_strm_error( &server->err_msg, ASRT_STRM_ERR_INVALID_DEFINE ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }

        for ( uint8_t i = 0; i < field_count; i++ ) {
                uint8_t type_tag = *buff->b++;

                if ( !asrt_strm_field_valid( type_tag ) ) {
                        ASRT_ERR_LOG(
                            "asrt_stream", "define: bad type tag 0x%02x at field %u", type_tag, i );
                        asrt_free( &server->alloc, (void**) &schema->fields );
                        asrt_free( &server->alloc, (void**) &schema );
                        if ( asrt_send_is_req_used(
                                 server->node.send_queue, &server->err_msg.req ) )
                                return ASRT_SUCCESS;
                        asrt_send_enque(
                            &server->node,
                            asrt_msg_ctor_strm_error(
                                &server->err_msg, ASRT_STRM_ERR_INVALID_DEFINE ),
                            NULL,
                            NULL );
                        return ASRT_SUCCESS;
                }

                schema->fields[i] = type_tag;
                record_size += asrt_strm_field_size( type_tag );
        }

        schema->record_size       = record_size;
        server->lookup[schema_id] = schema;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_stream_server_handle_data(
    struct asrt_stream_server* server,
    struct asrt_span*          buff )
{
        if ( asrt_span_unfit_for( buff, 1 ) )
                return ASRT_RECV_ERR;

        uint8_t schema_id = *buff->b++;

        if ( schema_id >= ASRT_STREAM_MAX_SCHEMAS || !server->lookup[schema_id] ) {
                ASRT_ERR_LOG( "asrt_stream", "data: unknown schema %u", schema_id );
                if ( asrt_send_is_req_used( server->node.send_queue, &server->err_msg.req ) )
                        return ASRT_SUCCESS;
                asrt_send_enque(
                    &server->node,
                    asrt_msg_ctor_strm_error( &server->err_msg, ASRT_STRM_ERR_UNKNOWN_SCHEMA ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }

        struct asrt_stream_schema* schema    = server->lookup[schema_id];
        uint32_t                   remaining = (uint32_t) ( buff->e - buff->b );

        if ( remaining != schema->record_size ) {
                ASRT_ERR_LOG(
                    "asrt_stream",
                    "data: size mismatch for schema %u: got %u, expected %u",
                    schema_id,
                    remaining,
                    schema->record_size );
                if ( asrt_send_is_req_used( server->node.send_queue, &server->err_msg.req ) )
                        return ASRT_SUCCESS;
                asrt_send_enque(
                    &server->node,
                    asrt_msg_ctor_strm_error( &server->err_msg, ASRT_STRM_ERR_SIZE_MISMATCH ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }

        // Allocate record node and data buffer
        struct asrt_stream_record* rec = asrt_alloc( &server->alloc, (uint32_t) sizeof( *rec ) );
        if ( !rec ) {
                ASRT_ERR_LOG( "asrt_stream", "data: alloc failed for schema %u", schema_id );
                if ( asrt_send_is_req_used( server->node.send_queue, &server->err_msg.req ) )
                        return ASRT_SUCCESS;
                asrt_send_enque(
                    &server->node,
                    asrt_msg_ctor_strm_error( &server->err_msg, ASRT_STRM_ERR_ALLOC_FAILURE ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }

        rec->next = NULL;
        rec->data = asrt_alloc( &server->alloc, (uint32_t) schema->record_size );
        if ( !rec->data ) {
                asrt_free( &server->alloc, (void**) &rec );
                ASRT_ERR_LOG( "asrt_stream", "data: data alloc failed for schema %u", schema_id );
                if ( asrt_send_is_req_used( server->node.send_queue, &server->err_msg.req ) )
                        return ASRT_SUCCESS;
                asrt_send_enque(
                    &server->node,
                    asrt_msg_ctor_strm_error( &server->err_msg, ASRT_STRM_ERR_ALLOC_FAILURE ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }
        memcpy( rec->data, buff->b, schema->record_size );

        // Append to linked list
        if ( !schema->first ) {
                schema->first = rec;
                schema->last  = rec;
        } else {
                schema->last->next = rec;
                schema->last       = rec;
        }
        schema->count++;

        return ASRT_SUCCESS;
}

static enum asrt_status asrt_stream_server_recv( void* data, struct asrt_span buff )
{
        struct asrt_stream_server* server = (struct asrt_stream_server*) data;

        if ( asrt_span_unfit_for( &buff, 1 ) )
                return ASRT_SUCCESS;
        asrt_strm_message_id id = (asrt_strm_message_id) *buff.b++;

        switch ( id ) {
        case ASRT_STRM_MSG_DEFINE:
                return asrt_stream_server_handle_define( server, &buff );
        case ASRT_STRM_MSG_DATA:
                return asrt_stream_server_handle_data( server, &buff );
        default:
                ASRT_ERR_LOG( "asrt_stream", "unknown message id: %u", id );
                return ASRT_RECV_UNEXPECTED_ERR;
        }
}

static enum asrt_status asrt_stream_server_event( void* p, enum asrt_event_e e, void* arg )
{
        struct asrt_stream_server* server = (struct asrt_stream_server*) p;
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return ASRT_SUCCESS;
        case ASRT_EVENT_RECV:
                return asrt_stream_server_recv( server, *(struct asrt_span*) arg );
        }
        ASRT_ERR_LOG( "asrt_stream_server", "unexpected event: %s", asrt_event_to_str( e ) );
        return ASRT_INVALID_EVENT_ERR;
}

enum asrt_status asrt_stream_server_init(
    struct asrt_stream_server* server,
    struct asrt_node*          prev,
    struct asrt_allocator      alloc )
{
        if ( !server || !prev )
                return ASRT_INIT_ERR;
        *server = ( struct asrt_stream_server ){
            .node =
                ( struct asrt_node ){
                    .chid       = ASRT_STRM,
                    .e_cb_ptr   = server,
                    .e_cb       = asrt_stream_server_event,
                    .next       = NULL,
                    .send_queue = prev->send_queue,
                },
            .alloc = alloc,
        };
        memset( (void*) server->lookup, 0, sizeof server->lookup );
        asrt_node_link( prev, &server->node );
        return ASRT_SUCCESS;
}

struct asrt_stream_schemas asrt_stream_server_take( struct asrt_stream_server* server )
{
        struct asrt_stream_schemas result = { .schemas = NULL, .schema_count = 0 };
        if ( !server )
                return result;

        // Count defined schemas
        uint32_t count = 0;
        for ( uint16_t i = 0; i < ASRT_STREAM_MAX_SCHEMAS; i++ )
                if ( server->lookup[i] )
                        count++;
        if ( count == 0 )
                return result;

        // Allocate output array
        struct asrt_stream_schema* arr =
            asrt_alloc( &server->alloc, count * (uint32_t) sizeof( *arr ) );
        if ( !arr )
                return result;

        // Move schemas into the array, freeing the original allocations
        uint32_t idx = 0;
        for ( uint16_t i = 0; i < ASRT_STREAM_MAX_SCHEMAS; i++ ) {
                if ( server->lookup[i] ) {
                        arr[idx] = *server->lookup[i];
                        asrt_free( &server->alloc, (void**) &server->lookup[i] );
                        idx++;
                }
        }

        result.schemas      = arr;
        result.schema_count = count;
        result.alloc        = server->alloc;
        return result;
}

void asrt_stream_schemas_free( struct asrt_stream_schemas* schemas )
{
        if ( !schemas || !schemas->schemas )
                return;
        struct asrt_allocator* alloc = &schemas->alloc;
        for ( uint32_t i = 0; i < schemas->schema_count; i++ )
                asrt_stream_clear_schema( alloc, &schemas->schemas[i] );
        asrt_free( alloc, (void**) &schemas->schemas );
        schemas->schema_count = 0;
}

void asrt_stream_server_clear( struct asrt_stream_server* server )
{
        if ( !server )
                return;
        for ( uint16_t i = 0; i < ASRT_STREAM_MAX_SCHEMAS; i++ ) {
                if ( server->lookup[i] ) {
                        asrt_stream_free_schema( &server->alloc, server->lookup[i] );
                        server->lookup[i] = NULL;
                }
        }
}

void asrt_stream_server_deinit( struct asrt_stream_server* server )
{
        asrt_node_unlink( &server->node );
        asrt_stream_server_clear( server );
}
