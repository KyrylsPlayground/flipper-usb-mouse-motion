#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool pending;
    uint32_t deadline;
} MotionTap;

static inline bool motion_tap_expired(MotionTap* tap, uint32_t now)
{
    if (tap->pending && (int32_t)(tap->deadline - now) <= 0)
    {
        tap->pending = false;
        return true;
    }
    return false;
}

// Call expired first: a second tap inside the window consumes the pending single tap.
static inline bool motion_tap_short(MotionTap* tap, uint32_t now, uint32_t window_ticks)
{
    if (tap->pending)
    {
        tap->pending = false;
        return true;
    }
    tap->pending = true;
    tap->deadline = now + window_ticks;
    return false;
}
