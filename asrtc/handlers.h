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
#ifndef ASRTC_HANDLERS_H
#define ASRTC_HANDLERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./result.h"

typedef enum asrt_status ( *asrt_init_callback )( void* ptr, enum asrt_status s );
typedef enum asrt_status (
    *asrt_test_count_callback )( void* ptr, enum asrt_status s, uint16_t test_count );
typedef enum asrt_status ( *asrt_desc_callback )( void* ptr, enum asrt_status s, char const* desc );
typedef enum asrt_status (
    *asrt_test_info_callback )( void* ptr, enum asrt_status s, uint16_t tid, char const* desc );
typedef enum asrt_status (
    *asrt_test_result_callback )( void* ptr, enum asrt_status s, struct asrt_result* res );

enum asrt_stage_e
{
        ASRT_STAGE_INIT    = 0x01,
        ASRT_STAGE_WAITING = 0x02,
        ASRT_STAGE_END     = 0x03,
};

struct asrt_init_handler
{
        struct asrt_u8d2msg msg;

        asrt_init_callback cb;
        void*              ptr;
        uint32_t           timeout;
        struct
        {
                uint16_t major;
                uint16_t minor;
                uint16_t patch;
        } ver;
};


struct asrt_tc_handler
{
        struct asrt_u8d2msg msg;

        uint16_t                 count;
        void*                    ptr;
        asrt_test_count_callback cb;
        uint32_t                 timeout;
};

struct asrt_desc_handler
{
        struct asrt_u8d2msg msg;

        char*              desc;
        void*              ptr;
        asrt_desc_callback cb;
        uint32_t           timeout;
};

struct asrt_ti_handler
{
        struct asrt_u8d4msg msg;

        uint16_t                tid;
        asrt_test_info_result   result;
        char*                   desc;
        void*                   ptr;
        asrt_test_info_callback cb;
        uint32_t                timeout;
};

struct asrt_exec_handler
{
        struct asrt_u8d8msg msg;

        struct asrt_result        res;
        void*                     ptr;
        asrt_test_result_callback cb;
        uint32_t                  timeout;
};

#ifdef __cplusplus
}
#endif

#endif
