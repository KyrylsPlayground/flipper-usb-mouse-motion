# USB Mouse Motion 1.2.0

- Left/Right: radius 25..500 relative HID counts (not screen pixels).
- Up/Down: interval 1..30 seconds.
- Short OK: left/right button for the next burst. Long OK: start/pause. Back: exit.
- Starts paused. Radius/interval changes apply next cycle. A burst uses one button throughout.

Each cycle generates a new spline through random waypoints within the chosen radius.
A small correction near the end finishes 1..5 HID counts from the starting point;
this deliberately replaces exact closure and can accumulate drift over repeated cycles.
Uneven timing slows the beginning/end, varies intermediate speed and adds a short pause
when the duration budget allows. Movements use 64 or 128 samples, at least 2 ms apart.
Duration remains 25..95% of the available time after the random start, reserving the
planned click burst and 40 ms scheduling margin. Late starts are limited to preserve
minimum USB spacing. Runtime scheduling delays can extend the planned timing.

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
