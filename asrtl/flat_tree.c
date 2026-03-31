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

static inline char* asrtl_flat_strdup( struct asrtl_allocator* alloc, char const* str )
{
        uint32_t len  = (uint32_t) strlen( str ) + 1;
        char*    copy = (char*) asrtl_alloc( alloc, len );
        if ( copy )
                memcpy( copy, str, len );
        return copy;
}

enum asrtl_status asrtl_flat_block_init(
    struct asrtl_flat_block* block,
    struct asrtl_allocator*  alloc,
    uint32_t                 node_capacity )
{
        if ( !block || node_capacity == 0 ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "block_init: NULL block or zero capacity" );
                return ASRTL_INIT_ERR;
        }

        block->nodes = (struct asrtl_flat_node*) asrtl_alloc(
            alloc, sizeof( struct asrtl_flat_node ) * node_capacity );
        if ( !block->nodes ) {
                ASRTL_ERR_LOG(
                    "asrtl_flat_tree", "block_init: alloc failed for %u nodes", node_capacity );
                return ASRTL_ALLOC_ERR;
        }
        for ( uint32_t i = 0; i < node_capacity; i++ )
                block->nodes[i] =
                    ( struct asrtl_flat_node ){ .value = { .type = ASRTL_FLAT_VALUE_TYPE_NONE } };
        block->node_count = 0;
        return ASRTL_SUCCESS;
}

static void asrtl_flat_free_node_strings(
    struct asrtl_allocator* alloc,
    struct asrtl_flat_node* node )
{
        if ( node->key )
                asrtl_free( alloc, (void**) &node->key );
        if ( node->value.type == ASRTL_FLAT_VALUE_TYPE_STR && node->value.str_val )
                asrtl_free( alloc, (void**) &node->value.str_val );
}

enum asrtl_status asrtl_flat_block_deinit(
    struct asrtl_flat_block* block,
    struct asrtl_allocator*  alloc,
    uint32_t                 node_capacity )
{
        if ( !block ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "block_deinit: NULL block" );
                return ASRTL_INIT_ERR;
        }
        if ( block->nodes != NULL ) {
                for ( uint32_t i = 0; i < node_capacity; i++ )
                        if ( block->nodes[i].value.type != ASRTL_FLAT_VALUE_TYPE_NONE )
                                asrtl_flat_free_node_strings( alloc, &block->nodes[i] );
                asrtl_free( alloc, (void**) &block->nodes );
        }
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
        if ( !t ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "init: NULL tree" );
                return ASRTL_INIT_ERR;
        }

        t->alloc  = alloc;
        t->blocks = (struct asrtl_flat_block*) asrtl_alloc(
            &t->alloc, sizeof( struct asrtl_flat_block ) * block_capacity );
        if ( !t->blocks ) {
                ASRTL_ERR_LOG(
                    "asrtl_flat_tree", "init: alloc failed for %u blocks", block_capacity );
                return ASRTL_ALLOC_ERR;
        }
        for ( uint32_t i = 0; i < block_capacity; i++ )
                t->blocks[i] = ( struct asrtl_flat_block ){ .nodes = NULL, .node_count = 0 };
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
    struct asrtl_allocator* alloc,
    char const*             key,
    struct asrtl_flat_value value )
{
        if ( !node ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "set_node: NULL node" );
                return ASRTL_ARG_ERR;
        }
        if ( key ) {
                node->key = asrtl_flat_strdup( alloc, key );
                if ( !node->key ) {
                        ASRTL_ERR_LOG( "asrtl_flat_tree", "set_node: key alloc failed" );
                        return ASRTL_ALLOC_ERR;
                }
        } else {
                node->key = NULL;
        }
        node->value = value;
        if ( value.type == ASRTL_FLAT_VALUE_TYPE_STR && value.str_val ) {
                node->value.str_val = asrtl_flat_strdup( alloc, value.str_val );
                if ( !node->value.str_val ) {
                        ASRTL_ERR_LOG( "asrtl_flat_tree", "set_node: str_val alloc failed" );
                        asrtl_free( alloc, (void**) &node->key );
                        return ASRTL_ALLOC_ERR;
                }
        }
        node->next_sibling = 0;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtl_flat_ensure_capacity(
    struct asrtl_flat_tree* t,
    asrtl_flat_id           node_id )
{
        uint32_t capacity = t->node_capacity * t->block_capacity;
        if ( node_id >= capacity ) {
                uint32_t new_block_capacity = t->block_capacity == 0 ? 1 : t->block_capacity * 2;
                while ( node_id >= t->node_capacity * new_block_capacity )
                        new_block_capacity *= 2;
                struct asrtl_flat_block* new_blocks = asrtl_flat_realloc_blocks(
                    &t->alloc, t->blocks, t->block_capacity, new_block_capacity );
                if ( !new_blocks ) {
                        ASRTL_ERR_LOG(
                            "asrtl_flat_tree", "ensure_capacity: realloc blocks failed" );
                        return ASRTL_ALLOC_ERR;
                }
                t->blocks         = new_blocks;
                t->block_capacity = new_block_capacity;
        }
        struct asrtl_flat_block* block = &t->blocks[node_id / t->node_capacity];
        if ( block->nodes == NULL ) {
                enum asrtl_status st = asrtl_flat_block_init( block, &t->alloc, t->node_capacity );
                if ( st != ASRTL_SUCCESS )
                        return st;
        }
        return ASRTL_SUCCESS;
}

