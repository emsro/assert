
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
#include "../asrtl/status_to_str.h"
#include "../asrtl/util.h"
#include "./status.h"

#include <string.h>

struct asrtr_diag
{
        struct asrtl_node   node;
        struct asrtl_sender sendr;
};

inline static enum asrtl_status asrtr_diag_recv( void* data, struct asrtl_span buff )
{
        (void) data;
        (void) buff;
        ASRTL_ERR_LOG( "asrtr_diag", "Received unexpected message on diag channel" );
        return ASRTL_SUCCESS;
}

inline static enum asrtr_status asrtr_diag_init(
    struct asrtr_diag*  diag,
    struct asrtl_node*  prev,
    struct asrtl_sender sender )
{
        if ( !diag || !prev ) {
                ASRTL_ERR_LOG( "asrtr_diag", "Invalid arguments to diag init" );
                return ASRTR_INIT_ERR;
        }
        *diag = ( struct asrtr_diag ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_DIAG,
                    .recv_ptr = diag,
                    .recv_cb  = asrtr_diag_recv,
                    .next     = NULL,
                },
            .sendr = sender,
        };
        prev->next = &diag->node;
        return ASRTR_SUCCESS;
}
inline static void asrtr_diag_record( struct asrtr_diag* diag, char const* file, uint32_t line )
{
        ASRTL_ASSERT( diag );
        ASRTL_ASSERT( file );

        uint8_t  prefix[4];
        uint8_t* p = prefix;
        asrtl_add_u32( &p, line );
        struct asrtl_rec_span line_buff = {
            .b = prefix, .e = prefix + sizeof prefix, .next = NULL };
        struct asrtl_rec_span file_buff = {
            .b = (uint8_t*) file, .e = (uint8_t*) file + strlen( file ), .next = NULL };
        line_buff.next = &file_buff;

        /*
        enum asrtl_status st = asrtl_send( &diag->sendr, ASRTL_DIAG, &line_buff );
        if ( st != ASRTL_SUCCESS )
                ASRTL_ERR_LOG(
                    "asrtr_diag", "Failed to send diag message: %s", asrtl_status_to_str( st ) );
                    */
}

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

#define ASRTR_CHECK( diag, rec, x )                                              \
        do {                                                                     \
                if ( !( x ) ) {                                                  \
                        asrtr_fail( ( rec ) );                                   \
                        asrtr_diag_record( ( diag ), ASRTR_FILENAME, __LINE__ ); \
                }                                                                \
        } while ( 0 )

#define ASRTR_REQUIRE( diag, rec, x )                                            \
        do {                                                                     \
                if ( !( x ) ) {                                                  \
                        asrtr_fail( ( rec ) );                                   \
                        asrtr_diag_record( ( diag ), ASRTR_FILENAME, __LINE__ ); \
                        return ASRTR_SUCCESS;                                    \
                }                                                                \
        } while ( 0 )

#ifdef __cplusplus
}
#endif

#endif  // ASRTR_DIAG_H
