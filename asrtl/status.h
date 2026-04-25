
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
#ifndef ASRT_STATUS_H
#define ASRT_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

enum asrt_status
{
        ASRT_VERSION_ERR  = -19,  // version mismatch between controller and reactor
        ASRT_TIMEOUT_ERR  = -18,  // operation timed out (e.g. waiting for controller init message)
        ASRT_CALLBACK_ERR = -17,
        ASRT_INVALID_EVENT_ERR   = -16,
        ASRT_KEY_FORBIDDEN_ERR   = -15,
        ASRT_KEY_REQUIRED_ERR    = -14,
        ASRT_INTERNAL_ERR        = -13,  // e.g. unexpected state in controller state machine
        ASRT_ARG_ERR             = -12,  // e.g. invalid argument passed to controller API
        ASRT_INIT_ERR            = -11,
        ASRT_RECV_TRAILING_ERR   = -10,
        ASRT_CHANN_NOT_FOUND     = -9,
        ASRT_ALLOC_ERR           = -8,  // e.g. failed to allocate buffer for sending message
        ASRT_SEND_ERR            = -7,
        ASRT_RECV_INTERNAL_ERR   = -6,
        ASRT_RECV_UNEXPECTED_ERR = -5,
        ASRT_RECV_UNKNOWN_ID_ERR = -4,
        ASRT_BUSY_ERR            = -3,  // e.g. starting init when not idle
        ASRT_RECV_ERR            = -2,
        ASRT_SIZE_ERR            = -1,  // e.g. message too large for buffer
        ASRT_SUCCESS             = 1,
};

#ifdef __cplusplus
}
#endif

#endif
