# Packet Format — Reverse Engineering Notes

This document is the live record of Magic Mouse HID packet decoding.
Updated as Phase 0 HID logger captures are analysed.

## How to Capture Packets

1. Pair Magic Mouse via Windows Bluetooth settings
2. Run `hid_logger.exe` with the mouse selected
3. Move finger, tap, scroll — capture a variety of inputs
4. Save the `.bin` file to `docs/packet-samples/`

## Device Identifiers

| Model         | VID    | PID    | Protocol          |
|---------------|--------|--------|-------------------|
| Magic Mouse 1 | 0x05AC | 0x030D | Bluetooth Classic |
| Magic Mouse 2 | 0x05AC | 0x0269 | BLE               |

## Report Format

> **TODO:** Fill this in from Phase 0 HID logger output.

### Known / Suspected Fields

| Byte(s) | Field              | Notes                        |
|---------|--------------------|------------------------------|
| 0       | Report ID          | TBD from capture             |
| 1–2     | Finger count       | TBD                          |
| 3–4     | Touch X (finger 1) | TBD — signed 16-bit?         |
| 5–6     | Touch Y (finger 1) | TBD                          |
| ...     | ...                | Decode and fill in here      |

## Battery Report

> **TODO:** Confirm report ID and byte offset from Phase 0 captures.

Suspected: single byte, `0x00 = 0%`, `0x64 = 100%`.

## Community References

- https://github.com/robotrovsky/Linux-Magic-Trackpad-2-Driver
- https://github.com/nickcoutsos/macbook-magic-mouse-2
