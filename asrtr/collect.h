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
#ifndef ASRTR_COLLECT_CLIENT_H
#define ASRTR_COLLECT_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "../asrtl/collect_proto.h"
#include "../asrtl/flat_tree.h"
#include "./status.h"

enum asrtr_collect_client_state
{
        ASRTR_COLLECT_CLIENT_IDLE = 0,
        ASRTR_COLLECT_CLIENT_READY_RECV,
        ASRTR_COLLECT_CLIENT_ACTIVE,
        ASRTR_COLLECT_CLIENT_ERROR,
};

/// Reactor-side collect client (ASRTL_COLL channel).
///
/// Receives READY from the controller, replies with READY_ACK, then
/// provides fire-and-forget append() calls to push tree nodes upstream.
/// Node IDs are auto-assigned from next_node_id (received in READY).
struct asrtr_collect_client
{
        struct asrtl_node   node;
        struct asrtl_sender sendr;

        enum asrtr_collect_client_state state;
        asrtl_flat_id                   root_id;       ///< Root ID from READY message.
        asrtl_flat_id                   next_node_id;  ///< Next auto-assigned node ID.
};

/// Initialise a collect client and link it into the node chain.
///
enum asrtr_status asrtr_collect_client_init(
    struct asrtr_collect_client* client,
    struct asrtl_node*           prev,
    struct asrtl_sender          sender );

/// Deinitialise a collect client and unlink it from the node chain.
///
void asrtr_collect_client_deinit( struct asrtr_collect_client* client );

/// Append a single node to the controller's tree (fire-and-forget).
///
/// The node_id is auto-assigned from the internal counter.  For container
/// types (OBJECT, ARRAY) the assigned ID is written to @p out_id so the
/// caller can use it as parent_id for child appends.
///
enum asrtl_status asrtr_collect_client_append(
    struct asrtr_collect_client*   client,
    asrtl_flat_id                  parent_id,
    char const*                    key,
    struct asrtl_flat_value const* value,
    asrtl_flat_id*                 out_id );

/// Convenience: append an OBJECT container node.
static inline enum asrtl_status asrtr_collect_client_append_object(
    struct asrtr_collect_client* client,
    asrtl_flat_id                parent_id,
    char const*                  key,
    asrtl_flat_id*               out_id )
{
        struct asrtl_flat_value v = { .type = ASRTL_FLAT_CTYPE_OBJECT };
        return asrtr_collect_client_append( client, parent_id, key, &v, out_id );
}

/// Convenience: append an ARRAY container node.
static inline enum asrtl_status asrtr_collect_client_append_array(
    struct asrtr_collect_client* client,
    asrtl_flat_id                parent_id,
    char const*                  key,
    asrtl_flat_id*               out_id )
{
        struct asrtl_flat_value v = { .type = ASRTL_FLAT_CTYPE_ARRAY };
        return asrtr_collect_client_append( client, parent_id, key, &v, out_id );
}

/// Convenience: append a U32 scalar node.
static inline enum asrtl_status asrtr_collect_client_append_u32(
    struct asrtr_collect_client* client,
    asrtl_flat_id                parent_id,
    char const*                  key,
    uint32_t                     val )
{
        struct asrtl_flat_value v = { .type = ASRTL_FLAT_STYPE_U32 };
        v.data.s.u32_val          = val;
        return asrtr_collect_client_append( client, parent_id, key, &v, NULL );
}

/// Convenience: append an I32 scalar node.
static inline enum asrtl_status asrtr_collect_client_append_i32(
    struct asrtr_collect_client* client,
    asrtl_flat_id                parent_id,
    char const*                  key,
    int32_t                      val )
{
        struct asrtl_flat_value v = { .type = ASRTL_FLAT_STYPE_I32 };
        v.data.s.i32_val          = val;
        return asrtr_collect_client_append( client, parent_id, key, &v, NULL );
}

/// Convenience: append a STR scalar node.
static inline enum asrtl_status asrtr_collect_client_append_str(
    struct asrtr_collect_client* client,
    asrtl_flat_id                parent_id,
    char const*                  key,
    char const*                  val )
{
        struct asrtl_flat_value v = { .type = ASRTL_FLAT_STYPE_STR };
        v.data.s.str_val          = val;
        return asrtr_collect_client_append( client, parent_id, key, &v, NULL );
}

/// Convenience: append a BOOL scalar node.
static inline enum asrtl_status asrtr_collect_client_append_bool(
    struct asrtr_collect_client* client,
    asrtl_flat_id                parent_id,
    char const*                  key,
    uint32_t                     val )
{
        struct asrtl_flat_value v = { .type = ASRTL_FLAT_STYPE_BOOL };
        v.data.s.bool_val         = val;
        return asrtr_collect_client_append( client, parent_id, key, &v, NULL );
}

/// Convenience: append a FLOAT scalar node.
static inline enum asrtl_status asrtr_collect_client_append_float(
    struct asrtr_collect_client* client,
    asrtl_flat_id                parent_id,
    char const*                  key,
    float                        val )
{
        struct asrtl_flat_value v = { .type = ASRTL_FLAT_STYPE_FLOAT };
        v.data.s.float_val        = val;
        return asrtr_collect_client_append( client, parent_id, key, &v, NULL );
}

/// Convenience: append a NULL scalar node.
static inline enum asrtl_status asrtr_collect_client_append_null(
    struct asrtr_collect_client* client,
    asrtl_flat_id                parent_id,
    char const*                  key )
{
        struct asrtl_flat_value v = { .type = ASRTL_FLAT_STYPE_NULL };
        return asrtr_collect_client_append( client, parent_id, key, &v, NULL );
}

/// Return the root_id received in the READY message.
static inline asrtl_flat_id asrtr_collect_client_root_id(
    struct asrtr_collect_client const* client )
{
        return client->root_id;
}

#ifdef __cplusplus
}
#endif

#endif  // ASRTR_COLLECT_CLIENT_H
