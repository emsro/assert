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

/// Arena-allocated flat tree for JSON-like structured data.
///
/// Nodes are addressed by integer IDs (asrtl_flat_id). ID 0 is a virtual root
/// sentinel — never stores data, serves as parent for top-level nodes.
///
/// Storage is split into fixed-size blocks, lazily allocated on first use.
/// Blocks grow by doubling when a node_id exceeds current capacity.
///
/// Children form a singly-linked list via next_sibling. Container nodes
/// (OBJECT, ARRAY) track first_child/last_child for O(1) append.
///
/// Strings (keys and STR values) are copied via the allocator and freed on
/// tree deinit — callers need not keep the originals alive.
///
/// Each node_id may be appended exactly once; duplicate appends are rejected.

#ifndef ASRTL_FLAT_TREE_H
#define ASRTL_FLAT_TREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./allocator.h"
#include "./status.h"
#include "./util.h"

#include <stdint.h>
#include <string.h>

typedef uint32_t asrtl_flat_id;

/// Scalar value types — leaf data carried by nodes.
enum asrtl_flat_stype
{
        ASRTL_FLAT_STYPE_NONE  = 0,
        ASRTL_FLAT_STYPE_STR   = 1,
        ASRTL_FLAT_STYPE_U32   = 2,
        ASRTL_FLAT_STYPE_FLOAT = 3,
        ASRTL_FLAT_STYPE_BOOL  = 4,
        ASRTL_FLAT_STYPE_NULL  = 5,
        ASRTL_FLAT_STYPE_I32   = 6,
};

/// Container value types — nodes that hold child lists.
enum asrtl_flat_ctype
{
        ASRTL_FLAT_CTYPE_NONE   = 0,
        ASRTL_FLAT_CTYPE_OBJECT = 7,
        ASRTL_FLAT_CTYPE_ARRAY  = 8,
};

/// Common type for both scalar and container type tags.  The enum values do
/// not overlap (except NONE = 0), so a single uint8_t can hold either.
typedef uint8_t asrtl_flat_value_type;

struct asrtl_flat_child_list
{
        asrtl_flat_id first_child;
        asrtl_flat_id last_child;
};

union asrtl_flat_scalar
{
        char const* str_val;
        uint32_t    u32_val;
        int32_t     i32_val;
        float       float_val;
        uint32_t    bool_val;
};

union asrtl_flat_data
{
        union asrtl_flat_scalar      s;
        struct asrtl_flat_child_list cont;
};

struct asrtl_flat_value
{
        asrtl_flat_value_type type;
        union asrtl_flat_data data;
};

struct asrtl_flat_node
{
        struct asrtl_flat_value value;
        char const*   key;  ///< Owned copy. Non-NULL for OBJECT children, NULL for ARRAY children.
        asrtl_flat_id next_sibling;  ///< 0 = no next sibling.
};

/// Contiguous array of nodes. Lazily allocated per block.
struct asrtl_flat_block
{
        struct asrtl_flat_node* nodes;
        uint32_t                node_count;
};

enum asrtl_status asrtl_flat_block_init(
    struct asrtl_flat_block* block,
    struct asrtl_allocator*  alloc,
    uint32_t                 node_capacity );

enum asrtl_status asrtl_flat_block_deinit(
    struct asrtl_flat_block* block,
    struct asrtl_allocator*  alloc,
    uint32_t                 node_capacity );

/// node_id maps to blocks[node_id / node_capacity][node_id % node_capacity].
struct asrtl_flat_tree
{
        struct asrtl_allocator alloc;

        struct asrtl_flat_block* blocks;
        uint32_t                 block_capacity;

        uint32_t node_capacity;  ///< Nodes per block, fixed at init.
};

enum asrtl_status asrtl_flat_tree_init(
    struct asrtl_flat_tree* tree,
    struct asrtl_allocator  alloc,
    uint32_t                block_capacity,
    uint32_t                node_capacity );