static inline struct asrtl_flat_child_list* asrtl_flat_get_child_list(
    struct asrtl_flat_node* parent,
    asrtl_flat_id           parent_id,
    char const*             key )
{
        if ( parent->value.type == ASRTL_FLAT_VALUE_TYPE_OBJECT ) {
                if ( key == NULL ) {
                        ASRTL_ERR_LOG( "asrtl_flat_tree", "object node must have a key" );
                        return NULL;
                }
                return &parent->value.obj_val;
        }
        if ( parent->value.type == ASRTL_FLAT_VALUE_TYPE_ARRAY ) {
                if ( key != NULL ) {
                        ASRTL_ERR_LOG( "asrtl_flat_tree", "array node cannot have a key" );
                        return NULL;
                }
                return &parent->value.arr_val;
        }
        ASRTL_ERR_LOG(
            "asrtl_flat_tree",
            "parent_id=%u is not a container (type=%d)",
            parent_id,
            parent->value.type );
        return NULL;
}

static enum asrtl_status asrtl_flat_link_child(
    struct asrtl_flat_tree* t,
    asrtl_flat_id           parent_id,
    asrtl_flat_id           node_id,
    char const*             key )
{
        if ( parent_id == 0 )
                return ASRTL_SUCCESS;

        struct asrtl_flat_node* parent = asrtl_flat_get_node( t, parent_id );
        if ( !parent ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "link_child: parent_id=%u not found", parent_id );
                return ASRTL_ARG_ERR;
        }

        struct asrtl_flat_child_list* child_list =
            asrtl_flat_get_child_list( parent, parent_id, key );
        if ( !child_list )
                return ASRTL_ARG_ERR;

        if ( child_list->first_child == 0 ) {
                child_list->first_child = node_id;
        } else {
                struct asrtl_flat_node* last_child =
                    asrtl_flat_get_node( t, child_list->last_child );
                if ( !last_child ) {
                        ASRTL_ERR_LOG( "asrtl_flat_tree", "link_child: last_child node missing" );
                        return ASRTL_INTERNAL_ERR;
                }
                last_child->next_sibling = node_id;
        }
        child_list->last_child = node_id;
        return ASRTL_SUCCESS;
}

