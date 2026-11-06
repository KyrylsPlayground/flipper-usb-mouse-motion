#include "motion_settings.h"

static uint16_t motion_settings_change(uint16_t value, int delta, uint16_t min, uint16_t max)
{
    int result = value + delta;
    if (result < min)
    {
        result = min;
    }
    if (result > max)
    {
        result = max;
    }
    return (uint16_t)result;
}

void motion_settings_init(MotionSettings* settings)
{
    *settings = (MotionSettings)
    {
        .area =
        {
            200U, 200U
        },
        .min_speed = 314U,
        .max_speed = 700U,
        .interval = 10U,
        .min_clicks = 1U,
        .max_clicks = 3U,
        .right_button = true,
        .test_mode = MotionTestStandard,
        .work_mode = MotionWorkNatural,
        .trajectory = MotionTrajectoryRandom,
    };
}

uint16_t motion_settings_work_speed(const MotionSettings* settings, uint32_t random)
{
    switch (settings->work_mode)
    {
        case MotionWorkRandom:
            return settings->min_speed + random % (settings->max_speed - settings->min_speed + 1U);
        case MotionWorkMinimum:
            return settings->min_speed;
        case MotionWorkMaximum:
            return settings->max_speed;
        default:
            return 0U;
    }
}

void motion_settings_select(MotionSettings* settings, int direction)
{
    settings->selected = (MotionItem)motion_settings_change(
        settings->selected, direction, 0U, MotionItemCount - 1U);
    if (settings->selected < settings->first_visible)
    {
        settings->first_visible = settings->selected;
    }
    if (settings->selected >= settings->first_visible + 4U)
    {
        settings->first_visible = settings->selected - 3U;
    }
}

void motion_settings_adjust(MotionSettings* settings, int direction, bool repeat)
{
    switch (settings->selected)
    {
        case MotionItemWidth:
            settings->area.width = motion_settings_change(settings->area.width, direction * (repeat ? 25 : 1), 50U, 1000U);
            break;
        case MotionItemHeight:
            settings->area.height = motion_settings_change(settings->area.height, direction * (repeat ? 25 : 1), 50U, 1000U);
            break;
        case MotionItemInterval:
            settings->interval = motion_settings_change(settings->interval, direction, 1U, 99U);
            break;
        case MotionItemTrajectory:
            settings->trajectory = (MotionTrajectory)((settings->trajectory + MotionTrajectoryCount + direction) % MotionTrajectoryCount);
            break;
        case MotionItemMinSpeed:
            settings->min_speed = motion_settings_change(
                settings->min_speed, direction * (repeat ? 25 : 1), 50U, settings->max_speed);
            break;
        case MotionItemMaxSpeed:
            settings->max_speed = motion_settings_change(
                settings->max_speed, direction * (repeat ? 25 : 1), settings->min_speed, 2000U);
            break;
        case MotionItemButton:
            settings->right_button = direction > 0;
            break;
        case MotionItemWorkMode:
            settings->work_mode = (MotionWorkMode)((settings->work_mode + MotionWorkCount + direction) % MotionWorkCount);
            break;
        case MotionItemMinClicks:
            settings->min_clicks = motion_settings_change(
                settings->min_clicks, direction, 0U, settings->max_clicks);
            break;
        case MotionItemMaxClicks:
            settings->max_clicks = motion_settings_change(
                settings->max_clicks, direction, settings->min_clicks, MOTION_CLICK_LIMIT);
            break;
        case MotionItemTestMode:
            settings->test_mode = (MotionTestMode)((settings->test_mode + MotionTestCount + direction) % MotionTestCount);
            break;
        default:
            break;
    }
}
