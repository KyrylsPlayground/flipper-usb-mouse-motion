#include "motion_timing.h"

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
