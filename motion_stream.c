#include "motion_stream.h"

static int16_t motion_stream_coordinate(int16_t start, int16_t target, uint32_t elapsed, uint32_t duration)
{
    const int64_t numerator = ((int64_t)target - start) * elapsed;
    const int64_t offset = numerator >= 0 ? (numerator + duration / 2U) / duration : -((-numerator + duration / 2U) / duration);
    return (int16_t)(start + offset);
}

void motion_stream_begin(MotionStream* stream, MotionPoint start, MotionPoint target, uint32_t duration, uint32_t frame_ticks, uint32_t minimum_ticks)
{
    if (duration < minimum_ticks)
    {
        duration = minimum_ticks;
    }
    uint32_t frames = (duration + frame_ticks - 1U) / frame_ticks;
    const uint32_t maximum_frames = duration / minimum_ticks;
    if (frames > maximum_frames)
    {
        frames = maximum_frames;
    }
    *stream = (MotionStream)
    {
        .start = start,
        .target = target,
        .duration = duration,
        .frames = frames,
    };
}

bool motion_stream_next(MotionStream* stream, MotionPoint* point, uint32_t* delay)
{
    if (stream->frame >= stream->frames)
    {
        return false;
    }
    stream->frame++;
    const uint32_t elapsed = (uint64_t)stream->duration * stream->frame / stream->frames;
    *delay = elapsed - stream->elapsed;
    stream->elapsed = elapsed;
    // Interpolate from the immutable start, not from rounded intermediate points.
    *point = (MotionPoint)
    {
        motion_stream_coordinate(stream->start.x, stream->target.x, elapsed, stream->duration),
        motion_stream_coordinate(stream->start.y, stream->target.y, elapsed, stream->duration),
    };
    return true;
}