enum asrtl_status asrtl_flat_tree_append(
    struct asrtl_flat_tree* t,
    asrtl_flat_id           parent_id,
    asrtl_flat_id           node_id,
    char const*             key,
    struct asrtl_flat_value value )
{
        if ( !t ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "append: NULL tree" );
                return ASRTL_INIT_ERR;
        }
        if ( node_id == 0 || node_id == parent_id ) {
                ASRTL_ERR_LOG(
                    "asrtl_flat_tree",
                    "append: invalid node_id=%u parent_id=%u",
                    node_id,
                    parent_id );
                return ASRTL_ARG_ERR;
        }

        enum asrtl_status st = asrtl_flat_ensure_capacity( t, node_id );
        if ( st != ASRTL_SUCCESS )
                return st;

        struct asrtl_flat_node* node = asrtl_flat_get_node( t, node_id );
        if ( !node ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "append: node_id=%u not reachable", node_id );
                return ASRTL_ARG_ERR;
        }
        if ( node->value.type != ASRTL_FLAT_VALUE_TYPE_NONE ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "append: node_id=%u already exists", node_id );
                return ASRTL_ARG_ERR;
        }

        st = asrtl_flat_set_node( node, &t->alloc, key, value );
        if ( st != ASRTL_SUCCESS )
                return st;

        return asrtl_flat_link_child( t, parent_id, node_id, key );
}

enum asrtl_status asrtl_flat_tree_query(
    struct asrtl_flat_tree*         t,
    asrtl_flat_id                   node_id,
    struct asrtl_flat_query_result* result )
{
        if ( !t || !result ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "query: NULL tree or result" );
                return ASRTL_INIT_ERR;
        }
        struct asrtl_flat_node* node = asrtl_flat_get_node( t, node_id );
        if ( !node ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "query: node_id=%u not found", node_id );
                return ASRTL_ARG_ERR;
        }
        result->id           = node_id;
        result->key          = node->key;
        result->value        = node->value;
        result->next_sibling = node->next_sibling;
        return ASRTL_SUCCESS;
}

enum asrtl_status asrtl_flat_tree_find_by_key(
    struct asrtl_flat_tree*         t,
    asrtl_flat_id                   parent_id,
    char const*                     key,
    struct asrtl_flat_query_result* result )
{
        if ( !t || !key || !result ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "find_by_key: NULL argument" );
                return ASRTL_INIT_ERR;
        }
        struct asrtl_flat_node* parent = asrtl_flat_get_node( t, parent_id );
        if ( !parent ) {
                ASRTL_ERR_LOG(
                    "asrtl_flat_tree", "find_by_key: parent_id=%u not found", parent_id );
                return ASRTL_ARG_ERR;
        }

        asrtl_flat_id child_id = 0;
        if ( parent->value.type == ASRTL_FLAT_VALUE_TYPE_OBJECT )
                child_id = parent->value.obj_val.first_child;
        else {
                ASRTL_ERR_LOG(
                    "asrtl_flat_tree", "find_by_key: parent_id=%u is not an object", parent_id );
                return ASRTL_ARG_ERR;
        }

        while ( child_id != 0 ) {
                struct asrtl_flat_node* child = asrtl_flat_get_node( t, child_id );
                if ( !child )
                        break;
                if ( child->key && strcmp( child->key, key ) == 0 ) {
                        result->id           = child_id;
                        result->key          = child->key;
                        result->value        = child->value;
                        result->next_sibling = child->next_sibling;
                        return ASRTL_SUCCESS;
                }
                child_id = child->next_sibling;
        }

        ASRTL_ERR_LOG(
            "asrtl_flat_tree", "find_by_key: key '%s' not found in parent %u", key, parent_id );
        return ASRTL_ARG_ERR;
}

enum asrtl_status asrtl_flat_tree_deinit( struct asrtl_flat_tree* t )
{
        if ( !t ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "deinit: NULL tree" );
                return ASRTL_INIT_ERR;
        }

        for ( uint32_t i = 0; i < t->block_capacity; i++ )
                asrtl_flat_block_deinit( &t->blocks[i], &t->alloc, t->node_capacity );
        asrtl_free( &t->alloc, (void**) &t->blocks );
        return ASRTL_SUCCESS;
}
