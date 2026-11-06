# USB Mouse Motion 1.5.2

Menu icon: custom 10x10 monochrome mouse with a sparkle (`usb_mouse_motion.png`).

- Area X/Y: full width/height, independently 50..1000 relative HID counts (not pixels).
  Bounds are [-X/2,+X/2] and [-Y/2,+Y/2] about the fixed center.
- Short Left/Right: decrease/increase the selected dimension by 50.
- Long Left: select X; Long Right: select Y. The selected value is in brackets.
- Up/Down: interval 1..99 seconds.
- Short OK: left/right button for the next burst. Long OK: start/pause. Back: exit.
- Double OK (within 350 ms): interrupt normal operation, travel to the boundary,
  trace the rectangular boundary of the selected dimensions, then return to the fixed center.
  The test uses the configured minimum moving speed (200 HID counts/second),
  sends no mouse clicks, and finishes paused. Duration depends on the path length.
  Long OK can stop the test; Back exits. A single OK is delayed 350 ms to distinguish
  it from a double tap. Dimension changes during a test apply to the next run.
- Starts paused. Dimensions/interval changes apply next cycle. A burst uses one button throughout.

Each cycle generates a new spline through random waypoints within the chosen rectangle.
A small correction near the end finishes approximately 1..5 HID counts from a fixed
origin established when the application opens. Every successful movement updates the
tracked offset, including partial paths before pause, so subsequent paths begin at the
actual tracked offset and endpoint errors do not accumulate across cycles or pauses.
After shrinking a dimension below the current offset, one recovery path starts at the
old position; subsequent paths fit the new rectangle. The origin persists until app exit.
These are relative HID bounds: host acceleration, screen clipping, physical mouse
movement and disconnects can change the on-screen position independently.
Uneven timing slows the beginning/end, varies intermediate speed and adds a short pause
when the duration budget allows. Movements use 64 or 128 samples, at least 2 ms apart.
The initial duration target is 25..95% of available time after a random start,
reserving the planned click burst and 40 ms scheduling margin. Per-moving-segment
timing is then clamped to static limits in `motion_timing.h`: `MOTION_SPEED_MIN=200`
and `MOTION_SPEED_MAX=700`, in relative HID counts per second. The first move also
waits its calculated travel time. Zero-distance samples use 2 ms. An explicit random
40..120 ms pause is excluded from the moving-speed limits.
Speed limits take precedence over the duration target. If even the fastest permitted
movement plus clicks cannot fit the selected interval, that cycle is extended and
starts immediately. Long intervals leave additional idle time instead of slowing
motion below the minimum speed. OS acceleration changes pixel speed; these limits
are configurable starting values, not a validated model of human motion. Host and
firmware scheduling can affect actual speed and cycle duration.
The double-OK perimeter test calculates each movement delay from its distance at
`MOTION_SPEED_MIN`, rounded to milliseconds with a 2 ms USB floor, without random pauses.

The path ends with a random burst of 1..3 clicks: each press 25..60 ms, gaps 50..90 ms.
Host settings may interpret these as double/triple clicks. Stop/exit releases a pressed
button. USB failures pause the app. Host acceleration and screen clipping affect the path.

USB identity: VID 046D, PID C52B, Logitech, USB Receiver, from the user's receiver.
The firmware's combined keyboard/mouse HID descriptor is used; it does not reproduce
that receiver's descriptors or establish undetectability.

Build: `ufbt` here, or `./fbt fap_usb_mouse_motion` in a complete firmware checkout.

## License

Licensed under the GNU General Public License v3.0 (**GPL-3.0-only**).
See [LICENSE.md](LICENSE.md) for the full license text.
