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
#ifndef ASRTC_STATUS_TO_STR_H
#define ASRTC_STATUS_TO_STR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./status.h"

inline static char const* asrtc_status_to_str( enum asrtc_status st )
{
        switch ( st ) {
        case ASRTC_CNTR_CB_ERR:
                return "controller callback error";
        case ASRTC_SEND_ERR:
                return "send error";
        case ASRTC_CNTR_BUSY_ERR:
                return "controller busy error";
        case ASRTC_CNTR_INIT_ERR:
                return "controller init error";
        case ASRTC_SUCCESS:
                return "success";
        }
        return "unknown error";
}

#ifdef __cplusplus
}
#endif

#endif
