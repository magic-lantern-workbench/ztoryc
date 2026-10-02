# External clipboard review follow-up

Review: matitanimata/ztoryc#4, October 2, 2026.
Code update: `9b4413a36195a65da52b1fc9a1bcf150072344a3`.

## Changes

Vector clipboard preview rendering now preserves the caller's Qt OpenGL context
and surface across the entire lifetime of the temporary rendering resources.
Restoration happens after their destruction, including on exceptions. If no
context was current on entry, none is left current afterward. A restoration
failure is reported rather than throwing from the cleanup guard.

The rendered vector clipboard image is limited to 4096 pixels per dimension,
with aspect ratio preserved and invalid/non-finite bounds rejected before
allocation. Large previews are downscaled; native strokes, geometry, palette
and the PLI payload are not. Preview creation also avoids an extra full-raster
clone. This is an allocation bound, not a guarantee of copy latency.

The Qt offscreen backend checks context creation, surface validity, framebuffer
validity/binding, and make-current failures. The surface uses the created
context's actual format. Ordinary resource failures can reach the clipboard's
existing native-copy fallback. This does not claim to catch graphics-driver
crashes, and it does not globally change the backend's context-restoration API.

## Paste behavior and limits

Drawing and external-image clipboard contents use normal Xsheet paste even
when the preference selects overwrite/cell-numbers-only paste. Copied Xsheet
cell data still takes the existing overwrite-numbers path under that preference.
This non-cell behavior change is intentional and should be stated in the PR
description.

Native raster transfer between instances uses the drawing Selection tool.
Direct cross-instance Xsheet raster paste remains image-based, not native Toonz
raster with its palette. Native in-process data still takes precedence over the
image representation, but drawing Copy/Cut still performs the new export work.
The eager Toonz raster serialization is unchanged by this follow-up.

The pre-existing overwrite-numbers no-op for a genuinely empty or nonexistent
column is not fixed here. Its normal-paste fallback remains a separate companion
correction. An empty cell in a populated column is a different test case.

Ztoryc's single-instance guard and concurrent project-save protection remain
separate work; clipboard transfer alone does not make simultaneous project
editing safe.

## Validation performed

An isolated local C++11 harness compiled and ran the new preview helper with
mocked Qt/GL resources under AddressSanitizer and UndefinedBehaviorSanitizer.
It exercised normal return, construction/clear/drawing/readback exceptions,
saved-surface restoration, no-prior-context handling, and 100,000 preview
geometry cases. Source comparisons confirmed unchanged payload/PLI and
paste/selection code. Added-line whitespace checks passed.

These are helper control-flow/arithmetic tests, not a full Qt build, a real
OpenGL test, or a raster-copy benchmark. Qt development headers, clang-format
and macOS were not available in the local validation environment.

## Manual acceptance checks still required

- [ ] Full macOS build and repository formatting checks. For a local macOS
      application test, use the repository's `build_and_deploy.sh` so required
      helper binaries are included in the bundle.
- [ ] Repeated vector Copy/Cut followed by drawing, scrubbing, playback, undo
      and redo. Include very large/widely separated strokes; verify the image
      preview scales down while editable native geometry does not.
- [ ] Instance A to B: vector Selection-tool paste; Toonz raster with empty
      and conflicting destination palettes; full-colour raster. Check correct
      colors and editable style associations/remapping, not identical numeric
      style IDs where remapping is necessary.
- [ ] Xsheet paste and external-application image paste under both paste
      preferences. Distinguish empty cells in populated columns from genuinely
      empty/nonexistent columns; record the known overwrite-numbers limitation.
- [ ] Same-instance Copy/Cut/Paste for all three drawing types, native-data
      precedence, and undo/redo.
- [ ] Approximately 4K raster and Toonz raster selections: measure Copy/Cut
      responsiveness and memory use. No performance result is claimed yet.
- [ ] Confirm the test setup permits two instances without conflating this PR
      with Ztoryc's separate single-instance/project-locking changes.

The integration could update the source branch but was denied permission to
edit the upstream PR description or post a reply. These notes are included in
the PR diff so the behavior clarification and pending tests remain reviewable.

Original port author: Rodney (@RodneyBaker). Follow-up prepared with assistance
from ChatGPT in response to matitanimata's review.
