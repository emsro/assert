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

inline enum asrt_status asrt_reac_assm_init(
    struct asrt_reac_assm* assembly,
    char const*            desc,
    uint32_t               timeout )
{
        asrt_send_req_list_init( &assembly->send_queue );
        if ( asrt_reactor_init( &assembly->reactor, &assembly->send_queue, desc ) !=
             ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrt_reac_assm", "reactor init failed" );
                return ASRT_INIT_ERR;
        }
        if ( asrt_diag_client_init( &assembly->diag, &assembly->reactor.node ) != ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrt_reac_assm", "diag init failed" );
                return ASRT_INIT_ERR;
        }
        if ( asrt_collect_client_init( &assembly->collect, &assembly->diag.node ) !=
             ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrt_reac_assm", "collect client init failed" );
                return ASRT_INIT_ERR;
        }
        if ( asrt_param_client_init(
                 &assembly->param,
                 &assembly->collect.node,
                 ( struct asrt_span ){
                     .b = assembly->param_cache_buf,
                     .e = assembly->param_cache_buf + sizeof( assembly->param_cache_buf ),
                 },
                 timeout ) != ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrt_reac_assm", "param client init failed" );
                return ASRT_INIT_ERR;
        }
        if ( asrt_stream_client_init( &assembly->stream, &assembly->param.node ) != ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrt_reac_assm", "stream client init failed" );
                return ASRT_INIT_ERR;
        }
        return ASRT_SUCCESS;
}

static inline void asrt_reac_assm_tick( struct asrt_reac_assm* assembly, uint32_t now )
{
        asrt_chann_tick_successors( &assembly->reactor.node, now );
}

static inline void asrt_reac_assm_deinit( struct asrt_reac_assm* assembly )
{
        asrt_stream_client_deinit( &assembly->stream );
        asrt_param_client_deinit( &assembly->param );
        asrt_collect_client_deinit( &assembly->collect );
        asrt_diag_client_deinit( &assembly->diag );
        asrt_reactor_deinit( &assembly->reactor );
}

#ifdef __cplusplus
}
#endif

#endif  // ASRT_REAC_ASSM_H
