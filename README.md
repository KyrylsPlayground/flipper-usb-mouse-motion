# USB Mouse Motion

A Flipper Zero USB HID application with smooth randomized mouse movement, configurable click bursts, and a rectangular boundary test.

Current version: **1.6.3**. Starts paused. Settings reset when the app closes.

## Features

- Independent X/Y dimensions around a fixed center, with tracked offsets to avoid cumulative endpoint drift.
- Random spline paths, variable movement timing, short pauses, and small endpoint corrections.
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

Holding OK is not required. Pause releases the pressed button, returns to the tracked center at the selected minimum speed, then becomes fully paused. The UI shows CENTERING during that return. The test interrupts normal operation, first returns to the same center, then traces the rectangular perimeter without clicks and finishes centered and paused. Its Standard 2-second duration excludes the preceding centering motion.

## Settings

| Setting | Range / options | Default |
| --- | --- | --- |
| X width | 50–1000 HID counts, step 1; hold-repeat 25 | 200 |
| Y height | 50–1000 HID counts, step 1; hold-repeat 25 | 200 |
| Interval | 1–99 seconds | 10 seconds |
| Minimum speed | 50–2000 HID counts/second | 314 |
| Maximum speed | 50–2000 HID counts/second | 700 |
| Mouse button | Left / Right | Right |
| Minimum clicks | 0–10 | 1 |
| Maximum clicks | 0–10 | 3 |
| Test mode | Min speed / Max speed / Standard | Standard |

Minimum values cannot exceed their corresponding maximum values. A short Left/Right changes a speed by 1. Set both click counts to **0** for movement only. Each cycle randomly chooses its click count within the selected range; zero skips clicking. Presses last 25–60 ms with 50–90 ms gaps, which the host may interpret as double/triple clicks.

X/Y are the **full width and height**, not radii. For example, X=400 and Y=200 gives relative bounds of ±200 horizontally and ±100 vertically.

Odd dimensions use integer bounds that retain the exact selected size: X=501 gives -250..+251. The extra HID count is on the positive side.

Movement settings and click counts apply to the next cycle. The selected button applies to the next burst and stays unchanged throughout that burst. A running test keeps the settings captured at its start.

## Timing and position

The initial movement-duration target is randomly chosen from 25–95% of the available time after the random start, reserving the planned click burst and a 40 ms scheduling margin. Moving-speed limits take priority over that target. If movement and clicks cannot fit the selected interval, the cycle extends rather than exceeding its planned maximum speed. Long intervals leave idle time instead of forcing slower movement.

Paths use 64 or 128 samples with a 2 ms minimum USB spacing and an explicit 40–120 ms pause. Small quantized steps cannot always represent the selected speed exactly. Speed-based tests use distance-derived delays without random pauses. Standard test mode schedules the complete perimeter-and-return path over approximately 2 seconds.

The origin is fixed when the application opens and is not shifted between cycles.
A random endpoint displacement is added to the current tracked offset, then clamped
to the fixed area. Each next path uses bounds relative to its actual tracked start:
`min_x = -X/2 - offset_x`, `max_x = X/2 - offset_x` (and likewise for Y).
For X=500, offset +50 gives -300..+200; offset +25 gives -275..+225.
Successful HID reports alone update that offset. Pause and test centering physically
send the inverse relative displacement; the counter is never simply reset to zero.

Every move is still clamped to the area captured at movement start. After resizing
below the current offset, recovery moves only inward until back inside. Start after
an interrupted return also centers before generating a new path. Disconnects and
failed reports hard-stop without discarding the offset; the paused UI shows OFF CENTER
if centering did not complete. Back exits immediately rather than delaying exit for
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
- `motion_path.c/.h`: random spline paths, bounds, and perimeter test paths.
- `motion_timing.c/.h`: random scheduling and distance/speed-based timing.
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

## License

Licensed under the GNU General Public License v3.0 (**GPL-3.0-only**).
See [LICENSE.md](LICENSE.md) for the full license text.
