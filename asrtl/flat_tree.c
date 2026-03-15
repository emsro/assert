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
#include "./flat_tree.h"

#include "./log.h"

// XXX: add error logs
// XXX: add tests

enum asrtl_status asrtl_flat_block_init(
    struct asrtl_flat_block* block,
    struct asrtl_allocator*  alloc,
    uint32_t                 node_capacity )
{
        if ( !block || node_capacity == 0 )
                return ASRTL_INIT_ERR;

        block->nodes = (struct asrtl_flat_node*) asrtl_alloc(
            alloc, sizeof( struct asrtl_flat_node ) * node_capacity );
        if ( !block->nodes )
                return ASRTL_ALLOC_ERR;
        block->node_count = 0;
        return ASRTL_SUCCESS;
}

enum asrtl_status asrtl_flat_block_deinit(
    struct asrtl_flat_block* block,
    struct asrtl_allocator*  alloc )
{
        if ( !block )
                return ASRTL_INIT_ERR;
        if ( block->nodes != NULL )
                asrtl_free( alloc, (void**) &block->nodes );
        block->nodes      = NULL;
        block->node_count = 0;
        return ASRTL_SUCCESS;
}

static inline struct asrtl_flat_block* asrtl_flat_realloc_blocks(
    struct asrtl_allocator*  alloc,
    struct asrtl_flat_block* blocks,
    uint32_t                 blocks_capacity,
    uint32_t                 new_capacity )
{
        struct asrtl_flat_block* new_blocks = (struct asrtl_flat_block*) asrtl_alloc(
            alloc, sizeof( struct asrtl_flat_block ) * new_capacity );
        if ( !new_blocks )
                return NULL;
        uint32_t i = 0;
        for ( ; i < blocks_capacity; i++ )
                new_blocks[i] = blocks[i];
        for ( ; i < new_capacity; i++ )
                new_blocks[i] = ( struct asrtl_flat_block ){ .nodes = NULL, .node_count = 0 };
        asrtl_free( alloc, (void**) &blocks );
        return new_blocks;
}

enum asrtl_status asrtl_flat_tree_init(
    struct asrtl_flat_tree* t,
    struct asrtl_allocator  alloc,
    uint32_t                block_capacity,
    uint32_t                node_capacity )
{
        if ( !t )
                return ASRTL_INIT_ERR;

        t->alloc  = alloc;
        t->blocks = (struct asrtl_flat_block*) asrtl_alloc(
            &t->alloc, sizeof( struct asrtl_flat_block ) * block_capacity );
        if ( !t->blocks )
                return ASRTL_ALLOC_ERR;
        t->block_capacity = block_capacity;
        t->node_capacity  = node_capacity;

        return ASRTL_SUCCESS;
}

static inline struct asrtl_flat_node* asrtl_flat_get_node(
    struct asrtl_flat_tree* t,
    asrtl_flat_id           id )
{
        uint32_t capacity = t->node_capacity * t->block_capacity;
        if ( id >= capacity )
                return NULL;
        struct asrtl_flat_block* block = &t->blocks[id / t->node_capacity];
        if ( !block->nodes )
                return NULL;
        return &block->nodes[id % t->node_capacity];
}

static inline enum asrtl_status asrtl_flat_set_node(
    struct asrtl_flat_node* node,
    char const*             key,
    struct asrtl_flat_value value )
{
        if ( !node )
                return ASRTL_ARG_ERR;
        node->key          = key;
        node->value        = value;
        node->next_sibling = 0;
        return ASRTL_SUCCESS;
}

enum asrtl_status asrtl_flat_tree_append(
    struct asrtl_flat_tree* t,
    asrtl_flat_id           parent_id,
    asrtl_flat_id           node_id,
    char const*             key,
    struct asrtl_flat_value value )
{
        if ( !t )
                return ASRTL_INIT_ERR;
        if ( node_id == 0 || node_id == parent_id )
                return ASRTL_ARG_ERR;
        uint32_t capacity = t->node_capacity * t->block_capacity;
        if ( node_id >= capacity ) {
                uint32_t new_block_capacity = t->block_capacity == 0 ? 1 : t->block_capacity * 2;
                while ( node_id >= t->node_capacity * new_block_capacity )
                        new_block_capacity *= 2;
                struct asrtl_flat_block* new_blocks = asrtl_flat_realloc_blocks(
                    &t->alloc, t->blocks, t->block_capacity, new_block_capacity );
                if ( !new_blocks )
                        return ASRTL_ALLOC_ERR;
                t->blocks         = new_blocks;
                t->block_capacity = new_block_capacity;
        }
        struct asrtl_flat_block* block = &t->blocks[node_id / t->node_capacity];
        if ( block->nodes == NULL ) {
                enum asrtl_status st = asrtl_flat_block_init( block, &t->alloc, t->node_capacity );
                if ( st != ASRTL_SUCCESS )
                        return st;
        }
        struct asrtl_flat_node* node = asrtl_flat_get_node( t, node_id );
        if ( !node )
                return ASRTL_ARG_ERR;

        enum asrtl_status st = asrtl_flat_set_node( node, key, value );
        if ( st != ASRTL_SUCCESS )
                return st;

        if ( parent_id == 0 ) {
                // appending root node, no need to update parent's child list
                return ASRTL_SUCCESS;
        }
        struct asrtl_flat_node* parent = asrtl_flat_get_node( t, parent_id );
        if ( !parent )
                return ASRTL_ARG_ERR;
        struct asrtl_flat_child_list* child_list = NULL;
        if ( parent->value.type == ASRTL_FLAT_VALUE_TYPE_OBJECT ) {
                if ( key == NULL ) {
                        ASRTL_ERR_LOG( "asrtl_flat_tree", "object node must have a key" );
                        return ASRTL_ARG_ERR;
                }
                child_list = &parent->value.obj_val;
        } else if ( parent->value.type == ASRTL_FLAT_VALUE_TYPE_ARRAY ) {
                if ( key != NULL ) {
                        ASRTL_ERR_LOG( "asrtl_flat_tree", "array node cannot have a key" );
                        return ASRTL_ARG_ERR;
                }
                child_list = &parent->value.arr_val;
        } else {
                ASRTL_ERR_LOG(
                    "asrtl_flat_tree", "invalid parent value type: %d", parent->value.type );
        }

        if ( !child_list )
                return ASRTL_ARG_ERR;

        if ( child_list->first_child == 0 ) {
                child_list->first_child = node_id;
        } else {
                struct asrtl_flat_node* last_child =
                    asrtl_flat_get_node( t, child_list->last_child );
                if ( !last_child )
                        return ASRTL_INTERNAL_ERR;
                last_child->next_sibling = node_id;
        }
        child_list->last_child = node_id;
        return ASRTL_SUCCESS;
}

enum asrtl_status asrtl_flat_tree_deinit( struct asrtl_flat_tree* t )
{
        if ( !t )
                return ASRTL_INIT_ERR;

        for ( uint32_t i = 0; i < t->block_capacity; i++ )
                asrtl_flat_block_deinit( &t->blocks[i], &t->alloc );
        asrtl_free( &t->alloc, (void**) &t->blocks );
        return ASRTL_SUCCESS;
}
