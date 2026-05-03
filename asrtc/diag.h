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

/// A single diagnostic record received from the reactor.
/// Memory for this struct and its string fields is allocated from the
/// asrt_allocator provided to asrt_diag_server_init().
struct asrt_diag_record
{
        char const* file;   ///< Source filename (owned copy).
        char const* extra;  ///< Optional expression string (owned copy), may be NULL.
        uint32_t    line;   ///< Source line number.

        struct asrt_diag_record* next;  ///< Intrusive linked-list link.
};

/// Free a single diag record (both string copies and the record itself).
/// @p alloc must be the same allocator that was provided to asrt_diag_server_init().
void asrt_diag_free_record( struct asrt_allocator* alloc, struct asrt_diag_record* rec );

/// Diagnostic server module — DIAG channel, controller side.
/// Accumulates RECORD messages from the reactor in a linked list.
struct asrt_diag_server
{
        struct asrt_node      node;
        struct asrt_allocator alloc;

        struct asrt_diag_record* first_rec;
        struct asrt_diag_record* last_rec;
};

/// Initialise the diag server and link it into the channel chain after @p prev.
enum asrt_status asrt_diag_server_init(
    struct asrt_diag_server* diag,
    struct asrt_node*        prev,
    struct asrt_allocator    alloc );

/// Remove and return the oldest record, or NULL if the list is empty.
/// The caller takes ownership and must free it with asrt_diag_free_record().
struct asrt_diag_record* asrt_diag_server_take_record( struct asrt_diag_server* diag );

/// Free all buffered records and the diag server resources.
void asrt_diag_server_deinit( struct asrt_diag_server* diag );

#ifdef __cplusplus
}
#endif

#endif  // ASRTC_DIAG_H
