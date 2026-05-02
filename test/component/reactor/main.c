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
#include <asrtl/log.h>
#include <asrtl/status.h>
#include <asrtr/reactor.h>
#include <assert.h>

ASRT_DEFINE_GPOS_LOG()

static enum asrt_status dummy_test( struct asrt_record* rec )
{
        (void) rec;
        return ASRT_SUCCESS;
}

int main( void )
{
        struct asrt_send_req_list send_queue;
        struct asrt_reactor       reac;
        struct asrt_test          t;

        asrt_send_req_list_init( &send_queue );

        enum asrt_status st = asrt_reactor_init( &reac, &send_queue, "smoke" );
        assert( st == ASRT_SUCCESS );

        asrt_test_init( &t, "dummy", NULL, dummy_test );
        st = asrt_reactor_add_test( &reac, &t );
        assert( st == ASRT_SUCCESS );

        asrt_reactor_deinit( &reac );
        return 0;
}
