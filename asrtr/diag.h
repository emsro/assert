
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
#include "../asrtl/log.h"
#include "../asrtl/util.h"

struct asrt_diag_client
{
        struct asrt_node   node;
        struct asrt_sender sendr;
};

enum asrt_status asrt_diag_client_init(
    struct asrt_diag_client* diag,
    struct asrt_node*        prev,
    struct asrt_sender       sender );

void asrt_diag_client_record(
    struct asrt_diag_client* diag,
    char const*              file,
    uint32_t                 line,
    char const*              extra );
void asrt_diag_client_deinit( struct asrt_diag_client* diag );

// Helper macro to record filename, if the method in question does not work, alternatives are:
// - -DASRT_FILENAME=__FILE__ directly (but it is long and includes path)
// - -DASRT_FILENAME=`"$(notdir $<)"` (works only with some compilers)
// - -DASRT_FILENAME=__FILE__ and -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/= (only on some compilers)

#ifndef ASRT_FILENAME
#define ASRT_FILENAME                                                 \
        ( strrchr( __FILE__, '/' )  ? strrchr( __FILE__, '/' ) + 1 :  \
          strrchr( __FILE__, '\\' ) ? strrchr( __FILE__, '\\' ) + 1 : \
                                      __FILE__ )
#endif

#define ASRT_CHECK( diag, rec, x )                                                        \
        do {                                                                              \
                if ( !( x ) ) {                                                           \
                        asrt_fail( ( rec ) );                                             \
                        asrt_diag_client_record( ( diag ), ASRT_FILENAME, __LINE__, #x ); \
                }                                                                         \
        } while ( 0 )

#define ASRT_REQUIRE( diag, rec, x )                                                      \
        do {                                                                              \
                if ( !( x ) ) {                                                           \
                        asrt_fail( ( rec ) );                                             \
                        asrt_diag_client_record( ( diag ), ASRT_FILENAME, __LINE__, #x ); \
                        return ASRT_SUCCESS;                                              \
                }                                                                         \
        } while ( 0 )

#ifdef __cplusplus
}
#endif

#endif  // ASRTR_DIAG_H
