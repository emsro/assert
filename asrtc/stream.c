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


static enum asrtl_status asrtc_stream_server_send( void* p, struct asrtl_rec_span* buff )
{
        struct asrtc_stream_server* server = (struct asrtc_stream_server*) p;
        return asrtl_send( &server->sendr, ASRTL_STRM, buff, NULL, NULL );
}

static void asrtc_stream_clear_schema(
    struct asrtl_allocator*     alloc,
    struct asrtc_stream_schema* schema )
{
        // Free records
        struct asrtc_stream_record* rec = schema->first;
        while ( rec ) {
                struct asrtc_stream_record* next = rec->next;
                if ( rec->data )
                        asrtl_free( alloc, (void**) &rec->data );
                asrtl_free( alloc, (void**) &rec );
                rec = next;
        }

        // Free fields array
        if ( schema->fields )
                asrtl_free( alloc, (void**) &schema->fields );
}

static void asrtc_stream_free_schema(
    struct asrtl_allocator*     alloc,
    struct asrtc_stream_schema* schema )
{
        if ( !schema )
                return;
        asrtc_stream_clear_schema( alloc, schema );
        asrtl_free( alloc, (void**) &schema );
}

static enum asrtl_status asrtc_stream_server_handle_define(
    struct asrtc_stream_server* server,
    struct asrtl_span*          buff )
{
        // Need at least schema_id(1) + field_count(1)
        if ( asrtl_span_unfit_for( buff, 2 ) )
                return ASRTL_RECV_ERR;

        uint8_t schema_id   = *buff->b++;
        uint8_t field_count = *buff->b++;

        if ( schema_id >= ASRTC_STREAM_MAX_SCHEMAS ) {
                ASRTL_ERR_LOG( "asrtc_stream", "define: schema_id %u out of range", schema_id );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_INVALID_DEFINE, asrtc_stream_server_send, server );
        }

        if ( field_count == 0 ) {
                ASRTL_ERR_LOG( "asrtc_stream", "define: zero field_count" );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_INVALID_DEFINE, asrtc_stream_server_send, server );
        }

        if ( server->lookup[schema_id] ) {
                ASRTL_ERR_LOG( "asrtc_stream", "define: schema %u already defined", schema_id );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_DUPLICATE_SCHEMA, asrtc_stream_server_send, server );
        }

        // Allocate schema
        struct asrtc_stream_schema* schema = asrtl_alloc( &server->alloc, sizeof( *schema ) );
        if ( !schema ) {
                ASRTL_ERR_LOG( "asrtc_stream", "define: schema alloc failed" );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_ALLOC_FAILURE, asrtc_stream_server_send, server );
        }
        *schema = ( struct asrtc_stream_schema ){
            .schema_id   = schema_id,
            .field_count = field_count,
            .record_size = 0,
            .fields      = NULL,
            .first       = NULL,
            .last        = NULL,
            .count       = 0,
        };

        // Allocate fields array
        schema->fields = asrtl_alloc(
            &server->alloc, (uint32_t) field_count * (uint32_t) sizeof( *schema->fields ) );
        if ( !schema->fields ) {
                ASRTL_ERR_LOG( "asrtc_stream", "define: fields alloc failed" );
                asrtl_free( &server->alloc, (void**) &schema );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_ALLOC_FAILURE, asrtc_stream_server_send, server );
        }
        memset( schema->fields, 0, (size_t) field_count * sizeof( *schema->fields ) );

        // Parse each field: type_tag(1)
        uint16_t record_size = 0;
        if ( asrtl_span_unfit_for( buff, field_count ) ) {
                ASRTL_ERR_LOG( "asrtc_stream", "define: truncated field tags" );
                asrtl_free( &server->alloc, (void**) &schema->fields );
                asrtl_free( &server->alloc, (void**) &schema );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_INVALID_DEFINE, asrtc_stream_server_send, server );
        }

        for ( uint8_t i = 0; i < field_count; i++ ) {
                uint8_t type_tag = *buff->b++;

                if ( !asrtl_strm_field_valid( type_tag ) ) {
                        ASRTL_ERR_LOG(
                            "asrtc_stream",
                            "define: bad type tag 0x%02x at field %u",
                            type_tag,
                            i );
                        asrtl_free( &server->alloc, (void**) &schema->fields );
                        asrtl_free( &server->alloc, (void**) &schema );
                        return asrtl_msg_ctor_strm_error(
                            ASRTL_STRM_ERR_INVALID_DEFINE, asrtc_stream_server_send, server );
                }

                schema->fields[i] = type_tag;
                record_size += asrtl_strm_field_size( type_tag );
        }

        schema->record_size       = record_size;
        server->lookup[schema_id] = schema;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_stream_server_handle_data(
    struct asrtc_stream_server* server,
    struct asrtl_span*          buff )
{
        if ( asrtl_span_unfit_for( buff, 1 ) )
                return ASRTL_RECV_ERR;

        uint8_t schema_id = *buff->b++;

