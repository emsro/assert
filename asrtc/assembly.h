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

typedef enum asrt_status (
    *asrt_assembly_exec_cb )( void* ptr, enum asrt_status s, struct asrt_result* res );

struct asrt_assembly_exec_handler
{
        uint16_t              tid;
        uint32_t              timeout;
        asrt_assembly_exec_cb cb;
        void*                 cb_ptr;
};

struct asrt_cntr_assm
{
        struct asrt_controller     cntr;
        struct asrt_diag_server    diag;
        struct asrt_collect_server collect;
        struct asrt_param_server   param;
        struct asrt_stream_server  stream;
        //
        struct asrt_assembly_exec_handler exec_hndl;
};

enum asrt_status asrt_cntr_assm_init(
    struct asrt_cntr_assm* assembly,
    struct asrt_sender     sender,
    struct asrt_allocator  alloc );

void asrt_cntr_assm_deinit( struct asrt_cntr_assm* assembly );

static inline void asrt_cntr_assm_tick( struct asrt_cntr_assm* assembly, uint32_t now )
{
        asrt_chann_tick_successors( &assembly->cntr.node, now );
}

/// Execute a test: optionally upload param tree, clear+ready collect, then
/// exec the test.  \p tree may be NULL to skip the param upload step.
/// \p cb is called exactly once with the test result (or first error status).
enum asrt_status asrt_cntr_assm_exec_test(
    struct asrt_cntr_assm*       assembly,
    struct asrt_flat_tree const* tree,
    asrt_flat_id                 root_id,
    uint16_t                     tid,
    uint32_t                     timeout,
    asrt_assembly_exec_cb        cb,
    void*                        cb_ptr );

#ifdef __cplusplus
}
#endif

#endif  // ASRTC_ASSEMBLY_H
