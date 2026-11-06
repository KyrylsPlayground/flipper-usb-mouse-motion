#include "motion_path.h"
#include <math.h>

#define MOTION_TAU 6.28318530718f
#define PATH_SEGMENTS 6U

typedef struct
{
    float x;
    float y;
} PathAnchor;

static float motion_random(uint32_t* seed)
{
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return (float)(*seed & 0xFFFFU) / 65535.0f;
}

void motion_path_generate_test(MotionPath* path, uint16_t radius, MotionPoint start)
{
    const float edge = radius - 1.0f;
    for (uint16_t i = 0U; i <= MOTION_PATH_STEPS; i++)
    {
        float x;
        float y;
        if (i <= 16U)
        {
            const float t = (float)i / 16.0f;
            x = start.x + (edge - start.x) * t;
            y = start.y * (1.0f - t);
        }
        else if (i <= 112U)
        {
            const float angle = (float)(i - 16U) * MOTION_TAU / 96.0f;
            x = cosf(angle) * edge;
            y = sinf(angle) * edge;
        }
        else
        {
            x = edge * (MOTION_PATH_STEPS - i) / 16.0f;
            y = 0.0f;
        }
        path->points[i] = (MotionPoint)
        {
            (int16_t)lroundf(x), (int16_t)lroundf(y)
        };
    }
    path->points[0] = start;
    path->points[MOTION_PATH_STEPS] = (MotionPoint)
    {
        0, 0
    };
}

static float motion_interpolate(float a, float b, float c, float d, float t)
{
    return 0.5f * (2.0f * b + (c - a) * t +
                   (2.0f * a - 5.0f * b + 4.0f * c - d) * t * t +
                   (-a + 3.0f * b - 3.0f * c + d) * t * t * t);
}

void motion_path_generate(MotionPath* path, uint16_t radius, MotionPoint start, uint32_t seed)
{
    if (seed == 0U)
    {
        seed = 1U;
    }
    PathAnchor anchors[PATH_SEGMENTS + 1U] =
    {
        0
    };
    anchors[0] = (PathAnchor)
    {
        start.x, start.y
    };
    // After shrinking the radius while paused, recover from the actual position
    // without jumping or expanding the previous region.
    const float start_distance = hypotf(start.x, start.y);
    const float path_limit = start_distance > radius ? start_distance - 1.0f : radius - 1.0f;
    float angle = motion_random(&seed) * MOTION_TAU;
    const float direction = motion_random(&seed) < 0.5f ? -1.0f : 1.0f;
    for (uint16_t i = 1U; i < PATH_SEGMENTS; i++)
    {
        angle += direction * (0.7f + motion_random(&seed) * 0.9f);
        const float distance = radius * (0.25f + motion_random(&seed) * 0.55f);
        anchors[i] = (PathAnchor)
        {
            cosf(angle) * distance, sinf(angle) * distance
        };
    }
    // A small final correction ends near the origin instead of snapping exactly back.
    const float error_angle = motion_random(&seed) * MOTION_TAU;
    const float error_radius = 1.0f + motion_random(&seed) * 4.0f;
    anchors[PATH_SEGMENTS] = (PathAnchor)
    {
        cosf(error_angle) * error_radius,
        sinf(error_angle) * error_radius
    };
    const float correction = (motion_random(&seed) - 0.5f) * 6.0f;
    for (uint16_t i = 0U; i <= MOTION_PATH_STEPS; i++)
    {
        const float position = (float)i * PATH_SEGMENTS / MOTION_PATH_STEPS;
        uint16_t segment = (uint16_t)position;
        if (segment >= PATH_SEGMENTS)
        {
            segment = PATH_SEGMENTS - 1U;
        }
        const float t = position - segment;
        const PathAnchor a = anchors[segment > 0U ? segment - 1U : 0U];
        const PathAnchor b = anchors[segment];
        const PathAnchor c = anchors[segment + 1U];
        const PathAnchor d = anchors[segment + 2U <= PATH_SEGMENTS ? segment + 2U : PATH_SEGMENTS];
        float x = motion_interpolate(a.x, b.x, c.x, d.x, t);
        float y = motion_interpolate(a.y, b.y, c.y, d.y, t);
        if (segment == PATH_SEGMENTS - 1U)
        {
            const float adjustment = correction * 16.0f * t * t * (1.0f - t) * (1.0f - t);
            x += adjustment;
            y -= adjustment;
        }
        const float distance = hypotf(x, y);
        if (distance > path_limit)
        {
            const float scale = path_limit / distance;
            x *= scale;
            y *= scale;
        }
        path->points[i] = (MotionPoint)
        {
            (int16_t)lroundf(x), (int16_t)lroundf(y)
        };
    }
    path->points[0] = start;
}
