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

enum asrtr_param_client_pending
{
        ASRTR_PARAM_CLIENT_PENDING_NONE = 0,
        ASRTR_PARAM_CLIENT_PENDING_READY,
        ASRTR_PARAM_CLIENT_PENDING_DELIVER,
        ASRTR_PARAM_CLIENT_PENDING_QUERY_ERROR,
};

struct asrtr_param_client
{
        struct asrt_node   node;
        struct asrt_sender sendr;
        asrt_flat_id       root_id;
        int                ready;

        uint8_t*     cache_buf;
        uint32_t     cache_capacity;
        uint32_t     cache_len;           // valid bytes (includes trailing 4-byte next_sib)
        asrt_flat_id cache_next_sibling;  // trailing next_sibling_id from last RESPONSE

        struct asrtr_param_query* pending_query;

        uint32_t timeout;

        enum asrtr_param_client_pending pending;

        union
        {
                asrt_flat_id root_id;
                struct
                {
                        uint8_t      error_code;
                        asrt_flat_id node_id;
                } error;
        } pending_data;
};

enum asrt_status asrtr_param_client_init(
    struct asrtr_param_client* client,
    struct asrt_node*          prev,
    struct asrt_sender         sender,
    struct asrt_span           msg_buffer,
    uint32_t                   timeout );

asrt_flat_id asrtr_param_client_root_id( struct asrtr_param_client const* client );


typedef void (
    *asrtr_param_u32_cb )( struct asrtr_param_client*, struct asrtr_param_query*, uint32_t val );
typedef void (
    *asrtr_param_i32_cb )( struct asrtr_param_client*, struct asrtr_param_query*, int32_t val );
typedef void (
    *asrtr_param_str_cb )( struct asrtr_param_client*, struct asrtr_param_query*, char const* val );
typedef void (
    *asrtr_param_float_cb )( struct asrtr_param_client*, struct asrtr_param_query*, float val );
typedef void (
    *asrtr_param_bool_cb )( struct asrtr_param_client*, struct asrtr_param_query*, uint32_t val );
typedef void ( *asrtr_param_obj_cb )(
    struct asrtr_param_client*,
    struct asrtr_param_query*,
    struct asrt_flat_child_list val );
typedef void ( *asrtr_param_arr_cb )(
    struct asrtr_param_client*,
    struct asrtr_param_query*,
    struct asrt_flat_child_list val );
typedef void ( *asrtr_param_any_cb )(
    struct asrtr_param_client*,
    struct asrtr_param_query*,
    struct asrt_flat_value val );

union asrtr_param_cb
{
        asrtr_param_any_cb   any;
        asrtr_param_u32_cb   u32;
        asrtr_param_i32_cb   i32;
        asrtr_param_str_cb   str;
        asrtr_param_float_cb flt;
        asrtr_param_bool_cb  bln;
        asrtr_param_obj_cb   obj;
        asrtr_param_arr_cb   arr;
};

struct asrtr_param_query
{
        enum asrt_param_err_e error_code;
        asrt_flat_id          node_id;
        char const*           key;
        asrt_flat_id          next_sibling;

        asrt_flat_value_type expected_type;
        union asrtr_param_cb cb;
        void*                cb_ptr;

        uint32_t start;
};

// Submit a query — expects cb, cb_ptr, and expected_type to be pre-set.
// If key is NULL, search by node_id; otherwise search by key.
enum asrt_status asrtr_param_client_query(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               node_id,
    char const*                key );

static inline int asrtr_param_query_pending( struct asrtr_param_client const* client )
{
        return client->pending_query != NULL;
}

// Typed query helpers — set expected_type + cb + cb_ptr, then submit
static inline enum asrt_status asrtr_param_client_fetch_any(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               node_id,
    asrtr_param_any_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_NONE;
        query->cb.any        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrtr_param_client_fetch_u32(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               node_id,
    asrtr_param_u32_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_U32;
        query->cb.u32        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrtr_param_client_fetch_i32(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               node_id,
    asrtr_param_i32_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_I32;
        query->cb.i32        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrtr_param_client_fetch_str(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               node_id,
    asrtr_param_str_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_STR;
        query->cb.str        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrtr_param_client_fetch_float(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               node_id,
    asrtr_param_float_cb       cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_FLOAT;
        query->cb.flt        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrtr_param_client_fetch_bool(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               node_id,
    asrtr_param_bool_cb        cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_BOOL;
        query->cb.bln        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrtr_param_client_fetch_obj(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               node_id,
    asrtr_param_obj_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_CTYPE_OBJECT;
        query->cb.obj        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, node_id, NULL );
}

static inline enum asrt_status asrtr_param_client_fetch_arr(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               node_id,
    asrtr_param_arr_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_CTYPE_ARRAY;
        query->cb.arr        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, node_id, NULL );
}

// Typed find helpers — set expected_type + cb + cb_ptr, then submit find
static inline enum asrt_status asrtr_param_client_find_any(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               parent_id,
    char const*                key,
    asrtr_param_any_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_NONE;
        query->cb.any        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrtr_param_client_find_u32(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               parent_id,
    char const*                key,
    asrtr_param_u32_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_U32;
        query->cb.u32        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrtr_param_client_find_i32(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               parent_id,
    char const*                key,
    asrtr_param_i32_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_I32;
        query->cb.i32        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrtr_param_client_find_str(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               parent_id,
    char const*                key,
    asrtr_param_str_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_STR;
        query->cb.str        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrtr_param_client_find_float(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               parent_id,
    char const*                key,
    asrtr_param_float_cb       cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_FLOAT;
        query->cb.flt        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrtr_param_client_find_bool(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               parent_id,
    char const*                key,
    asrtr_param_bool_cb        cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_STYPE_BOOL;
        query->cb.bln        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrtr_param_client_find_obj(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               parent_id,
    char const*                key,
    asrtr_param_obj_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_CTYPE_OBJECT;
        query->cb.obj        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, parent_id, key );
}

static inline enum asrt_status asrtr_param_client_find_arr(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrt_flat_id               parent_id,
    char const*                key,
    asrtr_param_arr_cb         cb,
    void*                      cb_ptr )
{
        query->expected_type = ASRT_FLAT_CTYPE_ARRAY;
        query->cb.arr        = cb;
        query->cb_ptr        = cb_ptr;
        return asrtr_param_client_query( query, client, parent_id, key );
}

void asrtr_param_client_deinit( struct asrtr_param_client* client );

#ifdef __cplusplus
}
#endif

#endif  // ASRTR_PARAM_H
