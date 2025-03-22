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
#ifndef ASRTL_STATUS_TO_STR_H
#define ASRTL_STATUS_TO_STR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./status.h"

inline static char const* asrtl_status_to_str( enum asrtl_status st )
{
        switch ( st ) {
        case ASRTL_CHANN_NOT_FOUND:
                return "channel not found";
        case ASRTL_SEND_ERR:
                return "send error";
        case ASRTL_RECV_INTERNAL_ERR:
                return "receive internal error";
        case ASRTL_RECV_UNKNOWN_ID_ERR:
                return "receive unknown id error";
        case ASRTL_RECV_UNEXPECTED_ERR:
                return "receive unexpected message error";
        case ASRTL_BUSY_ERR:
                return "busy error";
        case ASRTL_RECV_ERR:
                return "receive error";
        case ASRTL_SIZE_ERR:
                return "size error";
        case ASRTL_SUCCESS:
                return "success";
        }
        return "unknown error";
}

#ifdef __cplusplus
}
#endif

#endif
