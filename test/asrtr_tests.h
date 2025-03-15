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
#ifndef ASRTR_TESTS_H
#define ASRTR_TESTS_H

#include "../asrtr/record.h"

enum asrtr_status require_macro_test( struct asrtr_record* r )
{
        uint64_t* p = (uint64_t*) r->data;
        ASRTR_REQUIRE( r, 1 == 1 );
        *p += 1;
        ASRTR_REQUIRE( r, 1 == 0 );
        *p += 1;
        return ASRTR_SUCCESS;
}

enum asrtr_status check_macro_test( struct asrtr_record* r )
{
        uint64_t* p = (uint64_t*) r->data;
        ASRTR_CHECK( r, 1 == 1 );
        *p += 1;
        ASRTR_CHECK( r, 1 == 0 );
        *p += 1;
        return ASRTR_SUCCESS;
}

enum asrtr_status countdown_test( struct asrtr_record* x )
{
        uint64_t* p = (uint64_t*) x->data;
        *p -= 1;
        if ( *p == 0 )
                x->state = ASRTR_TEST_PASS;
        else
                x->state = ASRTR_TEST_RUNNING;
        return ASRTR_SUCCESS;
}

struct insta_test_data
{
        enum asrtr_test_state state;
        uint64_t              counter;
};
enum asrtr_status insta_test_fun( struct asrtr_record* x )
{
        struct insta_test_data* p = (struct insta_test_data*) x->data;
        p->counter += 1;
        x->state = p->state;
        return ASRTR_SUCCESS;
}

#endif
