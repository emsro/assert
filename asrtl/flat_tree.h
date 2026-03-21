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

#include <stdint.h>

typedef uint32_t asrtl_flat_id;

/// Value type 0 is reserved — indicates an uninitialized node slot.
enum asrtl_flat_value_type
{
        ASRTL_FLAT_VALUE_TYPE_NONE   = 0,
        ASRTL_FLAT_VALUE_TYPE_STR    = 1,
        ASRTL_FLAT_VALUE_TYPE_U32    = 2,
        ASRTL_FLAT_VALUE_TYPE_FLOAT  = 3,
        ASRTL_FLAT_VALUE_TYPE_OBJECT = 4,
        ASRTL_FLAT_VALUE_TYPE_ARRAY  = 5,
        ASRTL_FLAT_VALUE_TYPE_BOOL   = 6,
        ASRTL_FLAT_VALUE_TYPE_NULL   = 7,
};

struct asrtl_flat_child_list
{
        asrtl_flat_id first_child;
        asrtl_flat_id last_child;
};

struct asrtl_flat_value
{
        enum asrtl_flat_value_type type;
        union
        {
                char const*                  str_val;
                uint32_t                     u32_val;
                float                        float_val;
                struct asrtl_flat_child_list obj_val;
                struct asrtl_flat_child_list arr_val;
                uint32_t                     bool_val;
        };
};

/// Value constructors. Strings are copied into the tree on append.
static inline struct asrtl_flat_value asrtl_flat_value_null( void )
{
        return ( struct asrtl_flat_value ){ .type = ASRTL_FLAT_VALUE_TYPE_NULL };
}
static inline struct asrtl_flat_value asrtl_flat_value_bool( uint32_t val )
{
        return ( struct asrtl_flat_value ){ .type = ASRTL_FLAT_VALUE_TYPE_BOOL, .bool_val = val };
}
static inline struct asrtl_flat_value asrtl_flat_value_u32( uint32_t val )
{
        return ( struct asrtl_flat_value ){ .type = ASRTL_FLAT_VALUE_TYPE_U32, .u32_val = val };
}
static inline struct asrtl_flat_value asrtl_flat_value_str( char const* val )
{
        return ( struct asrtl_flat_value ){ .type = ASRTL_FLAT_VALUE_TYPE_STR, .str_val = val };
}
static inline struct asrtl_flat_value asrtl_flat_value_float( float val )
{
        return ( struct asrtl_flat_value ){ .type = ASRTL_FLAT_VALUE_TYPE_FLOAT, .float_val = val };
}
static inline struct asrtl_flat_value asrtl_flat_value_object( void )
{
        return ( struct asrtl_flat_value ){
            .type = ASRTL_FLAT_VALUE_TYPE_OBJECT, .obj_val = { 0, 0 } };
}
static inline struct asrtl_flat_value asrtl_flat_value_array( void )
{
        return ( struct asrtl_flat_value ){
            .type = ASRTL_FLAT_VALUE_TYPE_ARRAY, .arr_val = { 0, 0 } };
}

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

/// Insert a node. Grows blocks if node_id exceeds capacity.
/// parent_id=0 for root nodes. OBJECT parents require non-NULL key,
/// ARRAY parents require NULL key. Rejects duplicate node_ids.
enum asrtl_status asrtl_flat_tree_append(
    struct asrtl_flat_tree* tree,
    asrtl_flat_id           parent_id,
    asrtl_flat_id           node_id,
    char const*             key,
    struct asrtl_flat_value value );

struct asrtl_flat_query_result
{
        asrtl_flat_id           id;
        char const*             key;
        struct asrtl_flat_value value;
};

/// Read a single node by ID.
enum asrtl_status asrtl_flat_tree_query(
    struct asrtl_flat_tree*         tree,
    asrtl_flat_id                   node_id,
    struct asrtl_flat_query_result* result );

enum asrtl_status asrtl_flat_tree_deinit( struct asrtl_flat_tree* tree );

#ifdef __cplusplus
}
#endif

#endif
