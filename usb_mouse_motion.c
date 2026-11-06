#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include "motion_path.h"
#include "motion_timing.h"

#define RADIUS_MIN 25U
#define RADIUS_MAX 500U
#define RADIUS_STEP 25U
#define INTERVAL_MIN 1U
#define INTERVAL_MAX 30U

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
    uint16_t radius;
    uint8_t interval;
    uint8_t button;
    uint8_t pressed_button;
    bool running;
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
        status = app->phase == MotionPhaseMove ? "MOVING" : "WAITING";
    }
    char text[32];
    snprintf(text, sizeof(text), "%s  USB:%s", status,
             furi_hal_hid_is_connected() ? "ON" : "OFF");
    canvas_draw_str(canvas, 0, 21, text);
    snprintf(text, sizeof(text), "< > Radius: %u", app->radius);
    canvas_draw_str(canvas, 0, 32, text);
    snprintf(text, sizeof(text), "Up/Down Interval: %us", app->interval);
    canvas_draw_str(canvas, 0, 42, text);
    snprintf(text, sizeof(text), "OK: %s (1-3 clicks)", app->button == HID_MOUSE_BTN_LEFT ? "Left" : "Right");
    canvas_draw_str(canvas, 0, 52, text);
    canvas_draw_str(canvas, 0, 63, "Hold OK:run/pause Back:exit");
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
    if (app->pressed_button != 0U)
    {
        furi_hal_hid_mouse_release(app->pressed_button);
        app->pressed_button = 0U;
    }
}

static uint32_t mouse_motion_remaining(uint32_t deadline, uint32_t now)
{
    return (int32_t)(deadline - now) > 0 ? deadline - now : 0U;
}

static void mouse_motion_begin_window(MouseMotion* app)
{
    motion_path_generate(&app->path, app->radius, furi_hal_random_get());
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
    const uint32_t now = furi_get_tick();
    app->window_end = now + window_ticks;
    app->motion_start = now + timing.offset_ticks;
    app->motion_duration = timing.duration_ticks;
    app->point_stride = timing.point_stride;
    motion_timing_offsets(
        app->motion_offsets, MOTION_PATH_STEPS / app->point_stride,
        app->motion_duration, furi_ms_to_ticks(2U), furi_ms_to_ticks(120U),
        furi_hal_random_get());
    app->deadline = app->motion_start;
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
    app->step += app->point_stride;
    if (app->step == MOTION_PATH_STEPS)
    {
        app->burst_button = app->button;
        mouse_motion_press(app);
    }
    else
    {
        const uint32_t reports_sent = app->step / app->point_stride;
        app->deadline = app->motion_start +
                        app->motion_offsets[reports_sent];
    }
}

static bool mouse_motion_handle_input(MouseMotion* app, const InputEvent* input)
{
    if (input->key == InputKeyBack && input->type == InputTypeShort)
    {
        return false;
    }
    if (input->key == InputKeyOk)
    {
        if (input->type == InputTypeLong)
        {
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
            app->button = app->button == HID_MOUSE_BTN_LEFT ? HID_MOUSE_BTN_RIGHT : HID_MOUSE_BTN_LEFT;
        }
    }
    else if (input->type == InputTypeShort || input->type == InputTypeRepeat)
    {
        if (input->key == InputKeyLeft && app->radius > RADIUS_MIN)
        {
            app->radius -= RADIUS_STEP;
        }
        if (input->key == InputKeyRight && app->radius < RADIUS_MAX)
        {
            app->radius += RADIUS_STEP;
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
    app->radius = 100U;
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
