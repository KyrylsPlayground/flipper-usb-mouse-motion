#pragma once

#include "motion_path.h"

typedef struct
{
    uint32_t offset_ticks;
    uint32_t duration_ticks;
    uint16_t point_stride;
} MotionTiming;

MotionTiming motion_timing_plan(
    uint32_t interval_ticks,
    uint32_t reserve_ticks,
    uint32_t min_step_ticks,
    uint32_t start_random,
    uint32_t duration_random);