        if ( schema_id >= ASRTC_STREAM_MAX_SCHEMAS || !server->lookup[schema_id] ) {
                ASRTL_ERR_LOG( "asrtc_stream", "data: unknown schema %u", schema_id );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_UNKNOWN_SCHEMA, asrtc_stream_server_send, server );
        }

        struct asrtc_stream_schema* schema    = server->lookup[schema_id];
        uint32_t                    remaining = (uint32_t) ( buff->e - buff->b );

        if ( remaining != schema->record_size ) {
                ASRTL_ERR_LOG(
                    "asrtc_stream",
                    "data: size mismatch for schema %u: got %u, expected %u",
                    schema_id,
                    remaining,
                    schema->record_size );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_SIZE_MISMATCH, asrtc_stream_server_send, server );
        }

        // Allocate record node and data buffer
        struct asrtc_stream_record* rec = asrtl_alloc( &server->alloc, (uint32_t) sizeof( *rec ) );
        if ( !rec ) {
                ASRTL_ERR_LOG( "asrtc_stream", "data: alloc failed for schema %u", schema_id );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_ALLOC_FAILURE, asrtc_stream_server_send, server );
        }

        rec->next = NULL;
        rec->data = asrtl_alloc( &server->alloc, (uint32_t) schema->record_size );
        if ( !rec->data ) {
                asrtl_free( &server->alloc, (void**) &rec );
                ASRTL_ERR_LOG( "asrtc_stream", "data: data alloc failed for schema %u", schema_id );
                return asrtl_msg_ctor_strm_error(
                    ASRTL_STRM_ERR_ALLOC_FAILURE, asrtc_stream_server_send, server );
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

        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_stream_server_recv( void* data, struct asrtl_span buff )
{
        struct asrtc_stream_server* server = (struct asrtc_stream_server*) data;

        if ( asrtl_span_unfit_for( &buff, 1 ) )
                return ASRTL_SUCCESS;
        asrtl_strm_message_id id = (asrtl_strm_message_id) *buff.b++;

        switch ( id ) {
        case ASRTL_STRM_MSG_DEFINE:
                return asrtc_stream_server_handle_define( server, &buff );
        case ASRTL_STRM_MSG_DATA:
                return asrtc_stream_server_handle_data( server, &buff );
        default:
                ASRTL_ERR_LOG( "asrtc_stream", "unknown message id: %u", id );
                return ASRTL_RECV_UNEXPECTED_ERR;
        }
}

static enum asrtl_status asrtc_stream_server_event( void* p, enum asrtl_event_e e, void* arg )
{
        struct asrtc_stream_server* server = (struct asrtc_stream_server*) p;
        switch ( e ) {
        case ASRTL_EVENT_TICK:
                return ASRTL_SUCCESS;
        case ASRTL_EVENT_RECV:
                return asrtc_stream_server_recv( server, *(struct asrtl_span*) arg );
        }
        ASRTL_ERR_LOG( "asrtc_stream_server", "unexpected event: %s", asrtl_event_to_str( e ) );
        return ASRTL_INVALID_EVENT_ERR;
}

enum asrtc_status asrtc_stream_server_init(
    struct asrtc_stream_server* server,
    struct asrtl_node*          prev,
    struct asrtl_sender         sender,
    struct asrtl_allocator      alloc )
{
        if ( !server || !prev )
                return ASRTC_CNTR_INIT_ERR;
        *server = ( struct asrtc_stream_server ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_STRM,
                    .e_cb_ptr = server,
                    .e_cb     = asrtc_stream_server_event,
                    .next     = NULL,
                },
            .sendr = sender,
            .alloc = alloc,
        };
        memset( (void*) server->lookup, 0, sizeof server->lookup );
        prev->next = &server->node;
        return ASRTC_SUCCESS;
}

struct asrtc_stream_schemas asrtc_stream_server_take( struct asrtc_stream_server* server )
{
        struct asrtc_stream_schemas result = { .schemas = NULL, .schema_count = 0 };
        if ( !server )
                return result;

        // Count defined schemas
        uint32_t count = 0;
        for ( uint16_t i = 0; i < ASRTC_STREAM_MAX_SCHEMAS; i++ )
                if ( server->lookup[i] )
                        count++;
        if ( count == 0 )
                return result;

        // Allocate output array
        struct asrtc_stream_schema* arr =
            asrtl_alloc( &server->alloc, count * (uint32_t) sizeof( *arr ) );
        if ( !arr )
                return result;

        // Move schemas into the array, freeing the original allocations
        uint32_t idx = 0;
        for ( uint16_t i = 0; i < ASRTC_STREAM_MAX_SCHEMAS; i++ ) {
                if ( server->lookup[i] ) {
                        arr[idx] = *server->lookup[i];
                        asrtl_free( &server->alloc, (void**) &server->lookup[i] );
                        idx++;
                }
        }

        result.schemas      = arr;
        result.schema_count = count;
        result.alloc        = server->alloc;
        return result;
}

void asrtc_stream_schemas_free( struct asrtc_stream_schemas* schemas )
{
        if ( !schemas || !schemas->schemas )
                return;
        struct asrtl_allocator* alloc = &schemas->alloc;
        for ( uint32_t i = 0; i < schemas->schema_count; i++ )
                asrtc_stream_clear_schema( alloc, &schemas->schemas[i] );
        asrtl_free( alloc, (void**) &schemas->schemas );
        schemas->schema_count = 0;
}

void asrtc_stream_server_clear( struct asrtc_stream_server* server )
{
        if ( !server )
                return;
        for ( uint16_t i = 0; i < ASRTC_STREAM_MAX_SCHEMAS; i++ ) {
                if ( server->lookup[i] ) {
                        asrtc_stream_free_schema( &server->alloc, server->lookup[i] );
                        server->lookup[i] = NULL;
                }
        }
}

void asrtc_stream_server_deinit( struct asrtc_stream_server* server )
{
        asrtc_stream_server_clear( server );
}
