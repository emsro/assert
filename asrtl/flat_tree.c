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
                    ( struct asrtl_flat_node ){ .value = { .type = ASRTL_FLAT_STYPE_NONE } };
        block->node_count = 0;
        return ASRTL_SUCCESS;
}

static void asrtl_flat_free_node_strings(
    struct asrtl_allocator* alloc,
    struct asrtl_flat_node* node )
{
        if ( node->key )
                asrtl_free( alloc, (void**) &node->key );
        if ( node->value.type == ASRTL_FLAT_STYPE_STR && node->value.data.s.str_val )
                asrtl_free( alloc, (void**) &node->value.data.s.str_val );
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
                        if ( block->nodes[i].value.type != ASRTL_FLAT_STYPE_NONE )
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
    struct asrtl_flat_tree const* t,
    asrt_flat_id                  id )
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
    asrtl_flat_value_type   type,
    union asrt_flat_data    data )
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
        node->value.type = type;
        node->value.data = data;
        if ( type == ASRTL_FLAT_STYPE_STR && data.s.str_val ) {
                node->value.data.s.str_val = asrtl_flat_strdup( alloc, data.s.str_val );
                if ( !node->value.data.s.str_val ) {
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
    asrt_flat_id            node_id )
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

/// Validate parent/key constraints and locate the insertion points.
/// Walk keyed children to find the tail and reject duplicate keys.
/// Returns the next_sibling slot of the last child, or error on duplicate/broken chain.
static enum asrtl_status asrtl_flat_find_keyed_tail(
    struct asrtl_flat_tree* t,
    asrt_flat_id            first_child,
    asrt_flat_id            parent_id,
    char const*             key,
    asrt_flat_id**          link )
{
        asrt_flat_id            cid   = first_child;
        struct asrtl_flat_node* child = NULL;
        while ( cid != 0 ) {
                child = asrtl_flat_get_node( t, cid );
                if ( !child ) {
                        ASRTL_ERR_LOG(
                            "asrtl_flat_tree", "prepare_link: child node %u missing", cid );
                        return ASRTL_INTERNAL_ERR;
                }
                if ( child->key && strcmp( child->key, key ) == 0 ) {
                        ASRTL_ERR_LOG(
                            "asrtl_flat_tree",
                            "append: duplicate key '%s' under parent_id=%u",
                            key,
                            parent_id );
                        return ASRTL_ARG_ERR;
                }
                if ( child->next_sibling == 0 )
                        break;
                cid = child->next_sibling;
        }
        *link = &child->next_sibling;
        return ASRTL_SUCCESS;
}

/// On success, *link and *tail point to the two uint32_t slots that must
/// receive the new node_id to complete the linkage.  For parent_id==0
/// (root-level), both are set to NULL and the caller skips the write.
static enum asrtl_status asrtl_flat_prepare_link(
    struct asrtl_flat_tree* t,
    asrt_flat_id            parent_id,
    char const*             key,
    asrt_flat_id**          link,
    asrt_flat_id**          tail )
{
        *link = NULL;
        *tail = NULL;

        if ( parent_id == 0 )
                return ASRTL_SUCCESS;

        struct asrtl_flat_node* parent = asrtl_flat_get_node( t, parent_id );
        if ( !parent ) {
                ASRTL_ERR_LOG(
                    "asrtl_flat_tree", "prepare_link: parent_id=%u not found", parent_id );
                return ASRTL_ARG_ERR;
        }

        if ( parent->value.type == ASRTL_FLAT_CTYPE_OBJECT ) {
                if ( key == NULL ) {
                        ASRTL_ERR_LOG( "asrtl_flat_tree", "object node must have a key" );
                        return ASRTL_KEY_REQUIRED_ERR;
                }
        } else if ( parent->value.type == ASRTL_FLAT_CTYPE_ARRAY ) {
                if ( key != NULL ) {
                        ASRTL_ERR_LOG( "asrtl_flat_tree", "array node cannot have a key" );
                        return ASRTL_KEY_FORBIDDEN_ERR;
                }
        } else {
                ASRTL_ERR_LOG(
                    "asrtl_flat_tree",
                    "parent_id=%u is not a container (type=%d)",
                    parent_id,
                    parent->value.type );
                return ASRTL_ARG_ERR;
        }

        struct asrt_flat_child_list* cl = &parent->value.data.cont;
        if ( cl->first_child == 0 ) {
                *link = &cl->first_child;
        } else if ( key ) {
                enum asrtl_status s =
                    asrtl_flat_find_keyed_tail( t, cl->first_child, parent_id, key, link );
                if ( s != ASRTL_SUCCESS )
                        return s;
        } else {
                struct asrtl_flat_node* child = asrtl_flat_get_node( t, cl->last_child );
                if ( !child ) {
                        ASRTL_ERR_LOG(
                            "asrtl_flat_tree",
                            "prepare_link: last child node %u missing",
                            cl->last_child );
                        return ASRTL_INTERNAL_ERR;
                }
                *link = &child->next_sibling;
        }
        *tail = &cl->last_child;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtl_flat_tree_append_impl(
    struct asrtl_flat_tree* t,
    asrt_flat_id            parent_id,
    asrt_flat_id            node_id,
    char const*             key,
    asrtl_flat_value_type   type,
    union asrt_flat_data    data )
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
        if ( node->value.type != ASRTL_FLAT_STYPE_NONE ) {
                ASRTL_ERR_LOG( "asrtl_flat_tree", "append: node_id=%u already exists", node_id );
                return ASRTL_ARG_ERR;
        }

        asrt_flat_id* link = NULL;
        asrt_flat_id* tail = NULL;
        st                 = asrtl_flat_prepare_link( t, parent_id, key, &link, &tail );
        if ( st != ASRTL_SUCCESS )
                return st;

        st = asrtl_flat_set_node( node, &t->alloc, key, type, data );
        if ( st != ASRTL_SUCCESS )
                return st;

        if ( link && tail ) {
                *link = node_id;
                *tail = node_id;
        }
        return ASRTL_SUCCESS;
}

enum asrtl_status asrtl_flat_tree_append_scalar(
    struct asrtl_flat_tree* tree,
    asrt_flat_id            parent_id,
    asrt_flat_id            node_id,
    char const*             key,
    asrtl_flat_value_type   type,
    union asrt_flat_scalar  scalar )
{
        union asrt_flat_data data = { .s = scalar };
        return asrtl_flat_tree_append_impl( tree, parent_id, node_id, key, type, data );
}

enum asrtl_status asrtl_flat_tree_append_cont(
    struct asrtl_flat_tree* tree,
    asrt_flat_id            parent_id,
    asrt_flat_id            node_id,
    char const*             key,
    asrtl_flat_value_type   type )
{
        union asrt_flat_data data = { .cont = { 0, 0 } };
        return asrtl_flat_tree_append_impl( tree, parent_id, node_id, key, type, data );
}

enum asrtl_status asrtl_flat_tree_query(
    struct asrtl_flat_tree const*   t,
    asrt_flat_id                    node_id,
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
    asrt_flat_id                    parent_id,
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

        asrt_flat_id child_id = 0;
        if ( parent->value.type == ASRTL_FLAT_CTYPE_OBJECT )
                child_id = parent->value.data.cont.first_child;
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

enum asrtl_status asrtl_flat_value_decode(
    struct asrtl_span*      buff,
    uint8_t                 raw_type,
    struct asrt_flat_value* val )
{
        *val        = ( struct asrt_flat_value ){ 0 };
        val->type   = (asrtl_flat_value_type) raw_type;
        size_t need = asrtl_flat_value_wire_size( *val );
        if ( need > 0 && asrtl_span_unfit_for( buff, (uint32_t) need ) )
                return ASRTL_RECV_ERR;
        switch ( raw_type ) {
        case ASRTL_FLAT_STYPE_NONE:
        case ASRTL_FLAT_STYPE_NULL:
                break;
        case ASRTL_FLAT_STYPE_STR: {
                size_t   search_len = (size_t) ( buff->e - buff->b );
                uint8_t* snul       = (uint8_t*) memchr( buff->b, '\0', search_len );
                if ( !snul )
                        return ASRTL_RECV_ERR;
                val->data.s.str_val = (char const*) buff->b;
                buff->b             = snul + 1;
                break;
        }
        case ASRTL_FLAT_STYPE_U32:
                asrtl_cut_u32( &buff->b, &val->data.s.u32_val );
                break;
        case ASRTL_FLAT_STYPE_I32:
                asrtl_cut_i32( &buff->b, &val->data.s.i32_val );
                break;
        case ASRTL_FLAT_STYPE_BOOL:
                asrtl_cut_u32( &buff->b, &val->data.s.bool_val );
                break;
        case ASRTL_FLAT_STYPE_FLOAT: {
                uint32_t bits;
                asrtl_cut_u32( &buff->b, &bits );
                memcpy( &val->data.s.float_val, &bits, sizeof bits );
                break;
        }
        case ASRTL_FLAT_CTYPE_OBJECT:
        case ASRTL_FLAT_CTYPE_ARRAY:
                asrtl_cut_u32( &buff->b, &val->data.cont.first_child );
                asrtl_cut_u32( &buff->b, &val->data.cont.last_child );
                break;
        default:
                break;
        }
        return ASRTL_SUCCESS;
}
