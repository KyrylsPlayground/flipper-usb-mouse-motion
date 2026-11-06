#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <math.h>
#include "motion_path.h"
#include "motion_timing.h"
#include "motion_tap.h"
#include "motion_settings.h"
#include "motion_persistence.h"

#define DOUBLE_TAP_MS 350U
#define SETTINGS_SAVE_DELAY_MS 500U
#define SETTINGS_SAVE_RETRY_MS 2000U

typedef enum
{
    MotionPhaseWait,
    MotionPhaseMove,
    MotionPhaseRelease,
    MotionPhaseClick,
    MotionPhaseRest,
    MotionPhaseCenter,
} MotionPhase;

typedef enum
{
    MotionCenterPause,
    MotionCenterRun,
    MotionCenterTest,
} MotionCenterAction;

typedef struct
{
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    ViewPort* viewport;
    MotionPath path;
    MotionPoint position;
    MotionArea movement_area;
    uint16_t movement_speed_limit;
    uint16_t movement_constant_speed;
    MotionSettings settings;
    bool settings_dirty;
    bool settings_save_error;
    uint32_t settings_save_deadline;
    uint8_t pressed_button;
    bool running;
    bool testing;
    MotionTap ok_tap;
    MotionPhase phase;
    MotionCenterAction after_center;
    uint16_t center_speed;
    uint16_t step;
    uint32_t deadline;
    uint32_t window_end;
    uint32_t motion_start;
    uint32_t motion_duration;
    uint32_t motion_offsets[MOTION_PATH_STEPS];
    uint8_t click_count;
    uint8_t click_index;
    uint8_t burst_button;
    uint32_t click_hold_ticks[MOTION_CLICK_LIMIT];
    uint32_t click_gap_ticks[MOTION_CLICK_LIMIT - 1U];
    uint16_t point_stride;
    FuriHalUsbHidConfig usb_config;
} MouseMotion;

