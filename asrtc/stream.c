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


/// Recursively free the @p elem chain of a field descriptor.
/// Does NOT free @p desc itself — the caller owns it (it lives in the
/// flat @c schema->fields array).
static void free_field_desc_children(
    struct asrt_allocator*         alloc,
    struct asrt_stream_field_desc* desc )
{
        if ( desc->elem ) {
                free_field_desc_children( alloc, desc->elem );
                asrt_free( alloc, (void**) &desc->elem );
        }
}

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

        // Free elem chains in every field descriptor, then the flat fields array
        if ( schema->fields ) {
                for ( uint8_t i = 0; i < schema->field_count; i++ )
                        free_field_desc_children( alloc, &schema->fields[i] );
                asrt_free( alloc, (void**) &schema->fields );
        }
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

/// Compute the wire size (bytes) of one instance of a field.
/// For scalars this is just the tag size; for arrays it is count × elem_size.
static uint16_t field_desc_size( struct asrt_stream_field_desc const* desc )
{
        if ( desc->type == ASRT_STRM_FIELD_ARRAY )
                return (uint16_t) desc->count * field_desc_size( desc->elem );
        return asrt_strm_field_size( desc->type );
}

/// Parse one recursive field definition from @p bytes (up to @p remaining
/// bytes available), filling @p out.  Returns the number of bytes consumed
/// on success, or 0 on any error (truncation, invalid tag, zero count, or
/// alloc failure).  On failure all intermediate allocations are freed and
/// @p out is left unmodified.
static uint16_t parse_field_def(
    struct asrt_allocator*         alloc,
    uint8_t const*                 bytes,
    uint16_t                       remaining,
    struct asrt_stream_field_desc* out )
{
        if ( remaining < 1 )
                return 0;

        uint8_t tag = bytes[0];
        if ( !asrt_strm_field_valid( tag ) )
                return 0;

        if ( tag == ASRT_STRM_FIELD_ARRAY ) {
                // Need: tag(1) + count_hi(1) + count_lo(1) + at least one element byte
                if ( remaining < 4 )
                        return 0;

                uint16_t count = ( (uint16_t) bytes[1] << 8 ) | bytes[2];
                if ( count == 0 )
                        return 0;

                struct asrt_stream_field_desc* elem = asrt_alloc( alloc, sizeof( *elem ) );
                if ( !elem )
                        return 0;

                *elem             = ( struct asrt_stream_field_desc ){ 0 };
                uint16_t consumed = parse_field_def( alloc, bytes + 3, remaining - 3, elem );
                if ( consumed == 0 ) {
                        asrt_free( alloc, (void**) &elem );
                        return 0;
                }

                out->type  = ASRT_STRM_FIELD_ARRAY;
                out->count = count;
                out->elem  = elem;
                return (uint16_t) ( 3 + consumed );
        }

        // Scalar field
        out->type  = (asrt_strm_field_type) tag;
        out->count = 0;
        out->elem  = NULL;
        return 1;
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

        // Parse each field definition, supporting recursive ARRAY types.
        uint16_t record_size = 0;
        for ( uint8_t i = 0; i < field_count; i++ ) {
                uint16_t remaining = (uint16_t) ( buff->e - buff->b );
                uint16_t consumed =
                    parse_field_def( &server->alloc, buff->b, remaining, &schema->fields[i] );

                if ( consumed == 0 ) {
                        ASRT_ERR_LOG(
                            "asrt_stream",
                            "define: invalid field definition at logical field %u",
                            i );
                        // Free any elem chains already allocated in fields[0..i]
                        for ( uint8_t j = 0; j < field_count; j++ )
                                free_field_desc_children( &server->alloc, &schema->fields[j] );
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

                buff->b += consumed;
                record_size = (uint16_t) ( record_size + field_desc_size( &schema->fields[i] ) );
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
                return ASRT_RECV_ERR;
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
        return ASRT_ARG_ERR;
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
