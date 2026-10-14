# Welcome to Ztoryc!

Ztoryc is a storyboard and animatic tool that lives inside an animation application: the boards you draw are already the shots you will animate. What's new in each version is listed on the [Releases page](https://github.com/matitanimata/ztoryc/releases).

### How a Ztoryc scene is made

- The main Xsheet has **one column per shot**, and every shot is a sub-scene.
- **BOARD** room: draw and arrange the panels of the storyboard.
- **ANIMATIC** room: set the timing of the shots, add sound and dissolves.
- Inside a shot you work as in any Toonz scene: layout, poses, animation.
- Sound belongs to the main Xsheet (the animatic), never to the sub-scenes.
- **Copy** makes a shot that shares the same sub-scene; **Clone** makes a fully independent one.

-----

## Coming from OpenToonz or Tahoma2D

Ztoryc is based on Tahoma2D, so a few things differ from OpenToonz:
- Cells and Columns have no drag bars by default: click an exposed cell or a non-empty column, then drag it. To get the old behavior, enable `Preferences` -> `Scene` -> `Show Column and Cell Drag Bars`.
- `Alt` (`⌥ Option` on macOS) selects frames and animation keys; `Ctrl` (`⌘ Cmd` on macOS) starts a multi-cell/column selection.
- Implicit holds: an exposed frame is shown until the next exposed frame. Use `Stop Frame Hold` to end a hold.
- Some advanced features and settings are hidden: enable `Preferences` -> `General` -> `Show Advanced Preferences and Options`, then restart.

-----

## Tips

- Scenes and drawings are stored in separate files inside a project folder: create a project for each production instead of using the default `sandbox`.
- Use `Save All` regularly: it saves both the scene and the drawings. `Save Scene` saves only the scene.
- When using external files, use `Import` to keep the originals untouched.
- For several raster files to become one level, name them `levelname.####.ext` or `levelname_####.ext` (e.g. ABC.0001.png, ABC.0002.png).
- The animatic exports to video, and as a timeline for your editing software: `.fcpxml` (DaVinci Resolve, Final Cut Pro) and `_premiere.xml` (Adobe Premiere Pro).
- Keep an eye on the Status bar for more information about the current tool.
- Need help, or found a bug? Join the [Ztoryc Discord](https://discord.gg/ZP2gqQwmDb).
- Ztoryc is free and open source. If it helps you or your studio, consider [sponsoring it](https://github.com/sponsors/matitanimata).
