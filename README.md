# USB Mouse Motion 1.0.0

- Left/Right: radius 25..500 relative HID counts (host acceleration and screen edges affect pixels).
- Up/Down: interval 1..30 seconds.
- Short OK: left/right click after finishing the trajectory.
- Long OK: start/pause. Back: exit. Starts paused.
- Radius and interval changes apply to the next cycle. Click selection applies to the next click.

Each interval contains one new smooth, randomly shaped closed path and one click.
The start time is random within the time left after reserving movement and click time.
Movement adapts to short intervals: approximately 0.4 seconds at 1 second,
approximately 2 seconds at intervals of 5 seconds or longer.
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
