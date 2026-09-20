# Touchscreen Design Guidelines

- https://learn.microsoft.com/en-us/windows/apps/develop/input/guidelines-for-targeting
- https://learn.microsoft.com/en-us/windows/win32/uxguide/inter-touch
- https://developer.apple.com/design/human-interface-guidelines/
- https://developer.apple.com/design/tips/
- https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum
- https://www.w3.org/TR/WCAG22/#target-size-minimum
- https://www.iso.org/standard/77520.html
- https://www.iso.org/ics/13.180/x/
- https://webstore.iec.ch/en/publication/624
- https://webstore.iec.ch/en/publication/2186
- https://webstore.iec.ch/en/publication/71256

## Review scope and source findings (2026-09-19)

Every URL above was opened and its available content reviewed. The two Microsoft
articles, Apple design tips, and the W3C target-size pages provide directly
usable guidance. The Apple HIG and ISO ergonomics pages are indexes, so their
relevant topics are distinguished below from the index itself. ISO/IEC catalog
pages are not the full standards: the paid normative texts were not available
in the project and were not purchased. No certification or full platform/WCAG
conformance is claimed by this GUI update.

### 1. Microsoft: Guidelines for touch targets

[Source](https://learn.microsoft.com/en-us/windows/apps/develop/input/guidelines-for-targeting).
A physical target around 7.5 mm is the general recommendation (the example uses
40 pixels at 135 PPI). Frequency, consequence of an error, position, screen
size, posture and feedback should influence the final size. Frequent actions
can be larger; consequential actions need additional separation from edges.
**Application:** keep the full-width run control, use 44-pixel minimum visible
buttons, inset controls, and preserve confirmation before deleting or replacing
stored data. Physical dimensions still depend on the actual panel.

### 2. Microsoft: Touch (Win32)

[Source](https://learn.microsoft.com/en-us/windows/win32/uxguide/inter-touch).
This is older Windows guidance, not a current embedded-device specification.
It covers familiar gestures, immediate feedback, forgiving controls, safe
defaults, constrained input, target sizes and spacing. Its custom-control advice
uses 7 mm generally, 9 mm when mistakes are harder to correct, and a limited
5 mm fallback with 2 mm spacing. It recommends at least 5 pixels between
interactive controls in its Windows layout context, larger frequent commands,
nearby related controls, and avoiding small edge targets. Hover must not be
required; a tap ends on release. Keyboard, text-entry, multi-touch rotation,
zoom and Windows shell guidance apply only where those features exist.
**Application:** LVGL buttons, release activation, slide-off cancellation, nearby
value adjustment controls, visible unavailable states, and no added keyboard,
custom gesture, zoom or rotation dependency.

### 3. Apple: Human Interface Guidelines

[HIG index](https://developer.apple.com/design/human-interface-guidelines/).
The landing page organizes design principles, foundations and platform topics;
it is not itself a single touchscreen specification. Its official documentation
JSON was read because the HTML landing page requires JavaScript. Relevant
chapters reviewed:

- [Design principles](https://developer.apple.com/design/human-interface-guidelines/design-principles):
  familiar behavior, clear feedback, recoverable mistakes, concise wording,
  hierarchy and iterative evaluation. Apply these to the existing task flow.
- [Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility):
  use more than color or sound alone, sufficient contrast and spacing, simple
  gestures, visible alternatives, and explicit dismissal. The current table
  distinguishes recommended/default control sizes from smaller platform minima;
  those are Apple points. VoiceOver, Dynamic Type, speech and switch integration
  are Apple platform features, not capabilities supplied by this firmware.
- [Layout](https://developer.apple.com/design/human-interface-guidelines/layout):
  group related actions, preserve hierarchy and safe areas, and test text lengths
  and clipping in different localizations. Fixed hardware does not need Apple
  window size classes or Liquid Glass, but it still needs bounds checks.
- [Buttons](https://developer.apple.com/design/human-interface-guidelines/buttons):
  recognizable purpose, a pressed state, clear short labels, and consistent
  related choices. Use descriptive text where an isolated symbol is ambiguous.
- [Alerts](https://developer.apple.com/design/human-interface-guidelines/alerts):
  reserve interruptions for useful decisions, state the consequence briefly,
  name the action and provide an explicit cancellation. Prefer short messages
  that fit; allow scrolling here as a fallback for filenames and translations.
  Keep this device's established action positions and save/cancel colors rather
  than imposing Apple-specific button order on an existing embedded interface.
- [Gestures](https://developer.apple.com/design/human-interface-guidelines/gestures):
  familiar gestures, responsive feedback, visibly unavailable actions and tap
  alternatives to custom gestures. Keep tab buttons and tuning arrows; no action
  should depend on discovering a swipe.

### 4. Apple: UI Design Dos and Don'ts

[Source](https://developer.apple.com/design/tips/).
The page covers content fitting the screen, touch controls, 44 x 44 point hit
regions, text of at least 11 points, text/background contrast, non-overlapping
text, appropriate image resolution and aspect ratio, related-control grouping,
and alignment. These platform points are not firmware pixels.
**Application:** retain the existing 18/34-pixel fonts and crisp built-in symbols,
keep the QR square, improve bright-button contrast, fit labels and buttons inside
the display, and check both English and German. No raster artwork is needed.

### 5. W3C: Understanding Target Size (Minimum)

[Source](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum).
This explains SC 2.5.8 and its benefits for people with reduced pointer accuracy.
The target must contain an axis-aligned 24 x 24 CSS-pixel square, or qualify for
an exception. For undersized targets, the spacing test places a 24-pixel-diameter
circle at each target's bounding-box center; it must avoid other targets and
such circles. Size refers to the usable target, including clipping and shape,
not merely a declared rectangle. Newly opened overlays also need usable targets.
**Application:** inspect actual LVGL rectangles and ancestor clipping, including
lazy dialogs; do not use invisible hit-area expansion across neighboring controls.

### 6. W3C: WCAG 2.2, SC 2.5.8

[Normative target-size section](https://www.w3.org/TR/WCAG22/#target-size-minimum).
The AA requirement is 24 x 24 CSS pixels, with spacing, equivalent-control,
inline, user-agent-control and essential-presentation exceptions. SC 2.5.5 uses
44 x 44 CSS pixels at AAA. Related sections reviewed include release/cancellation
(2.5.2), color independence (1.4.1), contrast (1.4.3 and 1.4.11), labels (3.3.2),
and consistent identification (3.2.4). Text contrast guidance uses 4.5:1 normally
and 3:1 for large text; meaningful control graphics use 3:1 with exceptions.
**Application:** use these as design checks, not a web-conformance assertion.
The native panel has no CSS pixel mapping, browser accessibility tree, keyboard
navigation or screen-reader implementation. Do not claim those from larger buttons.

### 7. ISO 9241-210:2019

[Catalog and abstract](https://www.iso.org/standard/77520.html).
This concerns human-centred design throughout an interactive system's lifecycle.
It is a process framework rather than a pixel-size rule; the abstract explicitly
separates detailed ergonomics and health/safety treatment. The full normative
text is not available here. **Application:** record context, findings and test
limitations; use representative operating tasks to evaluate the result. This is
an engineering application of the public scope, not a clause-by-clause ISO audit.

### 8. ISO ICS 13.180: Ergonomics

[Catalog index](https://www.iso.org/ics/13.180/x/).
This lists many ergonomics standards and their status, including ISO 9241 parts;
it contains no single set of UI requirements. **Application:** treat it as a
bibliography for further hardware/usability work, not a source of invented
minimum pixel dimensions. Reading the index does not mean all listed standards
have been obtained or read.

### 9. IEC publication 624

[Catalog](https://webstore.iec.ch/en/publication/624).
The link resolves to IEC 60079-11:2011/ISH1:2014, an interpretation sheet about
intrinsically safe equipment in explosive atmospheres. The catalog marks it
revised/withdrawn and links a newer publication. **Application:** no touchscreen
layout rule can be derived from this page. No electrical protection or device
safety claim follows from UI styling.

### 10. IEC publication 2186

[Catalog and abstract](https://webstore.iec.ch/en/publication/2186).
This is IEC 60455-3-5:2006, about unsaturated polyester impregnating resins for
electrical insulation, including elevated-temperature properties. It is not a
touchscreen/HMI design document. **Application:** none to LVGL layout; retain the
link with this correction so it is not mistaken for a touch-target standard.

### 11. IEC publication 71256

[Catalog and abstract](https://webstore.iec.ch/en/publication/71256).
IEC 60204-1:2016+AMD1:2021 concerns electrical equipment of machines. The public
summary mentions drive systems, protection, bonding, emergency-stop/control
circuits, actuator symbols and technical documentation. It does not publish a
button-size rule. The full standard is not available here. **Application:** keep
this GUI work separate from electrical design verification; the on-screen Stop
command is an operating control, not a certified emergency-stop system.

## Device requirements derived from the findings

The existing hardware is 480 x 320 with configured `LV_DPI_DEF=130`. A 44-pixel
square is about 8.6 mm **if that configured density matches the physical panel**.
That conversion is an estimate, not a measurement. `ui_touch.h` centralizes the
44-pixel target and 8-pixel spacing choices. The gap is about 1.6 mm at that density;
it is not a claim to satisfy the older 2 mm exception for tiny targets, which
this design does not use. Adjacent tabs form one conventional segmented row.

| Finding | UI requirement / implementation |
| --- | --- |
| Target size and edge clearance | At least 44 x 44 visible button pixels; inset pages, profile actions and dialogs; no overlapping hit areas. |
| Readable hierarchy and alignment | Stable three-tab organization, adjacent value controls, complete primary weights, and labels contained within parents. |
| Clear purpose | Use standard symbols for unambiguous actions: sync, motor move, OK, close, copy, delete, create and Save. Keep state/value selectors as text, and state Flash/SD direction in the confirmation. |
| State feedback | Keep theme pressed/disabled feedback; visibly disable target adjustment while running instead of accepting a tap with no result. |
| Mistake recovery | Activate on release; slide outside to cancel; retain explicit confirmation for profile deletion and filesystem replacement. |
| Contrast and multiple cues | Black content on bright green/red/orange buttons; distinct text/symbols as well as colors; do not rely on beeps alone. |
| Complete decisions | Keep confirmation text concise where possible; wrap and vertically scroll long messages above fixed actions. |
| Localization | Test shipped English/German strings and long profile names; preserve language keys and storage/HTTP contracts. |
| Resource limits | Reuse LVGL buttons, labels, containers and shared styles; keep 24 KiB pool, fonts and enabled widgets unchanged. |

The installed ESP32 core (3.3.11), LVGL (9.5.0), local button/tabview/label/style/
scrolling examples, TFT touch integration and ESP-IDF LCD example index were
reviewed. Existing LVGL facilities supply the needed behavior; no new widget,
input driver, screen framework or image asset is needed. The scroll viewport is
one temporary container, destroyed with its message dialog.

## Verification

`python tools/ui_verify_layout.py` passed using the installed LVGL 9.5.0 and
32-bit MSVC. The harness compiles the actual `ui.c`, `ui_Screen1.c`, `ui_touch.c`
and extracts the current dialog factory bodies, avoiding a separately maintained
mock layout. Hardware callbacks only increment counters. It checks all three
tabs and opens all four tuning modes from the Profile tab in English/German,
including draw-memory headroom and immediate allocation recovery before another
dialog can open. It also checks minimum button size, ancestor clipping, 8-pixel
button separation, button text fit, release-only activation, slide into a
gap/neighbor cancellation, disabled target edits, modal blocking, long-message
scrolling with fixed actions, and allocation recovery over twenty message-dialog
lifecycles. Generated PNG previews in `%TEMP%/rtui_touch_audit`
were visually inspected for the main screen, Info, profile, German tuning and
long confirmation message.

The native harness uses one circle-cache entry because MSVC rejects LVGL's
zero-length GCC cache array; this override exists only in its generated config.
It retains the 24 KiB configured pool and enabled widgets. The monitor reported
21,748 managed bytes, an 18,280-byte peak and 13,524 bytes after dialogs closed.
The opaque modal backdrop prevents LVGL from also drawing the covered page, and
dialogs free synchronously so a result message cannot overlap the previous dialog.
Fixed Tune-button symbols use LVGL's background-symbol support, following its
spinbox example, which removes seven child-label objects. Redundant button
opacity and inherited-font/alignment properties are also omitted. The largest
free block during the Profile-to-Tune draw path is 4,384 bytes.
These are native harness measurements, not ESP32 runtime heap measurements.
The default blue was darkened for small white labels; bright green/red/orange
buttons retain black content. Existing red cancel and green save conventions
are preserved, with each action expanded to nearly half the tuning dialog width.

Firmware compile and image generation passed with
`python tools/firmware_build_upload.py --cli --error --compile-only`; the older `tools/compile_upload.py` named in `agents.md` is not
present. SD JSON parse validation and `git diff --check` passed. `lv_conf.h` is
unchanged. No device flash or electrical-standard certification was performed.
Native LVGL rendering can verify pixel bounds, text fit, state and synthetic
pointer behavior. It cannot establish actual finger accuracy, panel contrast,
Wi-Fi timing or ESP32 peak heap usage.

Physical-device follow-up: exercise all tabs and tuning modes with a finger,
including `100`, the motor-move action, save/cancel and both sync directions. Press, slide into a
gap, and release; also slide toward a neighboring button. Check that no unintended
action occurs. Read long translated messages to their end while actions remain
visible. Check QR placement, repeated dialog opening/closing with Wi-Fi active,
and physical readability under the intended lighting and viewing angle.

## Uniform row layout

Trickler controls and single-line labels use a shared 44-pixel height with
8-pixel horizontal and vertical gaps. Five main-screen rows occupy 252 pixels, fitting
below the 44-pixel tab bar with page margins. Profile navigation uses 88-pixel
Up/Down controls around the restored 75-pixel profile/action row, with a 5-pixel
inset around its Tune/Delete buttons; tuning uses four equal rows across a
404-pixel content area, with its mode selector placed at the panel's top inset
and Save/Close at the bottom inset. In Steps mode, one wide button cycles
through the `1`, `10`, and `100` increments beside the motor-test button,
directly below the step value. The profile-entry selector occupies the following
full-width row. Standalone labels add their centering
padding to an existing local style instead of allocating another style entry.
Multiline
messages and the Info log retain their variable/viewport heights. The native
harness checks exact row heights and gaps in addition to clipping and input.

Validation after row normalization: native geometry/input/lifecycle checks and
ESP32 compile-only build passed; SD JSON and whitespace checks passed.