/// Insert a scalar leaf node. Grows blocks if node_id exceeds capacity.
/// parent_id=0 for root nodes. OBJECT parents require non-NULL key,
/// ARRAY parents require NULL key. Rejects duplicate node_ids.
enum asrtl_status asrtl_flat_tree_append_scalar(
    struct asrtl_flat_tree* tree,
    asrtl_flat_id           parent_id,
    asrtl_flat_id           node_id,
    char const*             key,
    asrtl_flat_value_type   type,
    union asrtl_flat_scalar scalar );

/// Insert a container node (OBJECT or ARRAY). Same rules as append_scalar.
enum asrtl_status asrtl_flat_tree_append_cont(
    struct asrtl_flat_tree* tree,
    asrtl_flat_id           parent_id,
    asrtl_flat_id           node_id,
    char const*             key,
    asrtl_flat_value_type   type );

struct asrtl_flat_query_result
{
        asrtl_flat_id           id;
        char const*             key;
        struct asrtl_flat_value value;
        asrtl_flat_id           next_sibling;  ///< 0 = no next sibling.
};

/// Read a single node by ID.
enum asrtl_status asrtl_flat_tree_query(
    struct asrtl_flat_tree const*   tree,
    asrtl_flat_id                   node_id,
    struct asrtl_flat_query_result* result );

/// Find a child of parent_id whose key matches. Returns the child's query result.
enum asrtl_status asrtl_flat_tree_find_by_key(
    struct asrtl_flat_tree*         tree,
    asrtl_flat_id                   parent_id,
    char const*                     key,
    struct asrtl_flat_query_result* result );

enum asrtl_status asrtl_flat_tree_deinit( struct asrtl_flat_tree* tree );

/// Returns the number of bytes the value payload occupies on the wire (not
/// including node_id, key, or type byte).
static inline size_t asrtl_flat_value_wire_size( struct asrtl_flat_value v )
{
        switch ( v.type ) {
        case ASRTL_FLAT_STYPE_STR:
                return v.data.s.str_val ? strlen( v.data.s.str_val ) + 1U : 1U;
        case ASRTL_FLAT_STYPE_U32:
        case ASRTL_FLAT_STYPE_BOOL:
        case ASRTL_FLAT_STYPE_FLOAT:
        case ASRTL_FLAT_STYPE_I32:
                return 4U;
        case ASRTL_FLAT_CTYPE_OBJECT:
        case ASRTL_FLAT_CTYPE_ARRAY:
                return 8U;
        default:
                return 0U;
        }
}

/// Serialize a flat_value into the buffer at *p, advancing *p past the written
/// bytes.  Does not write the type byte — caller is responsible for that.
static inline void asrtl_flat_value_write( uint8_t** p, struct asrtl_flat_value v )
{
        switch ( v.type ) {
        case ASRTL_FLAT_STYPE_NONE:
        case ASRTL_FLAT_STYPE_NULL:
                break;
        case ASRTL_FLAT_STYPE_STR: {
                size_t sl = strlen( v.data.s.str_val );
                memcpy( *p, v.data.s.str_val, sl );
                *p += sl;
                *( *p )++ = '\0';
                break;
        }
        case ASRTL_FLAT_STYPE_U32:
                asrtl_add_u32( p, v.data.s.u32_val );
                break;
        case ASRTL_FLAT_STYPE_I32:
                asrtl_add_i32( p, v.data.s.i32_val );
                break;
        case ASRTL_FLAT_STYPE_BOOL:
                asrtl_add_u32( p, v.data.s.bool_val );
                break;
        case ASRTL_FLAT_STYPE_FLOAT: {
                uint32_t bits;
                memcpy( &bits, &v.data.s.float_val, sizeof bits );
                asrtl_add_u32( p, bits );
                break;
        }
        case ASRTL_FLAT_CTYPE_OBJECT:
        case ASRTL_FLAT_CTYPE_ARRAY:
                asrtl_add_u32( p, v.data.cont.first_child );
                asrtl_add_u32( p, v.data.cont.last_child );
                break;
        default:
                break;
        }
}

/// Decode a flat_value from the buffer, advancing buff->b past the consumed
/// bytes.  raw_type is the type byte already read by the caller.
enum asrtl_status asrtl_flat_value_decode(
    struct asrtl_span*       buff,
    uint8_t                  raw_type,
    struct asrtl_flat_value* val );

#ifdef __cplusplus
}
#endif

#endif
