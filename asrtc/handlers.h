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
#include "./status.h"

// XXX: the defs might be at better place  // C07
typedef enum asrtc_status ( *asrtc_init_callback )( void* ptr, enum asrtc_status s );
typedef enum asrtc_status (
    *asrtc_test_count_callback )( void* ptr, enum asrtc_status s, uint16_t test_count );
typedef enum asrtc_status ( *asrtc_desc_callback )( void* ptr, enum asrtc_status s, char* desc );
typedef enum asrtc_status (
    *asrtc_test_info_callback )( void* ptr, enum asrtc_status s, char* desc );
typedef enum asrtc_status (
    *asrtc_test_result_callback )( void* ptr, enum asrtc_status s, struct asrtc_result* res );

enum asrtc_stage_e
{
        ASRTC_STAGE_INIT    = 0x01,
        ASRTC_STAGE_WAITING = 0x02,
        ASRTC_STAGE_END     = 0x03,
};

struct asrtc_init_handler
{
        asrtc_init_callback cb;
        void*               ptr;
        uint32_t            timeout_ticks;
        struct
        {
                uint16_t major;
                uint16_t minor;
                uint16_t patch;
        } ver;
};


struct asrtc_tc_handler
{
        uint16_t                  count;
        void*                     ptr;
        asrtc_test_count_callback cb;
        uint32_t                  timeout_ticks;
};

struct asrtc_desc_handler
{
        char*               desc;
        void*               ptr;
        asrtc_desc_callback cb;
        uint32_t            timeout_ticks;
};

struct asrtc_ti_handler
{
        uint16_t                 tid;
        char*                    desc;
        void*                    ptr;
        asrtc_test_info_callback cb;
        uint32_t                 timeout_ticks;
};

struct asrtc_exec_handler
{
        struct asrtc_result        res;
        void*                      ptr;
        asrtc_test_result_callback cb;
        uint32_t                   timeout_ticks;
};

#ifdef __cplusplus
}
#endif

#endif
