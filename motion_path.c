#include "motion_path.h"
#include <math.h>

#define MOTION_TAU 6.28318530718f

static float motion_random(uint32_t* seed)
{
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return (float)(*seed & 0xFFFFU) / 65535.0f;
}

void motion_path_generate(MotionPath* path, uint16_t radius, uint32_t seed)
{
    if (seed == 0U)
    {
        seed = 1U;
    }
    const float phase_x = motion_random(&seed) * MOTION_TAU;
    const float phase_y = motion_random(&seed) * MOTION_TAU;
    const float harmonic_x = 0.25f + motion_random(&seed) * 0.5f;
    const float harmonic_y = 0.25f + motion_random(&seed) * 0.5f;
    const float angle = motion_random(&seed) * MOTION_TAU;
    const float direction = motion_random(&seed) < 0.5f ? -1.0f : 1.0f;
    const float rotation_x = cosf(angle);
    const float rotation_y = sinf(angle);
    float max_distance = 1.0f;

    path->points[0] = (MotionPoint)
    {
        0, 0
    };
    // Harmonics share the same period: both position and tangent close smoothly.
    for (uint16_t i = 1U; i < MOTION_PATH_STEPS; i++)
    {
        const float t = direction * MOTION_TAU * i / MOTION_PATH_STEPS;
        const float x = sinf(t) + harmonic_x * (sinf(2.0f * t + phase_x) - sinf(phase_x));
        const float y = cosf(t) - 1.0f + harmonic_y * (sinf(3.0f * t + phase_y) - sinf(phase_y));
        const float rotated_x = (x * rotation_x - y * rotation_y) * 4096.0f;
        const float rotated_y = (x * rotation_y + y * rotation_x) * 4096.0f;
        path->points[i].x = (int16_t)lroundf(rotated_x);
        path->points[i].y = (int16_t)lroundf(rotated_y);
        const float distance = hypotf(path->points[i].x, path->points[i].y);
        if (distance > max_distance)
        {
            max_distance = distance;
        }
    }

    // Leave one count for rounding so the requested radius is never exceeded.
    const float scale = (radius > 0U ? radius - 1U : 0U) / max_distance;
    for (uint16_t i = 1U; i < MOTION_PATH_STEPS; i++)
    {
        path->points[i].x = (int16_t)lroundf(path->points[i].x * scale);
        path->points[i].y = (int16_t)lroundf(path->points[i].y * scale);
    }
    path->points[MOTION_PATH_STEPS] = (MotionPoint)
    {
        0, 0
    };
}
