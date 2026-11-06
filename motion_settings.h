#pragma once

#include "motion_path.h"
#include <stdbool.h>

#define MOTION_CLICK_LIMIT 10U

typedef enum
{
    MotionItemWidth,
    MotionItemHeight,
    MotionItemInterval,
    MotionItemMinSpeed,
    MotionItemMaxSpeed,
    MotionItemButton,
    MotionItemMinClicks,
    MotionItemMaxClicks,
    MotionItemTestMode,
    MotionItemCount,
} MotionItem;

typedef enum
{
    MotionTestMinimum,
    MotionTestMaximum,
    MotionTestStandard,
    MotionTestCount,
} MotionTestMode;

typedef struct
{
    MotionArea area;
    uint16_t min_speed;
    uint16_t max_speed;
    uint8_t interval;
    uint8_t min_clicks;
    uint8_t max_clicks;
    bool right_button;
    MotionTestMode test_mode;
    MotionItem selected;
    uint8_t first_visible;
} MotionSettings;

void motion_settings_init(MotionSettings* settings);
void motion_settings_select(MotionSettings* settings, int direction);
void motion_settings_adjust(MotionSettings* settings, int direction, bool repeat);
