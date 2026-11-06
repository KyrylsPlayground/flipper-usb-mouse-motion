#include "motion_timing.h"
#include <math.h>

uint32_t motion_timing_test_offsets(const MotionPath* path, uint32_t* offsets_ms, uint16_t speed)
{
    if (speed == 0U)
    {
        for (uint16_t i = 0U; i < MOTION_PATH_STEPS; i++)
        {
            offsets_ms[i] = 2000U * (i + 1U) / MOTION_PATH_STEPS;
        }
        return 2000U;
    }
    uint32_t duration_ms = 0U;
    for (uint16_t i = 0U; i < MOTION_PATH_STEPS; i++)
    {
        const MotionPoint from = path->points[i];
        const MotionPoint to = path->points[i + 1U];
        const float distance = hypotf((float)(to.x - from.x), (float)(to.y - from.y));
        uint32_t delay = (uint32_t)lroundf(distance * 1000.0f / speed);
        if (delay < 2U)
        {
            delay = 2U;
        }
        duration_ms += delay;
        offsets_ms[i] = duration_ms;
    }
    return duration_ms;
}

MotionTiming motion_timing_plan(
    uint32_t interval_ticks,
    uint32_t reserve_ticks,
    uint32_t min_step_ticks,
    uint32_t start_random,
    uint32_t duration_random)
{
    // Even the 25% case must leave time for at least 64 USB reports.
    const uint32_t minimum_remaining = (MOTION_PATH_STEPS / 2U - 1U) * min_step_ticks * 4U;
    const uint32_t latest_start = interval_ticks - reserve_ticks - minimum_remaining;
    MotionTiming timing;
    timing.offset_ticks = start_random % (latest_start + 1U);
    const uint32_t remaining = interval_ticks - reserve_ticks - timing.offset_ticks;
    const uint32_t minimum_duration = (remaining + 3U) / 4U;
    const uint32_t maximum_duration = remaining * 95U / 100U;
    timing.duration_ticks = minimum_duration +
                            duration_random % (maximum_duration - minimum_duration + 1U);
    timing.point_stride = timing.duration_ticks >= (MOTION_PATH_STEPS - 1U) * min_step_ticks
                              ? 1U
                              : 2U;
    return timing;
}

MotionSpeedTiming motion_timing_speed_offsets(
    const MotionPath* path, uint16_t stride, uint32_t target_ms,
    uint32_t* offsets_ms, uint32_t seed, uint16_t min_speed, uint16_t max_speed)
{
    const uint16_t reports = MOTION_PATH_STEPS / stride;
    uint32_t proposed[MOTION_PATH_STEPS];
    motion_timing_offsets(proposed, reports, target_ms, 2U, 0U, seed);
    if (seed == 0U)
    {
        seed = 1U;
    }
    MotionSpeedTiming result =
    {
        .duration_ms = 0U,
        .pause_ms = 40U + seed % 81U,
        .pause_index = 1U + (seed >> 8) % (reports - 2U),
    };
    for (uint16_t i = 0U; i < reports; i++)
    {
        const MotionPoint from = path->points[i * stride];
        const MotionPoint to = path->points[(i + 1U) * stride];
        const float distance = hypotf((float)(to.x - from.x), (float)(to.y - from.y));
        uint32_t minimum_ms = (uint32_t)ceilf(distance * 1000.0f / max_speed);
        uint32_t maximum_ms = (uint32_t)floorf(distance * 1000.0f / min_speed);
        if (minimum_ms < 2U)
        {
            minimum_ms = 2U;
        }
        if (maximum_ms < minimum_ms)
        {
            maximum_ms = minimum_ms;
        }
        uint32_t delay = i == 0U ? proposed[1] : proposed[i] - proposed[i - 1U];
        if (delay < minimum_ms)
        {
            delay = minimum_ms;
        }
        if (delay > maximum_ms)
        {
            delay = maximum_ms;
        }
        result.duration_ms += delay;
        if (i == result.pause_index)
        {
            result.duration_ms += result.pause_ms;
        }
        offsets_ms[i] = result.duration_ms;
    }
    return result;
}

static uint32_t motion_timing_random(uint32_t* seed)
{
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

void motion_timing_offsets(
    uint32_t* offsets, uint16_t reports, uint32_t duration_ticks,
    uint32_t min_step_ticks, uint32_t pause_limit_ticks, uint32_t seed)
{
    if (seed == 0U)
    {
        seed = 1U;
    }
    uint16_t weights[MOTION_PATH_STEPS];
    const uint16_t intervals = reports - 1U;
    uint32_t total_weight = 0U;
    const uint16_t pause_index = 1U + motion_timing_random(&seed) % (intervals - 2U);
    const uint16_t speed_a = 4U + motion_timing_random(&seed) % 16U;
    const uint16_t speed_b = 4U + motion_timing_random(&seed) % 16U;
    for (uint16_t i = 0U; i < intervals; i++)
    {
        const uint16_t edge = i < intervals / 2U ? i : intervals - i - 1U;
        const uint16_t slowdown = edge < 12U ? (12U - edge) * 2U : 0U;
        weights[i] = (speed_a * (intervals - i) + speed_b * i) / intervals +
                     slowdown + motion_timing_random(&seed) % 3U;
        total_weight += weights[i];
    }
    const uint32_t extra = duration_ticks - intervals * min_step_ticks;
    uint32_t pause = extra / 8U;
    if (pause > pause_limit_ticks)
    {
        pause = pause_limit_ticks;
    }
    const uint32_t distributed = extra - pause;
    uint32_t weight_so_far = 0U;
    offsets[0] = 0U;
    for (uint16_t i = 0U; i < intervals; i++)
    {
        weight_so_far += weights[i];
        offsets[i + 1U] = (i + 1U) * min_step_ticks +
                          distributed * weight_so_far / total_weight +
                          (i >= pause_index ? pause : 0U);
    }
}
