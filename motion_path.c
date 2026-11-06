#include "motion_path.h"
#include <math.h>
#include <stdbool.h>

#define MOTION_TAU 6.28318530718f
#define PATH_SEGMENTS 6U

typedef struct
{
    float x;
    float y;
} PathAnchor;

MotionRelativeBounds motion_path_relative_bounds(MotionArea area, MotionPoint position)
{
    const int32_t half_x = area.width / 2U;
    const int32_t half_y = area.height / 2U;
    return (MotionRelativeBounds)
    {
        .min_x = -half_x - position.x,
        .max_x = (int32_t)area.width - half_x - position.x,
        .min_y = -half_y - position.y,
        .max_y = (int32_t)area.height - half_y - position.y,
    };
}

MotionPoint motion_path_return_step(MotionPoint position)
{
    const float distance = hypotf((float)position.x, (float)position.y);
    if (distance <= 4.0f)
    {
        return (MotionPoint)
        {
            0, 0
        };
    }
    const float scale = (distance - 4.0f) / distance;
    return (MotionPoint)
    {
        (int16_t)lroundf(position.x * scale), (int16_t)lroundf(position.y * scale)
    };
}

MotionPoint motion_path_clamp(MotionArea area, MotionPoint point)
{
    const int16_t half_x = area.width / 2U;
    const int16_t half_y = area.height / 2U;
    const int16_t max_x = area.width - half_x;
    const int16_t max_y = area.height - half_y;
    if (point.x < -half_x)
    {
        point.x = -half_x;
    }
    if (point.x > max_x)
    {
        point.x = max_x;
    }
    if (point.y < -half_y)
    {
        point.y = -half_y;
    }
    if (point.y > max_y)
    {
        point.y = max_y;
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

static float motion_circle_radius(MotionArea area, MotionPoint start, float nx, float ny)
{
    const MotionRelativeBounds bounds = motion_path_relative_bounds(area, start);
    float radius = (area.width < area.height ? area.width : area.height) / 2.0f;
    if (1.0f - nx > 0.000001f)
    {
        radius = fminf(radius, -bounds.min_x / (1.0f - nx));
    }
    if (1.0f + nx > 0.000001f)
    {
        radius = fminf(radius, bounds.max_x / (1.0f + nx));
    }
    if (1.0f - ny > 0.000001f)
    {
        radius = fminf(radius, -bounds.min_y / (1.0f - ny));
    }
    if (1.0f + ny > 0.000001f)
    {
        radius = fminf(radius, bounds.max_y / (1.0f + ny));
    }
    return fmaxf(0.0f, radius);
}

static void motion_path_generate_circle(MotionPath* path, MotionArea area, MotionPoint start, uint32_t seed)
{
    const float orientation = motion_random(&seed) * MOTION_TAU;
    float nx = cosf(orientation);
    float ny = sinf(orientation);
    const MotionRelativeBounds bounds = motion_path_relative_bounds(area, start);
    // A start on an edge permits only the inward normal as the center direction.
    if (bounds.min_x == 0 || bounds.max_x == 0)
    {
        nx = bounds.min_x == 0 ? 1.0f : -1.0f;
        ny = 0.0f;
    }
    if (bounds.min_y == 0 || bounds.max_y == 0)
    {
        nx = 0.0f;
        ny = bounds.min_y == 0 ? 1.0f : -1.0f;
    }
    const float radius = motion_circle_radius(area, start, nx, ny);
    const float center_x = start.x + radius * nx;
    const float center_y = start.y + radius * ny;
    const float direction = motion_random(&seed) < 0.5f ? -1.0f : 1.0f;
    for (uint16_t i = 0U; i <= MOTION_PATH_STEPS; i++)
    {
        const float angle = direction * MOTION_TAU * i / MOTION_PATH_STEPS;
        const float x = center_x - radius * (nx * cosf(angle) - ny * sinf(angle));
        const float y = center_y - radius * (nx * sinf(angle) + ny * cosf(angle));
        path->points[i] = motion_path_clamp(area, (MotionPoint)
        {
            (int16_t)lroundf(x), (int16_t)lroundf(y)
        });
    }
}

static void motion_path_generate_rectangle(MotionPath* path, MotionArea area, MotionPoint start, uint32_t seed)
{
    const MotionRelativeBounds bounds = motion_path_relative_bounds(area, (MotionPoint)
    {
        0, 0
    });
    MotionPoint corners[4];
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < 4U; i++)
    {
        const MotionPoint corner =
        {
            (int16_t)((i & 1U) ? bounds.max_x : bounds.min_x),
            (int16_t)((i & 2U) ? bounds.max_y : bounds.min_y),
        };
        if (corner.x != start.x && corner.y != start.y)
        {
            corners[count++] = corner;
        }
    }
    const MotionPoint opposite = corners[seed % count];
    const bool horizontal_first = (seed & 4U) != 0U;
    const MotionPoint vertices[] =
    {
        start,
        {
            horizontal_first ? opposite.x : start.x, horizontal_first ? start.y : opposite.y
        },
        opposite,
        {
            horizontal_first ? start.x : opposite.x, horizontal_first ? opposite.y : start.y
        },
        start,
    };
    for (uint16_t i = 0U; i < MOTION_PATH_STEPS; i++)
    {
        const uint16_t side = i / 32U;
        const float t = (float)(i % 32U) / 32.0f;
        const MotionPoint from = vertices[side];
        const MotionPoint to = vertices[side + 1U];
        path->points[i] = (MotionPoint)
        {
            (int16_t)lroundf(from.x + (to.x - from.x) * t),
            (int16_t)lroundf(from.y + (to.y - from.y) * t),
        };
    }
}

void motion_path_generate_loop(MotionPath* path, MotionArea area, MotionPoint start, MotionTrajectory trajectory, uint32_t seed)
{
    if (seed == 0U)
    {
        seed = 1U;
    }
    if (trajectory == MotionTrajectoryCircle)
    {
        motion_path_generate_circle(path, area, start, seed);
    }
    else
    {
        motion_path_generate_rectangle(path, area, start, seed);
    }
    path->points[0] = start;
    path->points[MOTION_PATH_STEPS] = start;
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
    const MotionRelativeBounds bounds = motion_path_relative_bounds(area, start);
    const float half_x = area.width / 2.0f;
    const float half_y = area.height / 2.0f;
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
            dx * distance - start.x, dy * distance - start.y
        };
    }
    // Endpoint error is relative to the current position; bounds stay anchored to the origin.
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
        x = fmaxf(bounds.min_x + 1.0f, fminf(bounds.max_x - 1.0f, x));
        y = fmaxf(bounds.min_y + 1.0f, fminf(bounds.max_y - 1.0f, y));
        path->points[i] = (MotionPoint)
        {
            (int16_t)(start.x + lroundf(x)), (int16_t)(start.y + lroundf(y))
        };
    }
    path->points[0] = start;
}
