# USB Mouse Motion

A Flipper Zero USB HID application with smooth mouse movement along random, circular or rectangular paths, configurable click bursts, and a rectangular boundary test.

Current version: **1.9.2**. Settings are restored from the SD card. Starts paused.

## Features

- Independent X/Y dimensions around a fixed center, with tracked offsets to avoid cumulative endpoint drift.
- Random spline paths with endpoint variation, or closed circle/rectangle paths.
- Interpolated 2–4 ms HID microsteps with exact segment endpoints and event-driven display updates.
- Random start time within each cycle and a configurable interval up to 99 seconds.
- Adjustable minimum and maximum moving speeds.
- Configurable left/right click bursts, including movement-only operation.
- Boundary tests at minimum speed, maximum speed, or a fixed 2-second duration.
- Scrolling settings list and a custom mouse-with-sparkle icon.

## Controls

| Button | Action |
| --- | --- |
| Up / Down | Select a setting; scrolls automatically |
| Left / Right | Change the selected value |
| Hold Left / Right | Repeat value changes; dimensions and speeds change in steps of 25 |
| Single OK | Start / pause, after a 350 ms double-tap detection window |
| Double OK | Run the boundary test, then return to pause |
| Back | Exit and restore the previous USB configuration |

Holding OK is not required. Pause releases the pressed button, returns to the tracked center, then becomes fully paused. In constant work-speed modes the return uses the captured speed; Natural uses the selected minimum. Test pre-centering uses its selected minimum/maximum speed (minimum for Standard). The UI shows CENTERING during that return. The test interrupts normal operation, first returns to the same center, then traces the rectangular perimeter without clicks and finishes centered and paused. Its Standard 2-second duration excludes the preceding centering motion.

## Settings

| Setting | Range / options | Default |
| --- | --- | --- |
| X width | 50–1000 HID counts, step 1; hold-repeat 25 | 200 |
| Y height | 50–1000 HID counts, step 1; hold-repeat 25 | 200 |
| Path | Random / Circle / Rectangle | Random |
| Interval | 1–99 seconds | 10 seconds |
| Minimum speed | 50–2000 HID counts/second | 314 |
| Maximum speed | 50–2000 HID counts/second | 700 |
| Run speed | Natural / Random / Min speed / Max speed | Natural |
| Mouse button | Left / Right | Right |
| Minimum clicks | 0–10 | 1 |
| Maximum clicks | 0–10 | 3 |
| Test mode | Min speed / Max speed / Standard | Standard |

Minimum values cannot exceed their corresponding maximum values. A short Left/Right changes a speed by 1. Set both click counts to **0** for movement only. Each cycle randomly chooses its click count within the selected range; zero skips clicking. Presses last 25–60 ms with 50–90 ms gaps, which the host may interpret as double/triple clicks.

X/Y are the **full width and height**, not radii. For example, X=400 and Y=200 gives relative bounds of ±200 horizontally and ±100 vertically.

Odd dimensions use integer bounds that retain the exact selected size: X=501 gives -250..+251. The extra HID count is on the positive side.

Random paths vary their endpoint within fixed bounds. Circle and Rectangle return exactly to the cycle starting offset without random endpoint displacement, with no entry/exit segments.

- **Circle** starts on its circumference at the current pointer offset. Center orientation and traversal direction are randomized. The requested diameter is the smaller X/Y dimension; the radius is reduced only as needed to keep the whole circle inside the fixed area. At an exact area corner no nonzero circle can fit through that point, so the path degenerates without crossing the bounds.
- **Rectangle** starts at one corner, with the diagonally opposite corner randomly selected from the area's corners. Degenerate corner choices are excluded. Horizontal-first/vertical-first traversal is randomized.

The double-OK area test still checks the full rectangular bounds from the center, independently of the work-path selection.

Movement settings and click counts apply to the next cycle. The selected button applies to the next burst and stays unchanged throughout that burst. A running test keeps the settings captured at its start.

## Saved settings

All eleven configurable parameters are saved to `apps_data/usb_mouse_motion/.settings`
on the Flipper SD card and restored on the next launch. A save is requested 500 ms
after the last Left/Right edit while paused. Changes made during movement wait until
the app pauses; exiting with Back also flushes pending changes. Storage writes never
run during movement or centering. Failed saves are logged and retried after 2 seconds
while paused; diagnostics are not displayed on the app screen.

The file has a type/version header and validated numeric ranges. Version 2 adds the path selection; existing version-1 files retain all previous settings and default to Random. Missing or invalid
settings fall back to defaults. Writing uses a temporary `.settings.tmp`; a complete
temporary file can be loaded if replacing the main file failed and left it unreadable.
The selected menu row, running state, cursor offset and random trajectory are not
persisted. Each launch starts paused with a new origin at the current pointer position.

## Timing and position

Natural mode initially chooses a movement-duration target from 25–95% of the available time after the random start, reserving the planned click burst and a 40 ms scheduling margin. Moving-speed limits take priority over that target. If movement and clicks cannot fit the selected interval, the cycle extends rather than exceeding its planned maximum speed. Long intervals leave idle time instead of forcing slower movement.

Paths contain 64 or 128 geometric segments. Each segment is interpolated into HID microsteps with 2–4 ms planned spacing rather than sent as one large jump. Interpolation always uses the original segment endpoints, with integer rounding and an exact final point, so splitting adds no net HID displacement error. Reports with zero movement are skipped; very low speeds remain limited by integer HID counts.

Run speed selects:

- **Natural**: variable segment timing within the selected limits, including a 40–120 ms pause.
- **Random**: one uniformly selected speed in the inclusive min/max range for the whole path.
- **Min speed**: constant selected minimum speed.
- **Max speed**: constant selected maximum speed.

