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
#ifndef ASRT_PARAM_H
#define ASRT_PARAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "../asrtl/flat_tree.h"
#include "../asrtl/param_proto.h"

enum asrt_param_client_state
{
        ASRT_PARAM_CLIENT_IDLE = 0,
        ASRT_PARAM_CLIENT_READY_RECV,
        ASRT_PARAM_CLIENT_READY_SENT,
        ASRT_PARAM_CLIENT_DELIVER,
        ASRT_PARAM_CLIENT_QUERY_ERROR,
};

struct asrt_param_client
{
        struct asrt_node node;
        asrt_flat_id     root_id;
        int              ready;

        uint8_t*     cache_buf;
        uint32_t     cache_capacity;
        uint32_t     cache_len;           // valid bytes (includes trailing 4-byte next_sib)
        asrt_flat_id cache_next_sibling;  // trailing next_sibling_id from last RESPONSE

        struct asrt_param_query* pending_query;

        uint32_t timeout;

        enum asrt_param_client_state state;

        union
        {
                asrt_flat_id root_id;
                struct
                {
                        uint8_t      error_code;
                        asrt_flat_id node_id;
                } error;
        } state_data;

        struct asrt_u8d5msg               ready_ack_msg;
        struct asrt_u8d5msg               query_msg;
        struct asrt_param_find_by_key_msg find_by_key_msg;
};

enum asrt_status asrt_param_client_init(
    struct asrt_param_client* client,
    struct asrt_node*         prev,
    struct asrt_span          msg_buffer,
    uint32_t                  timeout );

asrt_flat_id asrt_param_client_root_id( struct asrt_param_client const* client );


typedef void (
    *asrt_param_u32_cb )( struct asrt_param_client*, struct asrt_param_query*, uint32_t val );
typedef void (
    *asrt_param_i32_cb )( struct asrt_param_client*, struct asrt_param_query*, int32_t val );
typedef void (
    *asrt_param_str_cb )( struct asrt_param_client*, struct asrt_param_query*, char const* val );
typedef void (
    *asrt_param_float_cb )( struct asrt_param_client*, struct asrt_param_query*, float val );
typedef void (
    *asrt_param_bool_cb )( struct asrt_param_client*, struct asrt_param_query*, uint32_t val );
typedef void ( *asrt_param_obj_cb )(
    struct asrt_param_client*,
    struct asrt_param_query*,
    struct asrt_flat_child_list val );
typedef void ( *asrt_param_arr_cb )(
    struct asrt_param_client*,
    struct asrt_param_query*,
    struct asrt_flat_child_list val );
typedef void ( *asrt_param_any_cb )(
    struct asrt_param_client*,
    struct asrt_param_query*,
    struct asrt_flat_value val );

union asrt_param_cb
{
        asrt_param_any_cb   any;
        asrt_param_u32_cb   u32;
        asrt_param_i32_cb   i32;
        asrt_param_str_cb   str;
        asrt_param_float_cb flt;
        asrt_param_bool_cb  bln;
        asrt_param_obj_cb   obj;
        asrt_param_arr_cb   arr;
};

struct asrt_param_query
{
        enum asrt_param_err_e error_code;
        asrt_flat_id          node_id;
        char const*           key;
        asrt_flat_id          next_sibling;

        asrt_flat_value_type expected_type;
        union asrt_param_cb  cb;
        void*                cb_ptr;

        uint32_t start;
};

// Submit a query — expects cb, cb_ptr, and expected_type to be pre-set.
// If key is NULL, search by node_id; otherwise search by key.
enum asrt_status asrt_param_client_query(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    char const*               key );

static inline int asrt_param_query_pending( struct asrt_param_client const* client )
{
        return client->pending_query != NULL;
}

// Typed query helpers — set expected_type + cb + cb_ptr, then submit
static inline enum asrt_status asrt_param_client_fetch_any(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    asrt_param_any_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_NONE;
        query->cb.any        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrt_param_client_fetch_u32(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    asrt_param_u32_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_U32;
        query->cb.u32        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrt_param_client_fetch_i32(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    asrt_param_i32_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_I32;
        query->cb.i32        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrt_param_client_fetch_str(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    asrt_param_str_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_STR;
        query->cb.str        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrt_param_client_fetch_float(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    asrt_param_float_cb       cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_FLOAT;
        query->cb.flt        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrt_param_client_fetch_bool(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    asrt_param_bool_cb        cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_BOOL;
        query->cb.bln        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrt_param_client_fetch_obj(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    asrt_param_obj_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_CTYPE_OBJECT;
        query->cb.obj        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrt_param_client_fetch_arr(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    asrt_param_arr_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_CTYPE_ARRAY;
        query->cb.arr        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, node_id, NULL );
}

// Typed find helpers — set expected_type + cb + cb_ptr, then submit find
static inline enum asrt_status asrt_param_client_find_any(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              parent_id,
    char const*               key,
    asrt_param_any_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_NONE;
        query->cb.any        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrt_param_client_find_u32(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              parent_id,
    char const*               key,
    asrt_param_u32_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_U32;
        query->cb.u32        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrt_param_client_find_i32(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              parent_id,
    char const*               key,
    asrt_param_i32_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_I32;
        query->cb.i32        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrt_param_client_find_str(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              parent_id,
    char const*               key,
    asrt_param_str_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_STR;
        query->cb.str        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrt_param_client_find_float(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              parent_id,
    char const*               key,
    asrt_param_float_cb       cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_FLOAT;
        query->cb.flt        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrt_param_client_find_bool(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              parent_id,
    char const*               key,
    asrt_param_bool_cb        cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_BOOL;
        query->cb.bln        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrt_param_client_find_obj(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              parent_id,
    char const*               key,
    asrt_param_obj_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_CTYPE_OBJECT;
        query->cb.obj        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrt_param_client_find_arr(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              parent_id,
    char const*               key,
    asrt_param_arr_cb         cb,
    void*                     cb_ptr )
{
        query->expected_type = ASRT_FLAT_CTYPE_ARRAY;
        query->cb.arr        = cb;
        query->cb_ptr        = cb_ptr;
        return asrt_param_client_query( query, client, parent_id, key );
}

void asrt_param_client_deinit( struct asrt_param_client* client );

#ifdef __cplusplus
}
#endif

#endif  // ASRT_PARAM_H
