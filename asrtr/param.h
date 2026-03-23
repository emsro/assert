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
#ifndef ASRTR_PARAM_H
#define ASRTR_PARAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "../asrtl/flat_tree.h"
#include "../asrtl/param_proto.h"
#include "./status.h"

typedef void ( *asrtr_param_error_cb )( void* cb_ptr, uint8_t error_code, asrtl_flat_id node_id );

enum asrtr_param_client_pending
{
        ASRTR_PARAM_CLIENT_PENDING_NONE = 0,
        ASRTR_PARAM_CLIENT_PENDING_READY,
        ASRTR_PARAM_CLIENT_PENDING_DELIVER,
        ASRTR_PARAM_CLIENT_PENDING_ERROR,
};

struct asrtr_param_client
{
        struct asrtl_node   node;
        struct asrtl_sender sendr;
        asrtl_flat_id       root_id;
        int                 ready;

        uint8_t*      cache_buf;
        uint32_t      cache_capacity;
        uint32_t      cache_len;           // valid bytes (includes trailing 4-byte next_sib)
        asrtl_flat_id cache_next_sibling;  // trailing next_sibling_id from last RESPONSE

        asrtr_param_response_cb pending_cb;
        void*                   pending_cb_ptr;
        asrtr_param_error_cb    pending_error_cb;
        void*                   pending_error_cb_ptr;

        enum asrtr_param_client_pending pending;
        asrtl_flat_id                   query_node_id;

        union
        {
                asrtl_flat_id root_id;
                struct
                {
                        uint8_t       error_code;
                        asrtl_flat_id node_id;
                } error;
        } pending_data;
};

enum asrtr_status asrtr_param_client_init(
    struct asrtr_param_client* client,
    struct asrtl_node*         prev,
    struct asrtl_sender        sender,
    struct asrtl_span          msg_buffer );

asrtl_flat_id asrtr_param_client_root_id( struct asrtr_param_client const* client );

enum asrtl_status asrtr_param_client_query(
    struct asrtr_param_client* client,
    asrtl_flat_id              node_id,
    asrtr_param_response_cb    response_cb,
    void*                      response_cb_ptr,
    asrtr_param_error_cb       error_cb,
    void*                      error_cb_ptr );

enum asrtl_status asrtr_param_client_tick( struct asrtr_param_client* client );

void asrtr_param_client_deinit( struct asrtr_param_client* client );

#ifdef __cplusplus
}
#endif

#endif  // ASRTR_PARAM_H
