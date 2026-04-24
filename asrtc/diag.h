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

struct asrtc_diag_record
{
        char const* file;
        char const* extra;
        uint32_t    line;

        struct asrtc_diag_record* next;
};

void asrtc_diag_free_record( struct asrtl_allocator* alloc, struct asrtc_diag_record* rec );

struct asrtc_diag
{
        struct asrtl_node      node;
        struct asrtl_sender    sendr;
        struct asrtl_allocator alloc;

        struct asrtc_diag_record* first_rec;
        struct asrtc_diag_record* last_rec;
};

enum asrtl_status asrtc_diag_init(
    struct asrtc_diag*     diag,
    struct asrtl_node*     prev,
    struct asrtl_sender    sender,
    struct asrtl_allocator alloc );

struct asrtc_diag_record* asrtc_diag_take_record( struct asrtc_diag* diag );

void asrtc_diag_deinit( struct asrtc_diag* diag );

#ifdef __cplusplus
}
#endif

#endif  // ASRTC_DIAG_H
