#ifndef ASRTR_CHANN_H
#define ASRTR_CHANN_H

#include <stdint.h>

enum asrtl_chann_id_e
{
        ASRTL_META = 1,
        ASRTL_CORE = 2,
};

typedef uint16_t asrtl_chann_id;

enum asrtl_status
{
        ASRTL_SUCCESS = 1,
};

#endif
