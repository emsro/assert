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
#ifndef ASRTR_STATUS_TO_STR_H
#define ASRTR_STATUS_TO_STR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./status.h"

inline static char const* asrtr_status_to_str( enum asrtr_status st )
{
        switch ( st ) {
        case ASRTR_SEND_ERR:
                return "send error";
        case ASRTR_BUSY_ERR:
                return "busy error";
        case ASRTR_REAC_INIT_ERR:
                return "reactor init error";
        case ASRTR_TEST_INIT_ERR:
                return "test init error";
        case ASRTR_SUCCESS:
                return "success";
        }
        return "unknown error";
}

#ifdef __cplusplus
}
#endif

#endif
