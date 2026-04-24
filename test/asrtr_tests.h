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

#include "../asrtr/diag.h"
#include "../asrtr/record.h"

struct astrt_check_ctx
{
        struct asrtr_diag* diag;
        uint64_t           counter;
};

enum asrtl_status require_macro_test( struct asrtr_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRTR_REQUIRE( ctx->diag, r, 1 == 1 );
        ctx->counter += 1;
        ASRTR_REQUIRE( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        return ASRTL_SUCCESS;
}

enum asrtl_status check_macro_test( struct asrtr_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRTR_CHECK( ctx->diag, r, 1 == 1 );
        ctx->counter += 1;
        ASRTR_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        return ASRTL_SUCCESS;
}

enum asrtl_status countdown_test( struct asrtr_record* x )
{
        uint64_t* p = (uint64_t*) x->inpt->test_ptr;
        *p -= 1;
        if ( *p == 0 )
                x->state = ASRTR_TEST_PASS;
        else
                x->state = ASRTR_TEST_RUNNING;
        return ASRTL_SUCCESS;
}

struct insta_test_data
{
        enum asrtr_test_state state;
        uint64_t              counter;
};
enum asrtl_status insta_test_fun( struct asrtr_record* x )
{
        struct insta_test_data* p = (struct insta_test_data*) x->inpt->test_ptr;
        p->counter += 1;
        x->state = p->state;
        return ASRTL_SUCCESS;
}

// continue_f returns non-SUCCESS → record->state forced to ASRTR_TEST_ERROR
enum asrtl_status error_continue_fun( struct asrtr_record* x )
{
        (void) x;
        return ASRTL_INTERNAL_ERR;
}

// two consecutive CHECK failures → two diag messages, counter = 2
enum asrtl_status check_macro_two_fails( struct asrtr_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRTR_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        ASRTR_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        return ASRTL_SUCCESS;
}

// one CHECK failure then one pass → one diag message, counter = 2
enum asrtl_status check_macro_fail_pass( struct asrtr_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRTR_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        ASRTR_CHECK( ctx->diag, r, 1 == 1 );
        ctx->counter += 1;
        return ASRTL_SUCCESS;
}

// failing REQUIRE → CHECK and counter after it unreachable
enum asrtl_status require_then_check( struct asrtr_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRTR_REQUIRE( ctx->diag, r, 1 == 0 );
        ASRTR_CHECK( ctx->diag, r, 1 == 0 );  // unreachable
        ctx->counter += 1;                    // unreachable
        return ASRTL_SUCCESS;
}

// CHECK fails, REQUIRE passes, CHECK fails → two diag messages, counter = 3
enum asrtl_status mix_check_require_check( struct asrtr_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRTR_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        ASRTR_REQUIRE( ctx->diag, r, 1 == 1 );
        ctx->counter += 1;
        ASRTR_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        return ASRTL_SUCCESS;
}

// CHECK fails, REQUIRE fails → two diag messages, counter = 1
enum asrtl_status mix_check_require_fail( struct asrtr_record* r )
{
        struct astrt_check_ctx* ctx = (struct astrt_check_ctx*) r->inpt->test_ptr;
        ASRTR_CHECK( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;
        ASRTR_REQUIRE( ctx->diag, r, 1 == 0 );
        ctx->counter += 1;  // unreachable
        return ASRTL_SUCCESS;
}

#endif
