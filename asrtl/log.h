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

#ifndef ASRTL_LOG_H
#define ASRTL_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

enum asrtl_log_level
{
        ASRTL_LOG_DEBUG = 1,
        ASRTL_LOG_INFO  = 2,
        ASRTL_LOG_ERROR = 4,
};

void asrtl_log( enum asrtl_log_level level, char const* module, char const* fmt, ... );

#define ASRTL_DBG_LOG( module, ... ) asrtl_log( ASRTL_LOG_DEBUG, module, __VA_ARGS__ )

#define ASRTL_INF_LOG( module, ... ) asrtl_log( ASRTL_LOG_INFO, module, __VA_ARGS__ )

#define ASRTL_ERR_LOG( module, ... ) asrtl_log( ASRTL_LOG_ERROR, module, __VA_ARGS__ )

#define ASRTL_DEFINE_GPOS_LOG()                                                                \
        void asrtl_log( enum asrtl_log_level level, char const* module, char const* fmt, ... ) \
        {                                                                                      \
                                                                                               \
                char const* level_str = "UNK";                                                 \
                switch ( level ) {                                                             \
                case ASRTL_LOG_DEBUG:                                                          \
                        level_str = "\033[36mDEBUG\033[0m";                                    \
                        break;                                                                 \
                case ASRTL_LOG_INFO:                                                           \
                        level_str = "\033[32mINFO\033[0m";                                     \
                        break;                                                                 \
                case ASRTL_LOG_ERROR:                                                          \
                        level_str = "\033[31mERROR\033[0m";                                    \
                        break;                                                                 \
                default:                                                                       \
                        break;                                                                 \
                }                                                                              \
                                                                                               \
                char      timebuf[32];                                                         \
                time_t    t = time( NULL );                                                    \
                struct tm tm;                                                                  \
                localtime_r( &t, &tm );                                                        \
                strftime( timebuf, sizeof( timebuf ), "%Y%m%d %H%M%S", &tm );                  \
                                                                                               \
                fprintf( stderr, "%s %s %s :: ", timebuf, module ? module : "-", level_str );  \
                va_list args;                                                                  \
                va_start( args, fmt );                                                         \
                vfprintf( stderr, fmt, args );                                                 \
                va_end( args );                                                                \
                fprintf( stderr, "\n" );                                                       \
        }

#ifdef __cplusplus
}
#endif

#endif
