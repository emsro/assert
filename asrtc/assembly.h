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
#ifndef ASRTC_ASSEMBLY_H
#define ASRTC_ASSEMBLY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtc/collect.h"
#include "../asrtc/controller.h"
#include "../asrtc/diag.h"
#include "../asrtc/param.h"
#include "../asrtc/stream.h"

typedef enum asrtl_status (
    *asrtc_assembly_exec_cb )( void* ptr, enum asrtl_status s, struct asrtc_result* res );

struct asrtc_assembly_exec_handler
{
        uint16_t               tid;
        uint32_t               timeout;
        asrtc_assembly_exec_cb cb;
        void*                  cb_ptr;
};

struct asrtc_assembly
{
        struct asrtc_controller     cntr;
        struct asrtc_diag           diag;
        struct asrtc_collect_server collect;
        struct asrtc_param_server   param;
        struct asrtc_stream_server  stream;
        //
        struct asrtc_assembly_exec_handler exec_hndl;
};

enum asrtl_status asrtc_assembly_init(
    struct asrtc_assembly* assembly,
    struct asrtl_sender    sender,
    struct asrtl_allocator alloc );

void asrtc_assembly_deinit( struct asrtc_assembly* assembly );

static inline void asrtc_assembly_tick( struct asrtc_assembly* assembly, uint32_t now )
{
        asrtl_chann_tick_successors( &assembly->cntr.node, now );
}

/// Execute a test: optionally upload param tree, clear+ready collect, then
/// exec the test.  \p tree may be NULL to skip the param upload step.
/// \p cb is called exactly once with the test result (or first error status).
enum asrtl_status asrtc_assembly_exec_test(
    struct asrtc_assembly*        assembly,
    struct asrtl_flat_tree const* tree,
    asrtl_flat_id                 root_id,
    uint16_t                      tid,
    uint32_t                      timeout,
    asrtc_assembly_exec_cb        cb,
    void*                         cb_ptr );

#ifdef __cplusplus
}
#endif

#endif  // ASRTC_ASSEMBLY_H
