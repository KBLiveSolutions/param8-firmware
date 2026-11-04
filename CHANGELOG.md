# Changelog

## v1.0.3 — 2026-11-04

### Fixed
- **SysEx manufacturer ID filter**: `onSysEx` now rejects any SysEx message whose first two bytes are not `F0 6F` (param8's constructor byte). Previously, any SysEx from a third-party device or plugin routed to param8's MIDI port could accidentally trigger firmware actions — including flash writes — causing USB MIDI to drop permanently and requiring a device reboot.

---

## v1.0.2 — 2026-10-17

### Fixed
- Position display guard for encoder feedback
- Control name persistence across preset changes
- Preset lock after selection to prevent accidental re-trigger
- Utility Output and Utility Gain parameter names
- Dimmed fader display when encoder is inactive

---

## v1.0.1

### Fixed
- Initial bug fix release after v1.0.0
