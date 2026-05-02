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

#ifndef ASRTC_DIAG_H
#define ASRTC_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/allocator.h"
#include "../asrtl/chann.h"
#include "../asrtl/diag_proto.h"

struct asrt_diag_record
{
        char const* file;
        char const* extra;
        uint32_t    line;

        struct asrt_diag_record* next;
};

void asrt_diag_free_record( struct asrt_allocator* alloc, struct asrt_diag_record* rec );

struct asrt_diag_server
{
        struct asrt_node      node;
        struct asrt_allocator alloc;

        struct asrt_diag_record* first_rec;
        struct asrt_diag_record* last_rec;
};

enum asrt_status asrt_diag_server_init(
    struct asrt_diag_server* diag,
    struct asrt_node*        prev,
    struct asrt_allocator    alloc );

struct asrt_diag_record* asrt_diag_server_take_record( struct asrt_diag_server* diag );

void asrt_diag_server_deinit( struct asrt_diag_server* diag );

#ifdef __cplusplus
}
#endif

#endif  // ASRTC_DIAG_H
