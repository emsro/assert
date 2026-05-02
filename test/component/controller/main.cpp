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
#include <asrtc/cntr_assm.h>
#include <asrtl/log.h>
#include <asrtl/status.h>
#include <cassert>

ASRT_DEFINE_GPOS_LOG()

int main()
{
        struct asrt_cntr_assm asm_ = {};
        enum asrt_status      st   = asrt_cntr_assm_init( &asm_, asrt_default_allocator() );
        assert( st == ASRT_SUCCESS );
        asrt_cntr_assm_deinit( &asm_ );
        return 0;
}
