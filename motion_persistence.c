#include "motion_persistence.h"
#include <furi.h>
#include <flipper_format/flipper_format.h>
#include <storage/storage.h>
#include <string.h>

#define MOTION_SETTINGS_PATH APP_DATA_PATH(".settings")
#define MOTION_SETTINGS_TEMP_PATH APP_DATA_PATH(".settings.tmp")
#define MOTION_SETTINGS_TYPE "USB Mouse Motion Settings"
#define MOTION_SETTINGS_VERSION 1U

typedef enum
{
    MotionFieldWidth,
    MotionFieldHeight,
    MotionFieldInterval,
    MotionFieldMinSpeed,
    MotionFieldMaxSpeed,
    MotionFieldMinClicks,
    MotionFieldMaxClicks,
    MotionFieldWorkMode,
    MotionFieldTestMode,
    MotionFieldCount,
} MotionSettingsField;

static const char* const motion_settings_keys[MotionFieldCount] =
{
    "width", "height", "interval_s", "min_speed", "max_speed",
    "min_clicks", "max_clicks", "work_mode", "test_mode",
};

static bool motion_settings_fields_valid(const uint32_t* values)
{
    return values[MotionFieldWidth] >= 50U && values[MotionFieldWidth] <= 1000U &&
           values[MotionFieldHeight] >= 50U && values[MotionFieldHeight] <= 1000U &&
           values[MotionFieldInterval] >= 1U && values[MotionFieldInterval] <= 99U &&
           values[MotionFieldMinSpeed] >= 50U && values[MotionFieldMaxSpeed] <= 2000U &&
           values[MotionFieldMinSpeed] <= values[MotionFieldMaxSpeed] &&
           values[MotionFieldMinClicks] <= values[MotionFieldMaxClicks] &&
           values[MotionFieldMaxClicks] <= MOTION_CLICK_LIMIT &&
           values[MotionFieldWorkMode] < MotionWorkCount && values[MotionFieldTestMode] < MotionTestCount;
}

static bool motion_settings_load_file(MotionSettings* settings, const char* path)
{
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* type = furi_string_alloc();
    uint32_t version = 0U;
    uint32_t values[MotionFieldCount] =
    {
        0
    };
    bool right_button = true;
    bool loaded = flipper_format_file_open_existing(file, path) &&
                  flipper_format_read_header(file, type, &version) &&
                  strcmp(furi_string_get_cstr(type), MOTION_SETTINGS_TYPE) == 0 &&
                  version == MOTION_SETTINGS_VERSION;
    for (uint8_t i = 0U; loaded && i < MotionFieldCount; i++)
    {
        loaded = flipper_format_read_uint32(file, motion_settings_keys[i], &values[i], 1U);
    }
    if (loaded)
    {
        loaded = flipper_format_read_bool(file, "right_button", &right_button, 1U) && motion_settings_fields_valid(values);
    }
    flipper_format_free(file);
    furi_string_free(type);
    furi_record_close(RECORD_STORAGE);
    if (!loaded)
    {
        return false;
    }
    MotionSettings restored;
    motion_settings_init(&restored);
    restored.area.width = values[MotionFieldWidth];
    restored.area.height = values[MotionFieldHeight];
    restored.interval = values[MotionFieldInterval];
    restored.min_speed = values[MotionFieldMinSpeed];
    restored.max_speed = values[MotionFieldMaxSpeed];
    restored.min_clicks = values[MotionFieldMinClicks];
    restored.max_clicks = values[MotionFieldMaxClicks];
    restored.work_mode = values[MotionFieldWorkMode];
    restored.test_mode = values[MotionFieldTestMode];
    restored.right_button = right_button;
    *settings = restored;
    return true;
}

bool motion_settings_load(MotionSettings* settings)
{
    return motion_settings_load_file(settings, MOTION_SETTINGS_PATH) || motion_settings_load_file(settings, MOTION_SETTINGS_TEMP_PATH);
}

bool motion_settings_save(const MotionSettings* settings)
{
    const uint32_t values[MotionFieldCount] =
    {
        settings->area.width, settings->area.height, settings->interval,
        settings->min_speed, settings->max_speed, settings->min_clicks,
        settings->max_clicks, settings->work_mode, settings->test_mode,
    };
    if (!motion_settings_fields_valid(values))
    {
        return false;
    }
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    bool saved = storage_simply_mkdir(storage, APP_DATA_PATH("")) &&
                 flipper_format_file_open_always(file, MOTION_SETTINGS_TEMP_PATH) &&
                 flipper_format_write_header_cstr(file, MOTION_SETTINGS_TYPE, MOTION_SETTINGS_VERSION);
    for (uint8_t i = 0U; saved && i < MotionFieldCount; i++)
    {
        saved = flipper_format_write_uint32(file, motion_settings_keys[i], &values[i], 1U);
    }
    if (saved)
    {
        saved = flipper_format_write_bool(file, "right_button", &settings->right_button, 1U);
    }
    const bool complete_temp = saved;
    flipper_format_free(file);
    if (saved)
    {
        saved = storage_common_rename(storage, MOTION_SETTINGS_TEMP_PATH, MOTION_SETTINGS_PATH) == FSE_OK;
    }
    if (!saved)
    {
        // Firmware rename is implemented as remove/copy. Keep a complete temp
        // file for load fallback if replacing the destination failed.
        if (!complete_temp)
        {
            storage_simply_remove(storage, MOTION_SETTINGS_TEMP_PATH);
        }
        FURI_LOG_W("MouseMotion", "Settings save failed");
    }
    furi_record_close(RECORD_STORAGE);
    return saved;
}
