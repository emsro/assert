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

typedef void ( *asrt_param_ready_ack_cb )( void* ptr, enum asrt_status status );

/// What the param server is currently waiting for.
enum asrt_param_server_pending
{
        ASRT_PARAM_SERVER_PENDING_NONE = 0,   ///< Nothing outstanding.
        ASRT_PARAM_SERVER_PENDING_READY_ACK,  ///< Waiting for READY_ACK from reactor.
        ASRT_PARAM_SERVER_PENDING_QUERY,      ///< Waiting to encode a QUERY response.
};

/// Param server module — PARAM channel, controller side.
///
/// Receives QUERY / FIND_BY_KEY requests from the reactor and responds from a
/// flat_tree provided via asrt_param_server_set_tree().
struct asrt_param_server
{
        struct asrt_node     node;
        struct asrt_u8d6msg  find_by_key_err_msg;
        struct asrt_u8d6msg  query_err_msg;
        struct asrt_send_req query_msg;
        struct asrt_u8d5msg  ready_msg;

        struct asrt_flat_tree const*   tree;  // non-owning; must outlive each test
        struct asrt_allocator          alloc;
        uint32_t                       max_msg_size;
        int                            ack_received;
        uint8_t*                       enc_buff;  // allocated on READY_ACK tick, freed on deinit
        enum asrt_param_server_pending pending;
        union
        {
                uint32_t     max_msg_size;
                asrt_flat_id node_id;
        } pending_data;
        asrt_param_ready_ack_cb ack_cb;
        void*                   ack_cb_ptr;
        uint32_t                timeout;
        uint32_t                deadline;
};

/// Initialise the param server and link it into the channel chain after @p prev.
enum asrt_status asrt_param_server_init(
    struct asrt_param_server* param,
    struct asrt_node*         prev,
    struct asrt_allocator     alloc );

/// Set the flat_tree that will be served in response to QUERY / FIND_BY_KEY.
/// The tree is borrowed (non-owning); it must outlive each READY..READY_ACK cycle.
void asrt_param_server_set_tree(
    struct asrt_param_server*    param,
    struct asrt_flat_tree const* tree );

/// Send a READY message to the reactor, advertising the tree root node.
/// @p ack_cb is invoked once the reactor acknowledges or the operation times out.
enum asrt_status asrt_param_server_send_ready(
    struct asrt_param_server* param,
    asrt_flat_id              root_id,
    uint32_t                  timeout,
    asrt_param_ready_ack_cb   ack_cb,
    void*                     ack_cb_ptr );

/// Release all resources owned by the param server.
void asrt_param_server_deinit( struct asrt_param_server* param );

#ifdef __cplusplus
}
#endif

#endif  // ASRT_PARAM_SERVER_H