static void mouse_motion_draw(Canvas* canvas, void* context)
{
    MouseMotion* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
#ifdef FAP_VERSION
    canvas_draw_str(canvas, 0, 10, "Mouse Motion " FAP_VERSION);
#else
    canvas_draw_str(canvas, 0, 10, "Mouse Motion");
#endif
    canvas_set_font(canvas, FontSecondary);
    const char* status = app->position.x != 0 || app->position.y != 0 ? "OFF CENTER" : "PAUSED";
    if (app->settings_save_error)
    {
        status = "SAVE ERROR";
    }
    if (app->running)
    {
        status = app->phase == MotionPhaseCenter ? "CENTERING" : (app->testing ? "TEST" : (app->phase == MotionPhaseMove ? "MOVING" : "WAITING"));
    }
    char text[32];
    snprintf(text, sizeof(text), "%s %d,%d%s", status, app->position.x, app->position.y,
             furi_hal_hid_is_connected() ? "" : " USB-");
    canvas_draw_str(canvas, 0, 21, text);
    for (uint8_t row = 0U; row < 4U; row++)
    {
        const MotionItem item = app->settings.first_visible + row;
        if (item >= MotionItemCount)
        {
            break;
        }
        switch (item)
        {
            case MotionItemWidth:
                snprintf(text, sizeof(text), "X width: %u", app->settings.area.width);
                break;
            case MotionItemHeight:
                snprintf(text, sizeof(text), "Y height: %u", app->settings.area.height);
                break;
            case MotionItemInterval:
                snprintf(text, sizeof(text), "Interval: %us", app->settings.interval);
                break;
            case MotionItemMinSpeed:
                snprintf(text, sizeof(text), "Min speed: %u", app->settings.min_speed);
                break;
            case MotionItemMaxSpeed:
                snprintf(text, sizeof(text), "Max speed: %u", app->settings.max_speed);
                break;
            case MotionItemButton:
                snprintf(text, sizeof(text), "Button: %s", app->settings.right_button ? "Right" : "Left");
                break;
            case MotionItemWorkMode:
            {
                const char* label = "Natural";
                if (app->settings.work_mode == MotionWorkRandom)
                {
                    label = "Random";
                }
                if (app->settings.work_mode == MotionWorkMinimum)
                {
                    label = "Min speed";
                }
                if (app->settings.work_mode == MotionWorkMaximum)
                {
                    label = "Max speed";
                }
                snprintf(text, sizeof(text), "Run speed: %s", label);
                break;
            }
            case MotionItemTestMode:
            {
                const char* label = "Standard";
                if (app->settings.test_mode == MotionTestMinimum)
                {
                    label = "Min speed";
                }
                if (app->settings.test_mode == MotionTestMaximum)
                {
                    label = "Max speed";
                }
                snprintf(text, sizeof(text), "Test: %s", label);
                break;
            }
            case MotionItemMinClicks:
                snprintf(text, sizeof(text), "Min clicks: %u", app->settings.min_clicks);
                break;
            case MotionItemMaxClicks:
                snprintf(text, sizeof(text), "Max clicks: %u", app->settings.max_clicks);
                break;
            default:
                break;
        }
        const uint8_t top = 23U + row * 10U;
        if (item == app->settings.selected)
        {
            canvas_draw_box(canvas, 0, top, 123, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 2, top + 8U, text);
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_line(canvas, 126, 23, 126, 62);
    canvas_draw_box(canvas, 125, 23U + app->settings.first_visible * 40U / MotionItemCount,
                    3, 4U * 40U / MotionItemCount);
    furi_mutex_release(app->mutex);
}

static void mouse_motion_input(InputEvent* input, void* context)
{
    MouseMotion* app = context;
    furi_message_queue_put(app->queue, input, 0U);
}

static void mouse_motion_stop(MouseMotion* app)
{
    app->running = false;
    app->testing = false;
    app->after_center = MotionCenterPause;
    if (app->pressed_button != 0U)
    {
        furi_hal_hid_mouse_release(app->pressed_button);
        app->pressed_button = 0U;
    }
}

static void mouse_motion_start_test(MouseMotion* app)
{
    mouse_motion_stop(app);
    if (!furi_hal_hid_is_connected())
    {
        return;
    }
    app->movement_area = app->settings.area;
    motion_path_generate_test(&app->path, app->movement_area, app->position);
    app->running = true;
    app->testing = true;
    app->phase = MotionPhaseMove;
    app->step = 0U;
    app->point_stride = 1U;
    app->motion_start = furi_get_tick();
    uint16_t speed = 0U;
    if (app->settings.test_mode == MotionTestMinimum)
    {
        speed = app->settings.min_speed;
    }
    if (app->settings.test_mode == MotionTestMaximum)
    {
        speed = app->settings.max_speed;
    }
    app->movement_speed_limit = speed != 0U ? speed : app->settings.max_speed;
    app->movement_constant_speed = speed;
    app->motion_duration = furi_ms_to_ticks(motion_timing_test_offsets(&app->path, app->motion_offsets, speed));
    for (uint16_t i = 0U; i < MOTION_PATH_STEPS; i++)
    {
        app->motion_offsets[i] = furi_ms_to_ticks(app->motion_offsets[i]);
    }
    app->deadline = app->motion_start + app->motion_offsets[0];
}

static uint32_t mouse_motion_remaining(uint32_t deadline, uint32_t now)
{
    return (int32_t)(deadline - now) > 0 ? deadline - now : 0U;
}

static void mouse_motion_save_settings(MouseMotion* app)
{
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const bool save_pending = app->settings_dirty && !app->running;
    const MotionSettings settings = app->settings;
    furi_mutex_release(app->mutex);
    if (!save_pending)
    {
        return;
    }
    const bool saved = motion_settings_save(&settings);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->settings_dirty = !saved;
    app->settings_save_error = !saved;
    app->settings_save_deadline = furi_get_tick() + furi_ms_to_ticks(SETTINGS_SAVE_RETRY_MS);
    furi_mutex_release(app->mutex);
}

static void mouse_motion_begin_window(MouseMotion* app)
{
    app->movement_area = app->settings.area;
    app->movement_constant_speed = motion_settings_work_speed(&app->settings, furi_hal_random_get());
    app->movement_speed_limit = app->movement_constant_speed != 0U ? app->movement_constant_speed : app->settings.max_speed;
    motion_path_generate(&app->path, app->movement_area, app->position, furi_hal_random_get());
    const uint32_t interval_ms = app->settings.interval * 1000U;
    const uint32_t window_ticks = furi_ms_to_ticks(interval_ms);
    app->click_count = app->settings.min_clicks + furi_hal_random_get() %
                                                      (app->settings.max_clicks - app->settings.min_clicks + 1U);
    app->click_index = 0U;
    uint32_t click_ticks = furi_ms_to_ticks(40U);
    for (uint8_t i = 0U; i < app->click_count; i++)
    {
        app->click_hold_ticks[i] = furi_ms_to_ticks(25U + furi_hal_random_get() % 36U);
        click_ticks += app->click_hold_ticks[i];
        if (i + 1U < app->click_count)
        {
            app->click_gap_ticks[i] = furi_ms_to_ticks(50U + furi_hal_random_get() % 41U);
            click_ticks += app->click_gap_ticks[i];
        }
    }
    const uint32_t minimum_planning_ticks = click_ticks + furi_ms_to_ticks(512U);
    const uint32_t planning_ticks = window_ticks > minimum_planning_ticks
                                        ? window_ticks
                                        : minimum_planning_ticks;
    const MotionTiming timing = motion_timing_plan(
        planning_ticks,
        click_ticks,
        furi_ms_to_ticks(2U),
        furi_hal_random_get(),
        furi_hal_random_get());
    app->point_stride = timing.point_stride;
    const uint32_t target_ms = (uint64_t)timing.duration_ticks * 1000U /
                               furi_kernel_get_tick_frequency();
    uint32_t duration_ms;
    if (app->movement_constant_speed != 0U)
    {
        duration_ms = motion_timing_uniform_offsets(&app->path, app->point_stride, app->motion_offsets, app->movement_constant_speed);
    }
    else
    {
        const MotionSpeedTiming speed = motion_timing_speed_offsets(
            &app->path, app->point_stride, target_ms, app->motion_offsets, furi_hal_random_get(),
            app->settings.min_speed, app->settings.max_speed);
        duration_ms = speed.duration_ms;
    }
    const uint16_t reports = MOTION_PATH_STEPS / app->point_stride;
    for (uint16_t i = 0U; i < reports; i++)
    {
        app->motion_offsets[i] = furi_ms_to_ticks(app->motion_offsets[i]);
    }
    app->motion_duration = furi_ms_to_ticks(duration_ms);
    const uint32_t busy_ticks = app->motion_duration + click_ticks;
    const uint32_t cycle_ticks = busy_ticks > window_ticks ? busy_ticks : window_ticks;
    const uint32_t latest_start = cycle_ticks - busy_ticks;
    const uint32_t offset = timing.offset_ticks <= latest_start
                                ? timing.offset_ticks
                                : furi_hal_random_get() % (latest_start + 1U);
    const uint32_t now = furi_get_tick();
    app->window_end = now + cycle_ticks;
    app->motion_start = now + offset;
    app->deadline = app->motion_start + app->motion_offsets[0];
    app->phase = MotionPhaseWait;
    app->step = 0U;
}

static uint32_t mouse_motion_travel_ticks(MotionPoint from, MotionPoint to, uint16_t speed)
{
    const float distance = hypotf((float)(to.x - from.x), (float)(to.y - from.y));
    uint32_t delay_ms = (uint32_t)ceilf(distance * 1000.0f / speed);
    if (delay_ms < 2U)
    {
        delay_ms = 2U;
    }
    return furi_ms_to_ticks(delay_ms);
}

static void mouse_motion_finish_center(MouseMotion* app)
{
    if (app->after_center == MotionCenterTest)
    {
        mouse_motion_start_test(app);
    }
    else if (app->after_center == MotionCenterRun)
    {
        app->running = true;
        mouse_motion_begin_window(app);
    }
    else
    {
        mouse_motion_stop(app);
    }
}

static void mouse_motion_request_center(MouseMotion* app, MotionCenterAction action)
{
    mouse_motion_stop(app);
    if (!furi_hal_hid_is_connected())
    {
        return;
    }
    app->after_center = action;
    if (app->position.x == 0 && app->position.y == 0)
    {
        mouse_motion_finish_center(app);
        return;
    }
    app->running = true;
    app->phase = MotionPhaseCenter;
    app->center_speed = app->movement_constant_speed != 0U ? app->movement_constant_speed : app->settings.min_speed;
    if (action == MotionCenterTest)
    {
        app->center_speed = app->settings.test_mode == MotionTestMaximum ? app->settings.max_speed : app->settings.min_speed;
    }
    const MotionPoint next = motion_path_return_step(app->position);
    app->deadline = furi_get_tick() + mouse_motion_travel_ticks(app->position, next, app->center_speed);
}

static void mouse_motion_center_tick(MouseMotion* app)
{
    const MotionPoint next = motion_path_return_step(app->position);
    const int8_t dx = next.x - app->position.x;
    const int8_t dy = next.y - app->position.y;
    if (!furi_hal_hid_mouse_move(dx, dy))
    {
        mouse_motion_stop(app);
        return;
    }
    app->position = next;
    if (next.x == 0 && next.y == 0)
    {
        mouse_motion_finish_center(app);
        return;
    }
    const MotionPoint following = motion_path_return_step(next);
    app->deadline = furi_get_tick() + mouse_motion_travel_ticks(next, following, app->center_speed);
}

static void mouse_motion_begin_test(MouseMotion* app)
{
    mouse_motion_request_center(app, MotionCenterTest);
}

static void mouse_motion_press(MouseMotion* app)
{
    app->pressed_button = app->burst_button;
    if (!furi_hal_hid_mouse_press(app->pressed_button))
    {
        mouse_motion_stop(app);
        return;
    }
    app->phase = MotionPhaseRelease;
    app->deadline = furi_get_tick() + app->click_hold_ticks[app->click_index];
}

static void mouse_motion_tick(MouseMotion* app)
{
    if (!furi_hal_hid_is_connected())
    {
        mouse_motion_stop(app);
        return;
    }
    if (app->phase == MotionPhaseCenter)
    {
        mouse_motion_center_tick(app);
        return;
    }
    if (app->phase == MotionPhaseRest)
    {
        mouse_motion_begin_window(app);
        return;
    }
    if (app->phase == MotionPhaseClick)
    {
        mouse_motion_press(app);
        return;
    }
    if (app->phase == MotionPhaseRelease)
    {
        if (!furi_hal_hid_mouse_release(app->pressed_button))
        {
            mouse_motion_stop(app);
            return;
        }
        app->pressed_button = 0U;
        app->click_index++;
        if (app->click_index < app->click_count)
        {
            app->phase = MotionPhaseClick;
            app->deadline = furi_get_tick() + app->click_gap_ticks[app->click_index - 1U];
        }
        else
        {
            app->phase = MotionPhaseRest;
            app->deadline = app->window_end;
        }
        return;
    }

    app->phase = MotionPhaseMove;
    const MotionPoint target = motion_path_clamp(app->movement_area, app->path.points[app->step + app->point_stride]);
    const MotionPoint next = motion_path_advance(app->movement_area, app->position, target);
    const int16_t dx = next.x - app->position.x;
    const int16_t dy = next.y - app->position.y;
    if (!furi_hal_hid_mouse_move((int8_t)dx, (int8_t)dy))
    {
        mouse_motion_stop(app);
        return;
    }
    app->position = next;
    if (next.x != target.x || next.y != target.y)
    {
        const MotionPoint following = motion_path_advance(app->movement_area, next, target);
        app->deadline = furi_get_tick() + mouse_motion_travel_ticks(next, following, app->movement_speed_limit);
        return;
    }
    app->step += app->point_stride;
    if (app->step == MOTION_PATH_STEPS)
    {
        if (app->testing)
        {
            mouse_motion_stop(app);
        }
        else if (app->click_count == 0U)
        {
            app->phase = MotionPhaseRest;
            app->deadline = app->window_end;
        }
        else
        {
            app->burst_button = app->settings.right_button ? HID_MOUSE_BTN_RIGHT : HID_MOUSE_BTN_LEFT;
            mouse_motion_press(app);
        }
    }
    else
    {
        const uint32_t reports_sent = app->step / app->point_stride;
        const uint32_t delay = app->motion_offsets[reports_sent] - app->motion_offsets[reports_sent - 1U];
        app->deadline = furi_get_tick() + delay;
    }
}

static void mouse_motion_toggle(MouseMotion* app)
{
    if (app->running)
    {
        mouse_motion_request_center(app, MotionCenterPause);
    }
    else if (furi_hal_hid_is_connected())
    {
        mouse_motion_request_center(app, MotionCenterRun);
    }
}

static bool mouse_motion_handle_input(MouseMotion* app, const InputEvent* input)
{
    if (input->key == InputKeyBack && input->type == InputTypeShort)
    {
        app->ok_tap.pending = false;
        return false;
    }
    if (input->key == InputKeyOk)
    {
        if (input->type == InputTypeLong)
        {
            app->ok_tap.pending = false;
        }
        else if (input->type == InputTypeShort)
        {
            if (motion_tap_short(&app->ok_tap, furi_get_tick(), furi_ms_to_ticks(DOUBLE_TAP_MS)))
            {
                mouse_motion_begin_test(app);
            }
        }
    }
    else if (input->type == InputTypeShort || input->type == InputTypeRepeat)
    {
        if (input->key == InputKeyUp)
        {
            motion_settings_select(&app->settings, -1);
        }
        if (input->key == InputKeyDown)
        {
            motion_settings_select(&app->settings, 1);
        }
        if (input->key == InputKeyLeft || input->key == InputKeyRight)
        {
            motion_settings_adjust(&app->settings, input->key == InputKeyRight ? 1 : -1,
                                   input->type == InputTypeRepeat);
            app->settings_dirty = true;
            app->settings_save_deadline = furi_get_tick() + furi_ms_to_ticks(SETTINGS_SAVE_DELAY_MS);
        }
    }
    return true;
}

int32_t usb_mouse_motion_app(void* context)
{
    UNUSED(context);
    MouseMotion* app = calloc(1, sizeof(MouseMotion));
    motion_settings_init(&app->settings);
    motion_settings_load(&app->settings);
    app->queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->viewport = view_port_alloc();
    app->usb_config = (FuriHalUsbHidConfig)
    {
        .vid = 0x046D,
        .pid = 0xC52B,
        .manuf = "Logitech",
        .product = "USB Receiver",
    };
    FuriHalUsbInterface* previous_usb = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    const bool usb_ready = furi_hal_usb_set_config(&usb_hid, &app->usb_config);
    int32_t result = -1;
    if (usb_ready)
    {
        Gui* gui = furi_record_open(RECORD_GUI);
        view_port_draw_callback_set(app->viewport, mouse_motion_draw, app);
        view_port_input_callback_set(app->viewport, mouse_motion_input, app);
        gui_add_view_port(gui, app->viewport, GuiLayerFullscreen);
        bool keep_running = true;
        while (keep_running)
        {
            InputEvent input;
            uint32_t timeout = furi_ms_to_ticks(250U);
            if (app->settings_dirty && !app->running)
            {
                const uint32_t remaining = mouse_motion_remaining(app->settings_save_deadline, furi_get_tick());
                if (remaining < timeout)
                {
                    timeout = remaining;
                }
            }
            if (app->ok_tap.pending)
            {
                const uint32_t remaining = mouse_motion_remaining(app->ok_tap.deadline, furi_get_tick());
                if (remaining < timeout)
                {
                    timeout = remaining;
                }
            }
            if (app->running)
            {
                const uint32_t remaining = mouse_motion_remaining(app->deadline, furi_get_tick());
                if (remaining < timeout)
                {
                    timeout = remaining;
                }
            }
            const FuriStatus status = furi_message_queue_get(app->queue, &input, timeout);
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if (motion_tap_expired(&app->ok_tap, furi_get_tick()))
            {
                mouse_motion_toggle(app);
            }
            if (status == FuriStatusOk)
            {
                keep_running = mouse_motion_handle_input(app, &input);
            }
            if (keep_running && app->running && mouse_motion_remaining(app->deadline, furi_get_tick()) == 0U)
            {
                mouse_motion_tick(app);
            }
            furi_mutex_release(app->mutex);
            if (app->settings_dirty && !app->running && mouse_motion_remaining(app->settings_save_deadline, furi_get_tick()) == 0U)
            {
                mouse_motion_save_settings(app);
            }
            view_port_update(app->viewport);
        }
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        mouse_motion_stop(app);
        furi_mutex_release(app->mutex);
        view_port_enabled_set(app->viewport, false);
        gui_remove_view_port(gui, app->viewport);
        furi_record_close(RECORD_GUI);
        furi_hal_usb_set_config(previous_usb, NULL);
        result = 0;
    }
    mouse_motion_save_settings(app);
    view_port_free(app->viewport);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->queue);
    free(app);
    return result;
}
