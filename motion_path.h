#pragma once

#include <stdint.h>

#define MOTION_PATH_STEPS 128U

typedef struct
{
    int16_t x;
    int16_t y;
} MotionPoint;

typedef struct
{
    MotionPoint points[MOTION_PATH_STEPS + 1U];
} MotionPath;

void motion_path_generate(MotionPath* path, uint16_t radius, MotionPoint start, uint32_t seed);
