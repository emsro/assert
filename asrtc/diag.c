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
#include "./diag.h"

#include "../asrtl/log.h"

void asrt_diag_free_record( struct asrt_allocator* alloc, struct asrt_diag_record* rec )
{
        if ( rec->extra )
                asrt_free( alloc, (void**) &rec->extra );
        if ( rec->file )
                asrt_free( alloc, (void**) &rec->file );
        asrt_free( alloc, (void**) &rec );
}

static enum asrt_status asrt_diag_recv_record( struct asrt_diag_server* d, struct asrt_span* buff )
{
        if ( asrt_span_unfit_for( buff, sizeof( uint32_t ) + sizeof( uint8_t ) ) )
                return ASRT_RECV_ERR;

        uint32_t line;
        asrt_cut_u32( &buff->b, &line );

        uint8_t file_len = *buff->b++;

        if ( asrt_span_unfit_for( buff, file_len ) )
                return ASRT_RECV_ERR;

        struct asrt_diag_record* rec = asrt_alloc( &d->alloc, sizeof( *rec ) );
        if ( !rec )
                return ASRT_ALLOC_ERR;
        *rec = ( struct asrt_diag_record ){
            .file  = NULL,
            .extra = NULL,
            .line  = line,
            .next  = NULL,
        };

        char* file = asrt_alloc( &d->alloc, (uint32_t) file_len + 1 );
        if ( !file ) {
                asrt_free( &d->alloc, (void**) &rec );
                return ASRT_ALLOC_ERR;
        }
        memcpy( file, buff->b, file_len );
        file[file_len] = '\0';
        buff->b += file_len;
        rec->file = file;

        uint32_t extra_len = (uint32_t) ( buff->e - buff->b );
        if ( extra_len > 0 ) {
                char* extra = asrt_alloc( &d->alloc, extra_len + 1 );
                if ( !extra ) {
                        asrt_free( &d->alloc, (void**) &rec->file );
                        asrt_free( &d->alloc, (void**) &rec );
                        return ASRT_ALLOC_ERR;
                }
                memcpy( extra, buff->b, extra_len );
                extra[extra_len] = '\0';
                buff->b += extra_len;
                rec->extra = extra;
        }

        if ( !d->first_rec ) {
                d->first_rec = rec;
                d->last_rec  = rec;
        } else {
                d->last_rec->next = rec;
                d->last_rec       = rec;
        }

        return ASRT_SUCCESS;
}

static enum asrt_status asrt_diag_recv( void* data, struct asrt_span buff )
{
        struct asrt_diag_server* diag = (struct asrt_diag_server*) data;

        asrt_diag_message_id id;
        if ( asrt_span_unfit_for( &buff, sizeof( id ) ) )
                return ASRT_SUCCESS;
        id = (asrt_diag_message_id) *buff.b++;

        enum asrt_status st = ASRT_SUCCESS;
        switch ( id ) {
        case ASRT_DIAG_MSG_RECORD:
                st = asrt_diag_recv_record( diag, &buff );
                break;
        default:
                ASRT_ERR_LOG( "asrt_diag", "Received unknown diag message id: %u", id );
                return ASRT_RECV_ERR;
        }

        return st;
}

static enum asrt_status asrt_diag_event( void* p, enum asrt_event_e e, void* arg )
{
        struct asrt_diag_server* diag = (struct asrt_diag_server*) p;
        switch ( e ) {
        case ASRT_EVENT_RECV:
                return asrt_diag_recv( diag, *(struct asrt_span*) arg );
        case ASRT_EVENT_TICK:
                return ASRT_SUCCESS;
        }
        ASRT_ERR_LOG( "asrt_diag", "unexpected event: %s", asrt_event_to_str( e ) );
        return ASRT_ARG_ERR;
}

enum asrt_status asrt_diag_server_init(
    struct asrt_diag_server* diag,
    struct asrt_node*        prev,
    struct asrt_allocator    alloc )
{
        if ( !diag || !prev )
                return ASRT_INIT_ERR;
        *diag = ( struct asrt_diag_server ){
            .node =
                ( struct asrt_node ){
                    .chid       = ASRT_DIAG,
                    .e_cb_ptr   = diag,
                    .e_cb       = asrt_diag_event,
                    .next       = NULL,
                    .prev       = NULL,
                    .send_queue = prev->send_queue,
                },
            .alloc     = alloc,
            .first_rec = NULL,
            .last_rec  = NULL,
        };
        asrt_node_link( prev, &diag->node );
        return ASRT_SUCCESS;
}

struct asrt_diag_record* asrt_diag_server_take_record( struct asrt_diag_server* diag )
{
        if ( !diag || !diag->first_rec )
                return NULL;
        struct asrt_diag_record* rec = diag->first_rec;
        diag->first_rec              = rec->next;
        if ( !diag->first_rec )
                diag->last_rec = NULL;
        return rec;
}

void asrt_diag_server_deinit( struct asrt_diag_server* diag )
{
        if ( !diag )
                return;

        asrt_node_unlink( &diag->node );
        struct asrt_diag_record* rec = diag->first_rec;
        while ( rec ) {
                struct asrt_diag_record* next = rec->next;
                asrt_diag_free_record( &diag->alloc, rec );
                rec = next;
        }
        diag->first_rec = NULL;
        diag->last_rec  = NULL;
}
