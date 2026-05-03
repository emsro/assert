
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
#ifndef ASRT_REACTOR_H
#define ASRT_REACTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "../asrtl/core_proto.h"
#include "../asrtl/status.h"
#include "record.h"

#include <stdint.h>

/// A single test registered with the reactor.
/// Must remain at a stable address from the point of registration until
/// asrt_reactor_deinit() is called — the reactor holds a raw pointer to it.
///
/// Lifetime of a test execution: the controller sends a TEST_START message,
/// the reactor calls start_f once per tick until start_f returns, then sends
/// the result back.  The same asrt_test instance may be executed multiple times
/// across different test runs without re-registration.
struct asrt_test
{
        char const*        desc;     ///< Human-readable test name.
        void*              ptr;      ///< Context pointer forwarded to start_f.
        asrt_test_callback start_f;  ///< Entry point; called each tick while the test is running.
        struct asrt_test*  next;     ///< Intrusive linked-list link.
};

/// Internal state machine states for the reactor's CORE channel.
enum asrt_reactor_state
{
        ASRT_REAC_IDLE        = 1,  ///< No test running, ready for requests.
        ASRT_REAC_TEST_EXEC   = 2,  ///< Test is executing (start_f being called each tick).
        ASRT_REAC_TEST_REPORT = 3,  ///< Test finished, sending the result message.
        ASRT_REAC_WAIT_SEND   = 4,  ///< Blocked until test_start_msg send completes.
};

enum asrt_reactor_flags
{
        ASRT_FLAG_DESC      = 0x01,
        ASRT_FLAG_PROTO_VER = 0x02,
        ASRT_FLAG_TC        = 0x04,
        ASRT_FLAG_TI        = 0x08,
        ASRT_FLAG_TSTART    = 0x10,
        ASRT_FLAG_LOCKED    = 0x20,
};

// Mask of flags that carry persistent state (not pending work).
// Used to strip passive bits when checking for actionable flags.
#define ASRT_PASSIVE_FLAGS ( ASRT_FLAG_LOCKED )

struct asrt_reactor
{
        struct asrt_node node;
        char const*      desc;

        struct asrt_test* first_test;
        struct asrt_test* last_test;

        enum asrt_reactor_state state;
        struct asrt_reac_wait_send
        {
                enum asrt_reactor_state next_state;
                uint32_t                err_run_id;
                struct asrt_u8d8msg     err_result_msg;
        } wait_send;
        struct asrt_test_input test_info;
        struct asrt_record     record;

        uint32_t flags;  // values of asrt_reactor_flags

        uint16_t recv_test_info_id;
        uint16_t recv_test_start_id;
        uint32_t recv_test_run_id;

        struct asrt_u8d8msg            proto_ver_msg;
        struct asrt_core_desc_msg      desc_msg;
        struct asrt_u8d4msg            tc_msg;
        struct asrt_core_test_info_msg ti_msg;
        struct asrt_u8d8msg            test_start_msg;
        struct asrt_u8d8msg            test_result_msg;
};

/// Initialise the reactor and link it into @p send_queue.
/// @p desc is a human-readable description of the target (borrowed, must remain valid).
enum asrt_status asrt_reactor_init(
    struct asrt_reactor*       reac,
    struct asrt_send_req_list* send_queue,
    char const*                desc );

/// Initialise a test structure.  @p ptr is forwarded to @p start_f on every execution tick.
enum asrt_status asrt_test_init(
    struct asrt_test*  t,
    char const*        desc,
    void*              ptr,
    asrt_test_callback start_f );
/// Append @p test to the reactor’s test list.  The test pointer must remain valid.
enum asrt_status asrt_reactor_add_test( struct asrt_reactor* reac, struct asrt_test* test );
/// Unlink the reactor from the channel chain and release internal resources.
void asrt_reactor_deinit( struct asrt_reactor* reac );

#ifdef __cplusplus
}
#endif

#endif
