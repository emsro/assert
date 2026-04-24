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
#ifndef ASRTR_ASSEMBLY_H
#define ASRTR_ASSEMBLY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./collect.h"
#include "./diag.h"
#include "./param.h"
#include "./reactor.h"
#include "./stream.h"

struct asrtr_assembly
{
        struct asrtr_reactor        reactor;
        struct asrtr_diag           diag;
        struct asrtr_collect_client collect;
        uint8_t                     param_cache_buf[256];
        struct asrtr_param_client   param;
        struct asrtr_stream_client  stream;
};

inline enum asrtl_status asrtr_assembly_init(
    struct asrtr_assembly* assembly,
    struct asrtl_sender    sender,
    char const*            desc,
    uint32_t               timeout )
{
        if ( asrtr_reactor_init( &assembly->reactor, sender, desc ) != ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_assembly", "reactor init failed" );
                return ASRTL_INIT_ERR;
        }
        if ( asrtr_diag_init( &assembly->diag, &assembly->reactor.node, sender ) !=
             ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_assembly", "diag init failed" );
                return ASRTL_INIT_ERR;
        }
        if ( asrtr_collect_client_init( &assembly->collect, &assembly->diag.node, sender ) !=
             ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_assembly", "collect client init failed" );
                return ASRTL_INIT_ERR;
        }
        if ( asrtr_param_client_init(
                 &assembly->param,
                 &assembly->collect.node,
                 sender,
                 {
                     .b = assembly->param_cache_buf,
                     .e = assembly->param_cache_buf + sizeof( assembly->param_cache_buf ),
                 },
                 timeout ) != ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_assembly", "param client init failed" );
                return ASRTL_INIT_ERR;
        }
        if ( asrtr_stream_client_init( &assembly->stream, &assembly->param.node, sender ) !=
             ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_assembly", "stream client init failed" );
                return ASRTL_INIT_ERR;
        }
        return ASRTL_SUCCESS;
}

static inline void asrtr_assembly_tick( struct asrtr_assembly* assembly, uint32_t now )
{
        asrtl_chann_tick_successors( &assembly->reactor.node, now );
}

static inline void asrtr_assembly_deinit( struct asrtr_assembly* assembly )
{
        asrtr_stream_client_deinit( &assembly->stream );
        asrtr_param_client_deinit( &assembly->param );
        asrtr_collect_client_deinit( &assembly->collect );
        asrtr_diag_deinit( &assembly->diag );
        asrtr_reactor_deinit( &assembly->reactor );
}

#ifdef __cplusplus
}
#endif

#endif  // ASRTR_ASSEMBLY_H
