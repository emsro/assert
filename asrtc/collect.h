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
#ifndef ASRTC_COLLECT_SERVER_H
#define ASRTC_COLLECT_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/allocator.h"
#include "../asrtl/chann.h"
#include "../asrtl/collect_proto.h"
#include "../asrtl/flat_tree.h"
#include "../asrtl/status.h"

typedef void ( *asrt_collect_ready_ack_cb )( void* ptr, enum asrt_status status );

enum asrt_collect_server_state
{
        ASRT_COLLECT_SERVER_IDLE = 0,
        ASRT_COLLECT_SERVER_READY_SENT,
        ASRT_COLLECT_SERVER_READY_ACK_RECV,
        ASRT_COLLECT_SERVER_ACTIVE,
};

struct asrt_collect_ready_data
{
        asrt_flat_id              root_id;
        asrt_collect_ready_ack_cb ack_cb;
        void*                     ack_cb_ptr;
        uint32_t                  timeout;
        uint32_t                  deadline;
};

/// Controller-side collect server (ASRT_COLL channel).
///
/// Owns a flat_tree that the reactor populates remotely via APPEND messages.
/// This is the inverse of the param channel: param reads a tree from the
/// reactor; the collector writes a tree from the reactor into the controller.
///
/// Typical lifecycle:
///   1. Controller calls send_ready(root_id, ...) → READY sent to reactor.
///   2. Reactor replies with READY_ACK → tick fires ack_cb.
///   3. Reactor sends APPEND messages (fire-and-forget, no per-append ACK).
///   4. After test ends, controller reads the tree via tree().
struct asrt_collect_server
{
        struct asrt_node node;

        struct asrt_u8d2msg err_msg;
        struct asrt_u8d9msg ready_msg;

        struct asrt_allocator alloc;
        struct asrt_flat_tree tree;
        uint32_t              tree_block_cap;
        uint32_t              tree_node_cap;

        enum asrt_collect_server_state state;
        asrt_flat_id                   next_node_id;

        struct asrt_collect_ready_data cmd;
};

/// Initialise a collect server and link it into the node chain.
///
enum asrt_status asrt_collect_server_init(
    struct asrt_collect_server* server,
    struct asrt_node*           prev,
    struct asrt_allocator       alloc,
    uint32_t                    tree_block_cap,
    uint32_t                    tree_node_cap );

/// Send a READY message to the reactor, starting a new collection session.
///
/// The READY payload carries @p root_id (the tree root the reactor should
/// populate) and the server's internal next_node_id counter.  The server
/// transitions to READY_SENT and waits for READY_ACK or timeout.
///
/// May be called from IDLE or ACTIVE state (to start a new session).
///
enum asrt_status asrt_collect_server_send_ready(
    struct asrt_collect_server* server,
    asrt_flat_id                root_id,
    uint32_t                    timeout,
    asrt_collect_ready_ack_cb   ack_cb,
    void*                       ack_cb_ptr );

/// Access the built flat_tree.
struct asrt_flat_tree const* asrt_collect_server_tree( struct asrt_collect_server const* server );

/// Free internal resources (flat_tree storage, any buffered append data).
void asrt_collect_server_deinit( struct asrt_collect_server* server );

#ifdef __cplusplus
}
#endif

#endif  // ASRT_COLLECT_SERVER_H
