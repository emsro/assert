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

void asrtc_diag_free_record( struct asrtl_allocator* alloc, struct asrtc_diag_record* rec )
{
        if ( rec->extra )
                asrtl_free( alloc, (void**) &rec->extra );
        if ( rec->file )
                asrtl_free( alloc, (void**) &rec->file );
        asrtl_free( alloc, (void**) &rec );
}

static enum asrtl_status asrtc_diag_recv_record( struct asrtc_diag* d, struct asrtl_span* buff )
{
        if ( asrtl_span_unfit_for( buff, sizeof( uint32_t ) + sizeof( uint8_t ) ) )
                return ASRTL_RECV_ERR;

        uint32_t line;
        asrtl_cut_u32( &buff->b, &line );

        uint8_t file_len = *buff->b++;

        if ( asrtl_span_unfit_for( buff, file_len ) )
                return ASRTL_RECV_ERR;

        struct asrtc_diag_record* rec = asrtl_alloc( &d->alloc, sizeof( *rec ) );
        if ( !rec )
                return ASRTL_ALLOC_ERR;
        *rec = ( struct asrtc_diag_record ){
            .file  = NULL,
            .extra = NULL,
            .line  = line,
            .next  = NULL,
        };

        char* file = asrtl_alloc( &d->alloc, (uint32_t) file_len + 1 );
        if ( !file ) {
                asrtl_free( &d->alloc, (void**) &rec );
                return ASRTL_ALLOC_ERR;
        }
        memcpy( file, buff->b, file_len );
        file[file_len] = '\0';
        buff->b += file_len;
        rec->file = file;

        uint32_t extra_len = (uint32_t) ( buff->e - buff->b );
        if ( extra_len > 0 ) {
                char* extra = asrtl_alloc( &d->alloc, extra_len + 1 );
                if ( !extra ) {
                        asrtl_free( &d->alloc, (void**) &rec->file );
                        asrtl_free( &d->alloc, (void**) &rec );
                        return ASRTL_ALLOC_ERR;
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

        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_diag_recv( void* data, struct asrtl_span buff )
{
        struct asrtc_diag* diag = (struct asrtc_diag*) data;

        asrtl_diag_message_id id;
        if ( asrtl_span_unfit_for( &buff, sizeof( id ) ) )
                return ASRTL_SUCCESS;
        id = (asrtl_diag_message_id) *buff.b++;

        enum asrtl_status st = ASRTL_SUCCESS;
        switch ( id ) {
        case ASRTL_DIAG_MSG_RECORD:
                st = asrtc_diag_recv_record( diag, &buff );
                break;
        default:
                ASRTL_ERR_LOG( "asrtc_diag", "Received unknown diag message id: %u", id );
                return ASRTL_RECV_UNEXPECTED_ERR;
        }

        return st;
}

static enum asrtl_status asrtc_diag_event( void* p, enum asrtl_event_e e, void* arg )
{
        struct asrtc_diag* diag = (struct asrtc_diag*) p;
        switch ( e ) {
        case ASRTL_EVENT_RECV:
                return asrtc_diag_recv( diag, *(struct asrtl_span*) arg );
        case ASRTL_EVENT_TICK:
                return ASRTL_SUCCESS;
        }
        ASRTL_ERR_LOG( "asrtc_diag", "unexpected event: %s", asrtl_event_to_str( e ) );
        return ASRTL_INVALID_EVENT_ERR;
}

enum asrtl_status asrtc_diag_init(
    struct asrtc_diag*     diag,
    struct asrtl_node*     prev,
    struct asrtl_sender    sender,
    struct asrtl_allocator alloc )
{
        if ( !diag || !prev )
                return ASRTL_INIT_ERR;
        *diag = ( struct asrtc_diag ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_DIAG,
                    .e_cb_ptr = diag,
                    .e_cb     = asrtc_diag_event,
                    .next     = NULL,
                },
            .sendr     = sender,
            .alloc     = alloc,
            .first_rec = NULL,
            .last_rec  = NULL,
        };
        asrtl_node_link( prev, &diag->node );
        return ASRTL_SUCCESS;
}

struct asrtc_diag_record* asrtc_diag_take_record( struct asrtc_diag* diag )
{
        if ( !diag || !diag->first_rec )
                return NULL;
        struct asrtc_diag_record* rec = diag->first_rec;
        diag->first_rec               = rec->next;
        if ( !diag->first_rec )
                diag->last_rec = NULL;
        return rec;
}

void asrtc_diag_deinit( struct asrtc_diag* diag )
{
        if ( !diag )
                return;

        asrtl_node_unlink( &diag->node );
        struct asrtc_diag_record* rec = diag->first_rec;
        while ( rec ) {
                struct asrtc_diag_record* next = rec->next;
                asrtc_diag_free_record( &diag->alloc, rec );
                rec = next;
        }
        diag->first_rec = NULL;
        diag->last_rec  = NULL;
}
