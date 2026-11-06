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

typedef struct
{
    uint16_t width;
    uint16_t height;
} MotionArea;

void motion_path_generate(MotionPath* path, MotionArea area, MotionPoint start, uint32_t seed);
void motion_path_generate_test(MotionPath* path, MotionArea area, MotionPoint start);
