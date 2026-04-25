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
#include "./assembly.h"

enum asrt_status asrtc_assembly_init(
    struct asrtc_assembly* a,
    struct asrt_sender     sender,
    struct asrt_allocator  alloc )
{
        enum asrt_status st;
        st = asrtc_cntr_init( &a->cntr, sender, alloc );
        if ( st != ASRT_SUCCESS )
                return st;
        st = asrtc_diag_server_init( &a->diag, &a->cntr.node, sender, alloc );
        if ( st != ASRT_SUCCESS )
                goto fail_cntr;
        st = asrtc_collect_server_init( &a->collect, &a->diag.node, sender, alloc, 64, 256 );
        if ( st != ASRT_SUCCESS )
                goto fail_diag;
        st = asrtc_param_server_init( &a->param, &a->collect.node, sender, alloc );
        if ( st != ASRT_SUCCESS )
                goto fail_collect;
        st = asrtc_stream_server_init( &a->stream, &a->param.node, sender, alloc );
        if ( st != ASRT_SUCCESS )
                goto fail_param;
        return ASRT_SUCCESS;

fail_param:
        asrtc_param_server_deinit( &a->param );
fail_collect:
        asrtc_collect_server_deinit( &a->collect );
fail_diag:
        asrtc_diag_server_deinit( &a->diag );
fail_cntr:
        asrtc_cntr_deinit( &a->cntr );
        return st;
}

void asrtc_assembly_deinit( struct asrtc_assembly* a )
{
        if ( !a )
                return;
        asrtc_stream_server_deinit( &a->stream );
        asrtc_param_server_deinit( &a->param );
        asrtc_collect_server_deinit( &a->collect );
        asrtc_diag_server_deinit( &a->diag );
        asrtc_cntr_deinit( &a->cntr );
}

// --- internal callback chain -------------------------------------------------

static enum asrt_status asrtc_on_test_result(
    void*                ptr,
    enum asrt_status     s,
    struct asrtc_result* res )
{
        struct asrtc_assembly* a = (struct asrtc_assembly*) ptr;
        return a->exec_hndl.cb( a->exec_hndl.cb_ptr, s, res );
}

static void asrtc_on_collect_ack( void* ptr, enum asrt_status s )
{
        struct asrtc_assembly* a = (struct asrtc_assembly*) ptr;
        if ( s != ASRT_SUCCESS ) {
                a->exec_hndl.cb( a->exec_hndl.cb_ptr, s, NULL );
                return;
        }
        enum asrt_status es = asrtc_cntr_test_exec(
            &a->cntr, a->exec_hndl.tid, asrtc_on_test_result, a, a->exec_hndl.timeout );
        if ( es != ASRT_SUCCESS )
                a->exec_hndl.cb( a->exec_hndl.cb_ptr, es, NULL );
}

static void asrtc_on_param_ack( void* ptr, enum asrt_status s )
{
        struct asrtc_assembly* a = (struct asrtc_assembly*) ptr;
        if ( s != ASRT_SUCCESS ) {
                a->exec_hndl.cb( a->exec_hndl.cb_ptr, s, NULL );
                return;
        }
        asrtc_stream_server_clear( &a->stream );
        enum asrt_status cs = asrtc_collect_server_send_ready(
            &a->collect, 0, a->exec_hndl.timeout, asrtc_on_collect_ack, a );
        if ( cs != ASRT_SUCCESS )
                a->exec_hndl.cb( a->exec_hndl.cb_ptr, cs, NULL );
}

// -----------------------------------------------------------------------------

enum asrt_status asrtc_assembly_exec_test(
    struct asrtc_assembly*       a,
    struct asrt_flat_tree const* tree,
    asrt_flat_id                 root_id,
    uint16_t                     tid,
    uint32_t                     timeout,
    asrtc_assembly_exec_cb       cb,
    void*                        cb_ptr )
{
        a->exec_hndl.tid     = tid;
        a->exec_hndl.timeout = timeout;
        a->exec_hndl.cb      = cb;
        a->exec_hndl.cb_ptr  = cb_ptr;

        if ( tree != NULL ) {
                asrtc_param_server_set_tree( &a->param, tree );
                return asrtc_param_server_send_ready(
                    &a->param, root_id, timeout, asrtc_on_param_ack, a );
        }

        asrtc_stream_server_clear( &a->stream );
        return asrtc_collect_server_send_ready( &a->collect, 0, timeout, asrtc_on_collect_ack, a );
}
