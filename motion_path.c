#include "motion_path.h"
#include <math.h>

#define MOTION_TAU 6.28318530718f
#define PATH_SEGMENTS 6U

typedef struct
{
    float x;
    float y;
} PathAnchor;

MotionPoint motion_path_clamp(MotionArea area, MotionPoint point)
{
    const int16_t half_x = area.width / 2U;
    const int16_t half_y = area.height / 2U;
    if (point.x < -half_x)
    {
        point.x = -half_x;
    }
    if (point.x > half_x)
    {
        point.x = half_x;
    }
    if (point.y < -half_y)
    {
        point.y = -half_y;
    }
    if (point.y > half_y)
    {
        point.y = half_y;
    }
    return point;
}

MotionPoint motion_path_advance(MotionArea area, MotionPoint position, MotionPoint target)
{
    target = motion_path_clamp(area, target);
    int32_t dx = (int32_t)target.x - position.x;
    int32_t dy = (int32_t)target.y - position.y;
    if (dx < -127)
    {
        dx = -127;
    }
    if (dx > 127)
    {
        dx = 127;
    }
    if (dy < -127)
    {
        dy = -127;
    }
    if (dy > 127)
    {
        dy = 127;
    }
    position.x += dx;
    position.y += dy;
    return position;
}

static float motion_random(uint32_t* seed)
{
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return (float)(*seed & 0xFFFFU) / 65535.0f;
}

void motion_path_generate_test(MotionPath* path, MotionArea area, MotionPoint start)
{
    const float x_edge = area.width / 2.0f;
    const float y_edge = area.height / 2.0f;
    const PathAnchor corners[] =
    {
        {
            x_edge, 0
        },
        {
            x_edge, -y_edge
        },
        {
            -x_edge, -y_edge
        },
        {
            -x_edge, y_edge
        },
        {
            x_edge, y_edge
        },
        {
            x_edge, 0
        },
    };
    const uint16_t stops[] =
    {
        16U, 28U, 52U, 76U, 100U, 112U
    };
    uint16_t side = 0U;
    for (uint16_t i = 0U; i <= MOTION_PATH_STEPS; i++)
    {
        float x;
        float y;
        if (i <= 16U)
        {
            const float t = (float)i / 16.0f;
            x = start.x + (x_edge - start.x) * t;
            y = start.y * (1.0f - t);
        }
        else if (i <= 112U)
        {
            while (side < 4U && i > stops[side + 1U])
            {
                side++;
            }
            const float t = (float)(i - stops[side]) / (stops[side + 1U] - stops[side]);
            x = corners[side].x + (corners[side + 1U].x - corners[side].x) * t;
            y = corners[side].y + (corners[side + 1U].y - corners[side].y) * t;
        }
        else
        {
            x = x_edge * (MOTION_PATH_STEPS - i) / 16.0f;
            y = 0.0f;
        }
        path->points[i] = (MotionPoint)
        {
            (int16_t)lroundf(x), (int16_t)lroundf(y)
        };
        path->points[i] = motion_path_clamp(area, path->points[i]);
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

void motion_path_generate(MotionPath* path, MotionArea area, MotionPoint start, uint32_t seed)
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
    const float half_x = area.width / 2.0f;
    const float half_y = area.height / 2.0f;
    const float limit_x = half_x - 1.0f;
    const float limit_y = half_y - 1.0f;
    float angle = motion_random(&seed) * MOTION_TAU;
    const float direction = motion_random(&seed) < 0.5f ? -1.0f : 1.0f;
    for (uint16_t i = 1U; i < PATH_SEGMENTS; i++)
    {
        angle += direction * (0.7f + motion_random(&seed) * 0.9f);
        const float dx = cosf(angle);
        const float dy = sinf(angle);
        const float edge = fminf(half_x / fmaxf(fabsf(dx), 0.0001f),
                                 half_y / fmaxf(fabsf(dy), 0.0001f));
        const float distance = edge * (0.25f + motion_random(&seed) * 0.55f);
        anchors[i] = (PathAnchor)
        {
            dx * distance, dy * distance
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
        x = fmaxf(-limit_x, fminf(limit_x, x));
        y = fmaxf(-limit_y, fminf(limit_y, y));
        path->points[i] = (MotionPoint)
        {
            (int16_t)lroundf(x), (int16_t)lroundf(y)
        };
    }
    path->points[0] = start;
}
