
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

#ifndef ASRTR_DIAG_H
#define ASRTR_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "../asrtl/diag_proto.h"
#include "../asrtl/log.h"
#include "../asrtl/util.h"

/// Diagnostic client module — DIAG channel, reactor side.
/// Sends RECORD messages to the controller when assertions fail.
struct asrt_diag_client
{
        struct asrt_node            node;
        struct asrt_diag_record_msg msg;
};

typedef void ( *asrt_diag_record_done_cb )( void* ptr, enum asrt_status status );

enum asrt_diag_record_result
{
        ASRT_DIAG_RECORD_ACCEPTED = 0,
        ASRT_DIAG_RECORD_BUSY     = 1,
};

/// Initialise the diag client and link it into the channel chain after @p prev.
enum asrt_status asrt_diag_client_init( struct asrt_diag_client* diag, struct asrt_node* prev );

/// Enqueue a diagnostic record sourced from @p file:@p line with optional expression @p extra.
/// Returns ASRT_DIAG_RECORD_BUSY if a previous record send is still in progress.
enum asrt_diag_record_result asrt_diag_client_record(
    struct asrt_diag_client* diag,
    char const*              file,
    uint32_t                 line,
    char const*              extra,
    asrt_diag_record_done_cb done_cb,
    void*                    done_ptr );
/// Unlink and release the diag client.
void asrt_diag_client_deinit( struct asrt_diag_client* diag );

/// Resolves to the basename of __FILE__, falling back gracefully on compilers
/// that cannot strip the path at compile time.

#ifndef ASRT_FILENAME

#if defined( __FILE_NAME__ )
#define ASRT_FILENAME __FILE_NAME__
#else
#error "ASRT_FILENAME not defined and __FILE_NAME__ is unavailable on this compiler"
#endif

#endif

/// Check @p x; if false, mark the record as failed and send a RECORD message (non-fatal).
#define ASRT_CHECK( diag, rec, x )                                               \
        do {                                                                     \
                if ( !( x ) ) {                                                  \
                        ( rec )->state = ASRT_TEST_FAIL;                         \
                        asrt_diag_client_record(                                 \
                            ( diag ), ASRT_FILENAME, __LINE__, #x, NULL, NULL ); \
                }                                                                \
        } while ( 0 )

/// Check @p x; if false, mark the record as failed, send a RECORD message, and return (fatal).
#define ASRT_REQUIRE( diag, rec, x )                                             \
        do {                                                                     \
                if ( !( x ) ) {                                                  \
                        ( rec )->state = ASRT_TEST_FAIL;                         \
                        asrt_diag_client_record(                                 \
                            ( diag ), ASRT_FILENAME, __LINE__, #x, NULL, NULL ); \
                        return ASRT_SUCCESS;                                     \
                }                                                                \
        } while ( 0 )

#ifdef __cplusplus
}
#endif

#endif  // ASRTR_DIAG_H
