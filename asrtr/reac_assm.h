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
#ifndef ASRT_REAC_ASSM_H
#define ASRT_REAC_ASSM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./collect.h"
#include "./diag.h"
#include "./param.h"
#include "./reactor.h"
#include "./stream.h"

/// Convenience assembly of all reactor-side channel modules wired together
/// in the correct order.  Intended for targets that use every channel; for
/// leaner setups, initialise the individual modules directly.
struct asrt_reac_assm
{
        struct asrt_send_req_list  send_queue;
        struct asrt_reactor        reactor;
        struct asrt_diag_client    diag;
        struct asrt_collect_client collect;
        uint8_t                    param_cache_buf[256];
        struct asrt_param_client   param;
        struct asrt_stream_client  stream;
};

/// Initialise all modules in the assembly.  @p desc is the target description;
/// @p timeout applies to param-channel READY wait.
enum asrt_status asrt_reac_assm_init(
    struct asrt_reac_assm* assembly,
    char const*            desc,
    uint32_t               timeout );

/// Advance all modules by one tick.  Should be called periodically from the main loop.
static inline void asrt_reac_assm_tick( struct asrt_reac_assm* assembly, uint32_t now )
{
        asrt_chann_tick_successors( &assembly->reactor.node, now );
}

/// Release all resources owned by the assembly modules.
void asrt_reac_assm_deinit( struct asrt_reac_assm* assembly );

#ifdef __cplusplus
}
#endif

#endif  // ASRT_REAC_ASSM_H
