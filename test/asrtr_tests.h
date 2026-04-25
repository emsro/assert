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
#ifndef ASRT_TESTS_H
#define ASRT_TESTS_H

#include "../asrtr/diag.h"
#include "../asrtr/record.h"

struct astrt_check_ctx
{
        struct asrt_diag_client* diag;
        uint64_t                 counter;
};

enum asrt_status require_macro_test( struct asrt_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRT_REQUIRE( ctx->diag, r, 1 == 1 );
        ctx->counter += 1;
        ASRT_REQUIRE( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        return ASRT_SUCCESS;
}

enum asrt_status check_macro_test( struct asrt_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRT_CHECK( ctx->diag, r, 1 == 1 );
        ctx->counter += 1;
        ASRT_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        return ASRT_SUCCESS;
}

enum asrt_status countdown_test( struct asrt_record* x )
{
        uint64_t* p = (uint64_t*) x->inpt->test_ptr;
        *p -= 1;
        if ( *p == 0 )
                x->state = ASRT_TEST_PASS;
        else
                x->state = ASRT_TEST_RUNNING;
        return ASRT_SUCCESS;
}

struct insta_test_data
{
        enum asrt_test_state state;
        uint64_t             counter;
};
enum asrt_status insta_test_fun( struct asrt_record* x )
{
        struct insta_test_data* p = (struct insta_test_data*) x->inpt->test_ptr;
        p->counter += 1;
        x->state = p->state;
        return ASRT_SUCCESS;
}

// continue_f returns non-SUCCESS → record->state forced to ASRT_TEST_ERROR
enum asrt_status error_continue_fun( struct asrt_record* x )
{
        (void) x;
        return ASRT_INTERNAL_ERR;
}

// two consecutive CHECK failures → two diag messages, counter = 2
enum asrt_status check_macro_two_fails( struct asrt_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRT_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        ASRT_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        return ASRT_SUCCESS;
}

// one CHECK failure then one pass → one diag message, counter = 2
enum asrt_status check_macro_fail_pass( struct asrt_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRT_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        ASRT_CHECK( ctx->diag, r, 1 == 1 );
        ctx->counter += 1;
        return ASRT_SUCCESS;
}

// failing REQUIRE → CHECK and counter after it unreachable
enum asrt_status require_then_check( struct asrt_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRT_REQUIRE( ctx->diag, r, 1 == 0 );
        ASRT_CHECK( ctx->diag, r, 1 == 0 );  // unreachable
        ctx->counter += 1;                   // unreachable
        return ASRT_SUCCESS;
}

// CHECK fails, REQUIRE passes, CHECK fails → two diag messages, counter = 3
enum asrt_status mix_check_require_check( struct asrt_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRT_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        ASRT_REQUIRE( ctx->diag, r, 1 == 1 );
        ctx->counter += 1;
        ASRT_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        return ASRT_SUCCESS;
}

// CHECK fails, REQUIRE fails → two diag messages, counter = 1
enum asrt_status mix_check_require_fail( struct asrt_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRT_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        ASRT_REQUIRE( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;  // unreachable
        return ASRT_SUCCESS;
}

#endif