Constant modes derive delays from segment distance and add no artificial random pause.
The captured speed is also used for split/corrective moves and pause centering. Changes
apply to the next cycle. Tiny quantized steps and the USB floor can make the realized
speed lower than selected. All delays are scheduled from completion of the previous
HID submission: delayed execution does not catch up by sending a burst of overdue moves.
Runtime overhead can lengthen a cycle or the nominal 2-second Standard boundary test.

The title shows the installed version; the status line shows the operating state
and USB connection. Tracked offsets and error diagnostics are not displayed. UI updates
are requested only when settings, selection, visible status or USB connection change.
Movement microsteps, unchanged button events and background-save retries do not redraw
the screen. There is no periodic UI refresh; an idle app waits for input/USB events
or a pending action deadline.

The origin is fixed when the application opens and is not shifted between cycles.
A random endpoint displacement is added to the current tracked offset, then clamped
to the fixed area. Each next path uses bounds relative to its actual tracked start:
`min_x = -X/2 - offset_x`, `max_x = X/2 - offset_x` (and likewise for Y).
For X=500, offset +50 gives -300..+200; offset +25 gives -275..+225.
Successful HID reports alone update that offset. Pause and test centering physically
send the inverse relative displacement; the counter is never simply reset to zero.

Every move is still clamped to the area captured at movement start. After resizing
below the current offset, the next cycle first centers in small inward steps. Start after
an interrupted return also centers before generating a new path. Disconnects and
failed reports hard-stop without discarding the offset, which remains available for
the next centering attempt. Back exits immediately rather than delaying exit for
centering.

**HID counts are not screen pixels.** Host acceleration, screen edges, manual mouse movement, USB disconnects, and runtime scheduling can change the on-screen path or timing. This is not a validated model of human motion or a guarantee of undetectability.

## Build

### Standalone with uFBT

From the repository root:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install ufbt
ufbt
```

Output: `dist/usb_mouse_motion.fap`.

### Inside a firmware checkout

From the root of a complete Flipper Zero firmware checkout:

```sh
git clone https://github.com/KyrylsPlayground/flipper-usb-mouse-motion.git applications_user/usb_mouse_motion
./fbt fap_usb_mouse_motion
```

Copy the resulting `.fap` to the Flipper SD card's `apps/USB` directory and launch it from the device. Connect Flipper to the target computer with a USB data cable.

The recorded builds used **target F7, API 88.2**. Hardware/host behavior has not been verified in this development session.

## USB identity

Configured identity: **VID 046D, PID C52B**, manufacturer `Logitech`, product `USB Receiver`, copied from the user's receiver. The application uses the firmware's standard combined keyboard/mouse HID descriptor. These identifiers do not reproduce that receiver's descriptors or behavior; the project is not affiliated with Logitech.

## Source layout

- `usb_mouse_motion.c`: application lifecycle, input, rendering, and HID state machine.
- `motion_settings.c/.h`: menu selection, setting ranges, and defaults.
- `motion_persistence.c/.h`: validated settings loading and SD-card saving.
- `motion_path.c/.h`: random spline paths, bounds, and perimeter test paths.
- `motion_timing.c/.h`: random scheduling and distance/speed-based timing.
- `motion_stream.c/.h`: fine-grained interpolation with exact endpoint accounting.
- `motion_tap.h`: single/double-tap recognition.
- `application.fam`: Flipper application manifest.
- `usb_mouse_motion.png`: 10×10 monochrome menu icon.

## Development history

The version commits were created retrospectively by replaying recorded edits from the development conversation. Their source snapshots preserve the recorded iterations; Git commit timestamps reflect the reconstruction, not the original editing times. The earlier, separate USB HID Autofire customization is not part of this application's history.

| Version | Change |
| --- | --- |
| 1.0.0 | Separate app, random closed paths, radius/interval/button controls, receiver identity |
| 1.1.0 | Random movement duration within 25–95% of remaining time |
| 1.2.0 | Spline paths, uneven timing, pauses, endpoint error, 1–3 click bursts |
| 1.2.1 | Fixed origin and tracked position prevent cumulative endpoint drift |
| 1.3.0 | Double-OK boundary test and return to pause |
| 1.4.0 | Rectangular bounds with independent X/Y dimensions |
| 1.5.0 | Interval up to 99 seconds and moving-speed limits of 100–700 |
| 1.5.1 | Minimum speed 200; boundary test at minimum speed |
| 1.5.2 | Mouse-with-sparkle menu icon |
| 1.5.3 | Minimum speed 314 |
| 1.6.0 | Scrolling settings list, adjustable speeds/click counts, three test modes, single-OK start/pause |
| 1.6.1 | Clamp every move to fixed bounds and calculate deltas from the latest tracked offset |
| 1.6.2 | Offset-relative path bounds; return to center before pause, resume, and boundary tests |
| 1.6.3 | Dimension step 1 / hold-repeat 25 and exact odd-sized rectangular bounds |
| 1.7.0 | Four work-speed modes, no catch-up bursts, uniform corrective speed, and visible tracked offsets |
| 1.8.0 | Save and restore settings on the SD card; paused autosave and exit flush |
| 1.9.0 | Fine-grained HID streaming, throttled display updates, and saved Random/Circle/Rectangle path selection |
| 1.9.1 | Start-on-circumference oriented circles and start-corner rectangles without entry/exit segments |
| 1.9.2 | Remove on-screen diagnostics and redraw only on visible state changes |

## License

Licensed under the GNU General Public License v3.0 (**GPL-3.0-only**).
See [LICENSE.md](LICENSE.md) for the full license text.
