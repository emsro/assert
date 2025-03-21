
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
#ifndef ASRTC_DEFAULT_ERROR_CB_H
#define ASRTC_DEFAULT_ERROR_CB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/ecode_to_str.h"
#include "./callbacks.h"

#include <stdio.h>

static inline enum asrtc_status asrtc_default_error_callback(
    void*             ptr,
    enum asrtc_source src,
    uint16_t          ecode )
{
        (void) ptr;
        printf(
            "Error reported from %s: %i %s",
            src == ASRTC_CONTROLLER ? "controller" : "reactor",
            ecode,
            asrtl_ecode_to_str( ecode ) );
        return ASRTC_SUCCESS;
}

static inline struct asrtc_error_cb asrtc_default_error_cb( void )
{
        return ( struct asrtc_error_cb ){
            .ptr = NULL,
            .cb  = &asrtc_default_error_callback,
        };
}

#ifdef __cplusplus
}
#endif

#endif
