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

typedef enum
{
    MotionTrajectoryRandom,
    MotionTrajectoryCircle,
    MotionTrajectoryRectangle,
    MotionTrajectoryCount,
} MotionTrajectory;

typedef struct
{
    int32_t min_x;
    int32_t max_x;
    int32_t min_y;
    int32_t max_y;
} MotionRelativeBounds;

MotionRelativeBounds motion_path_relative_bounds(MotionArea area, MotionPoint position);
MotionPoint motion_path_return_step(MotionPoint position);
MotionPoint motion_path_clamp(MotionArea area, MotionPoint point);
MotionPoint motion_path_advance(MotionArea area, MotionPoint position, MotionPoint target);
void motion_path_generate(MotionPath* path, MotionArea area, MotionPoint start, uint32_t seed);
void motion_path_generate_test(MotionPath* path, MotionArea area, MotionPoint start);
void motion_path_generate_loop(MotionPath* path, MotionArea area, MotionPoint start, MotionTrajectory trajectory, uint32_t seed);
