#pragma once

#include "motion_path.h"

#define MOTION_SPEED_MIN 100U
#define MOTION_SPEED_MAX 700U

typedef struct
{
    uint32_t duration_ms;
    uint32_t pause_ms;
    uint16_t pause_index;
} MotionSpeedTiming;

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

void motion_timing_offsets(
    uint32_t* offsets, uint16_t reports, uint32_t duration_ticks,
    uint32_t min_step_ticks, uint32_t pause_limit_ticks, uint32_t seed);

MotionSpeedTiming motion_timing_speed_offsets(
    const MotionPath* path, uint16_t stride, uint32_t target_ms,
    uint32_t* offsets_ms, uint32_t seed);
