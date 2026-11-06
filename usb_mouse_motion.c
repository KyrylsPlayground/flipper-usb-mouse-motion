#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include "motion_path.h"
#include "motion_timing.h"
#include "motion_tap.h"

#define SIZE_MIN 50U
#define MOTION_SIZE_MAX 1000U
#define SIZE_STEP 50U
#define INTERVAL_MIN 1U
#define INTERVAL_MAX 99U
#define DOUBLE_TAP_MS 350U

typedef enum
{
    MotionPhaseWait,
    MotionPhaseMove,
    MotionPhaseRelease,
    MotionPhaseClick,
    MotionPhaseRest,
} MotionPhase;

typedef struct
{
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    ViewPort* viewport;
    MotionPath path;
    MotionPoint position;
    MotionArea area;
    bool edit_y;
    uint8_t interval;
    uint8_t button;
    uint8_t pressed_button;
    bool running;
    bool testing;
    MotionTap ok_tap;
    MotionPhase phase;
    uint16_t step;
    uint32_t deadline;
    uint32_t window_end;
    uint32_t motion_start;
    uint32_t motion_duration;
    uint32_t motion_offsets[MOTION_PATH_STEPS];
    uint8_t click_count;
    uint8_t click_index;
    uint8_t burst_button;
    uint32_t click_hold_ticks[3];
    uint32_t click_gap_ticks[2];
    uint16_t point_stride;
    FuriHalUsbHidConfig usb_config;
} MouseMotion;

static void mouse_motion_draw(Canvas* canvas, void* context)
{
    MouseMotion* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "USB Mouse Motion");
    canvas_set_font(canvas, FontSecondary);
    const char* status = "PAUSED";
    if (app->running)
    {
        status = app->testing ? "TEST" : (app->phase == MotionPhaseMove ? "MOVING" : "WAITING");
    }
    char text[32];
    snprintf(text, sizeof(text), "%s  USB:%s", status,
             furi_hal_hid_is_connected() ? "ON" : "OFF");
    canvas_draw_str(canvas, 0, 21, text);
    if (app->edit_y)
    {
        snprintf(text, sizeof(text), "X:%u  Y:[%u]", app->area.width, app->area.height);
    }
    else
    {
        snprintf(text, sizeof(text), "X:[%u]  Y:%u", app->area.width, app->area.height);
    }
    canvas_draw_str(canvas, 0, 32, text);
    snprintf(text, sizeof(text), "Up/Down Interval: %us", app->interval);
    canvas_draw_str(canvas, 0, 42, text);
    snprintf(text, sizeof(text), "OK:%s 2x:test (1-3)", app->button == HID_MOUSE_BTN_LEFT ? "Left" : "Right");
    canvas_draw_str(canvas, 0, 52, text);
    canvas_draw_str(canvas, 0, 63, "Hold <>:axis OK:run/pause");
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
    if (app->pressed_button != 0U)
    {
        furi_hal_hid_mouse_release(app->pressed_button);
        app->pressed_button = 0U;
    }
}

static void mouse_motion_begin_test(MouseMotion* app)
{
    mouse_motion_stop(app);
    if (!furi_hal_hid_is_connected())
    {
        return;
    }
    motion_path_generate_test(&app->path, app->area, app->position);
    app->running = true;
    app->testing = true;
    app->phase = MotionPhaseMove;
    app->step = 0U;
    app->point_stride = 1U;
    app->motion_start = furi_get_tick();
    app->motion_duration = furi_ms_to_ticks(motion_timing_test_offsets(&app->path, app->motion_offsets));
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

static void mouse_motion_begin_window(MouseMotion* app)
{
    motion_path_generate(&app->path, app->area, app->position, furi_hal_random_get());
    const uint32_t interval_ms = app->interval * 1000U;
    const uint32_t window_ticks = furi_ms_to_ticks(interval_ms);
    app->click_count = 1U + furi_hal_random_get() % 3U;
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
    const MotionTiming timing = motion_timing_plan(
        window_ticks,
        click_ticks,
        furi_ms_to_ticks(2U),
        furi_hal_random_get(),
        furi_hal_random_get());
    app->point_stride = timing.point_stride;
    const uint32_t target_ms = (uint64_t)timing.duration_ticks * 1000U /
                               furi_kernel_get_tick_frequency();
    const MotionSpeedTiming speed = motion_timing_speed_offsets(
        &app->path, app->point_stride, target_ms, app->motion_offsets, furi_hal_random_get());
    const uint16_t reports = MOTION_PATH_STEPS / app->point_stride;
    for (uint16_t i = 0U; i < reports; i++)
    {
        app->motion_offsets[i] = furi_ms_to_ticks(app->motion_offsets[i]);
    }
    app->motion_duration = furi_ms_to_ticks(speed.duration_ms);
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
    const MotionPoint previous = app->path.points[app->step];
    const MotionPoint next = app->path.points[app->step + app->point_stride];
    const int16_t dx = next.x - previous.x;
    const int16_t dy = next.y - previous.y;
    if (dx < -127 || dx > 127 || dy < -127 || dy > 127 || !furi_hal_hid_mouse_move((int8_t)dx, (int8_t)dy))
    {
        mouse_motion_stop(app);
        return;
    }
    app->position = next;
    app->step += app->point_stride;
    if (app->step == MOTION_PATH_STEPS)
    {
        if (app->testing)
        {
            mouse_motion_stop(app);
        }
        else
        {
            app->burst_button = app->button;
            mouse_motion_press(app);
        }
    }
    else
    {
        const uint32_t reports_sent = app->step / app->point_stride;
        app->deadline = app->motion_start + app->motion_offsets[reports_sent];
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
            if (app->running)
            {
                mouse_motion_stop(app);
            }
            else if (furi_hal_hid_is_connected())
            {
                app->running = true;
                mouse_motion_begin_window(app);
            }
        }
        else if (input->type == InputTypeShort)
        {
            if (motion_tap_short(&app->ok_tap, furi_get_tick(), furi_ms_to_ticks(DOUBLE_TAP_MS)))
            {
                mouse_motion_begin_test(app);
            }
        }
    }
    else if (input->type == InputTypeLong && (input->key == InputKeyLeft || input->key == InputKeyRight))
    {
        app->edit_y = input->key == InputKeyRight;
    }
    else if (input->type == InputTypeShort || input->type == InputTypeRepeat)
    {
        uint16_t* size = app->edit_y ? &app->area.height : &app->area.width;
        if (input->type == InputTypeShort)
        {
            if (input->key == InputKeyLeft && *size > SIZE_MIN)
            {
                *size -= SIZE_STEP;
            }
            if (input->key == InputKeyRight && *size < MOTION_SIZE_MAX)
            {
                *size += SIZE_STEP;
            }
        }
        if (input->key == InputKeyUp && app->interval < INTERVAL_MAX)
        {
            app->interval++;
        }
        if (input->key == InputKeyDown && app->interval > INTERVAL_MIN)
        {
            app->interval--;
        }
    }
    return true;
}

int32_t usb_mouse_motion_app(void* context)
{
    UNUSED(context);
    MouseMotion* app = calloc(1, sizeof(MouseMotion));
    app->area = (MotionArea)
    {
        200U, 200U
    };
    app->interval = 10U;
    app->button = HID_MOUSE_BTN_RIGHT;
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
                app->button = app->button == HID_MOUSE_BTN_LEFT ? HID_MOUSE_BTN_RIGHT : HID_MOUSE_BTN_LEFT;
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
    view_port_free(app->viewport);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->queue);
    free(app);
    return result;
}
