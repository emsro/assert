
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
#ifndef ASRTL_STATUS_H
#define ASRTL_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

// XXX: check _recv of reactor/controller and revamp error codes
enum asrtl_status
{
        ASRTL_CHANN_NOT_FOUND     = -8,
        ASRTL_SEND_ERR            = -7,
        ASRTL_RECV_INTERNAL_ERR   = -6,
        ASRTL_RECV_UNEXPECTED_ERR = -5,
        ASRTL_RECV_UNKNOWN_ID_ERR = -4,
        ASRTL_BUSY_ERR            = -3,
        ASRTL_RECV_ERR            = -2,
        ASRTL_SIZE_ERR            = -1,
        ASRTL_SUCCESS             = 1,
};

#ifdef __cplusplus
}
#endif

#endif
