# USB Mouse Motion 1.1.0

- Left/Right: radius 25..500 relative HID counts (host acceleration and screen edges affect pixels).
- Up/Down: interval 1..30 seconds.
- Short OK: left/right click after finishing the trajectory.
- Long OK: start/pause. Back: exit. Starts paused.
- Radius and interval changes apply to the next cycle. Click selection applies to the next click.

Each interval contains one new smooth, randomly shaped closed path and one click.
The start time is random; enough time is reserved for movement, a 40 ms click,
and a 40 ms scheduling margin. Movement duration is randomly chosen between
25% and 95% of the time remaining after that start and the click/margin reservation.
Short movements use 64 samples; longer movements use 128 samples. USB reports
are scheduled at least 2 ms apart. Late start times are limited so even the
25% case can complete the path smoothly within the interval.
The requested radius bounds the path relative to its starting position.
Stop interrupts motion immediately; completing a path returns the relative HID offset to zero.
USB failures pause the application. Host acceleration/clipping can prevent an exact on-screen return.

USB identity: VID 046D, PID C52B, manufacturer Logitech, product USB Receiver,
as requested from the user's receiver. Uses the firmware's standard combined keyboard/mouse
HID descriptor; these settings do not reproduce the real receiver's descriptors or behavior.

Build with `ufbt` in this folder, or `./fbt fap_usb_mouse_motion` in a complete firmware checkout.

## License

Licensed under the GNU General Public License v3.0 (**GPL-3.0-only**).
See [LICENSE.md](LICENSE.md) for the full license text.
