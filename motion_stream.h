#pragma once

#include "motion_path.h"
#include <stdbool.h>

typedef struct
{
    MotionPoint start;
    MotionPoint target;
    uint32_t duration;
    uint32_t elapsed;
    uint32_t frame;
    uint32_t frames;
} MotionStream;

void motion_stream_begin(MotionStream* stream, MotionPoint start, MotionPoint target, uint32_t duration, uint32_t frame_ticks, uint32_t minimum_ticks);
bool motion_stream_next(MotionStream* stream, MotionPoint* point, uint32_t* delay);
