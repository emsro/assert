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
#ifndef ASRTC_PARAM_SERVER_H
#define ASRTC_PARAM_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/allocator.h"
#include "../asrtl/chann.h"
#include "../asrtl/flat_tree.h"
#include "../asrtl/param_proto.h"
#include "./status.h"

typedef void ( *asrtc_param_ready_ack_cb )( void* ptr, enum asrtc_status status );

enum asrtc_param_server_pending
{
        ASRTC_PARAM_SERVER_PENDING_NONE = 0,
        ASRTC_PARAM_SERVER_PENDING_READY_ACK,
        ASRTC_PARAM_SERVER_PENDING_QUERY,
};

struct asrtc_param_server
{
        struct asrtl_node               node;
        struct asrtl_sender             sendr;
        struct asrtl_flat_tree const*   tree;  // non-owning; must outlive each test
        struct asrtl_allocator          alloc;
        uint32_t                        max_msg_size;
        int                             ack_received;
        uint8_t*                        enc_buff;  // allocated on READY_ACK tick, freed on deinit
        enum asrtc_param_server_pending pending;
        union
        {
                uint32_t      max_msg_size;
                asrtl_flat_id node_id;
        } pending_data;
        asrtc_param_ready_ack_cb ack_cb;
        void*                    ack_cb_ptr;
        uint32_t                 timeout;
        uint32_t                 deadline;
};

enum asrtc_status asrtc_param_server_init(
    struct asrtc_param_server* param,
    struct asrtl_node*         prev,
    struct asrtl_sender        sender,
    struct asrtl_allocator     alloc );

void asrtc_param_server_set_tree(
    struct asrtc_param_server*    param,
    struct asrtl_flat_tree const* tree );

enum asrtl_status asrtc_param_server_send_ready(
    struct asrtc_param_server* param,
    asrtl_flat_id              root_id,
    uint32_t                   timeout,
    asrtc_param_ready_ack_cb   ack_cb,
    void*                      ack_cb_ptr );


void asrtc_param_server_deinit( struct asrtc_param_server* param );

#ifdef __cplusplus
}
#endif

#endif  // ASRTC_PARAM_SERVER_H
