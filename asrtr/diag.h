
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
#include "./status.h"

struct asrtr_diag
{
        struct asrtl_node   node;
        struct asrtl_sender sendr;
};

enum asrtr_status asrtr_diag_init(
    struct asrtr_diag*  diag,
    struct asrtl_node*  prev,
    struct asrtl_sender sender );

void asrtr_diag_record(
    struct asrtr_diag* diag,
    char const*        file,
    uint32_t           line,
    char const*        extra );
void asrtr_diag_deinit( struct asrtr_diag* diag );

// Helper macro to record filename, if the method in question does not work, alternatives are:
// - -DASRTR_FILENAME=__FILE__ directly (but it is long and includes path)
// - -DASRTR_FILENAME=`"$(notdir $<)"` (works only with some compilers)
// - -DASRTR_FILENAME=__FILE__ and -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/= (only on some compilers)

#ifndef ASRTR_FILENAME
#define ASRTR_FILENAME                                                \
        ( strrchr( __FILE__, '/' )  ? strrchr( __FILE__, '/' ) + 1 :  \
          strrchr( __FILE__, '\\' ) ? strrchr( __FILE__, '\\' ) + 1 : \
                                      __FILE__ )
#endif

#define ASRTR_CHECK( diag, rec, x )                                                  \
        do {                                                                         \
                if ( !( x ) ) {                                                      \
                        asrtr_fail( ( rec ) );                                       \
                        asrtr_diag_record( ( diag ), ASRTR_FILENAME, __LINE__, #x ); \
                }                                                                    \
        } while ( 0 )

#define ASRTR_REQUIRE( diag, rec, x )                                                \
        do {                                                                         \
                if ( !( x ) ) {                                                      \
                        asrtr_fail( ( rec ) );                                       \
                        asrtr_diag_record( ( diag ), ASRTR_FILENAME, __LINE__, #x ); \
                        return ASRTR_SUCCESS;                                        \
                }                                                                    \
        } while ( 0 )

#ifdef __cplusplus
}
#endif

#endif  // ASRTR_DIAG_H
