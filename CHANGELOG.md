# Changelog

All notable changes to axGUI. Dates are YYYY-MM-DD.

## [0.1.0] - 2026-10-20

First public version: the library together with its demo app, desktop and WebAssembly builds.

### Added
- **Style editor**: `showStyleWindow()` (ready-made window that keeps its own look while you edit) and `styleEditor()` (contents only). Sizes, colors, behavior, text box and tab settings; save / load as JSON; "Copy C++" exports the current style as code.
- **Color picker** (`drawColorPicker`, opened by `colorButton`): palette, HSV saturation / value square, hue and alpha bars, eyedropper, editable HEX field (`#RRGGBB` / `#RRGGBBAA`), alpha % and RGB fields.
- **Anti-aliased rendering** in axDrawBot (ImGui-style AA fringe): `aaFillConvex`, `aaFillFan`, `aaStroke` and path builders for rounded rects, ellipses and arcs. Used by rects, circles, ellipses, lines, triangles, stars and tooltips. `bot.aaFringe = 0` turns it off.
- **Icons from drawings or textures**: `bot.addIconFromDrawing(name, px, fn)`, `bot.addIconFromTexture(name, tex, flipY, src)`, `bot.removeIcon(name)`; `gui.loadIconAtlas(path, cell)` and `bot.addIconCell(name, col, row)` for icon atlases.
- **Combo box item icons**: `IdStrPair::icon`.
- **Localization**: `axLang` / `axTr("English", "Türkçe")`, and ImGui-style `"Label###id"` IDs so window, section, group and tab state survive a language change.
- **Collapse animation** for `beginSection` and `beginGroupBox` (`gui.collapseDuration`); reusable as `beginCollapse(id, open)` / `endCollapse()`.
- **Two-way hover animation** for buttons, toggles and section headers (`gui.hoverDuration`).
- **Style parameters**: `gui.checkRadius`, `gui.frameRadius`, `gui.hintTime`, `gui.smoothText`, `gui.winSnap`, `gui.styleWinPos` / `gui.styleWinSize`, `GroupStyle::padding`.
- **Theme colors**: `GUI_TOOLTIP_BG`, `GUI_TOOLTIP_TEXT`; helpers `mixColor`, `withAlpha`, `contrast`.
- **Retained-mode helpers**: `bot->deferred(fn)` runs direct raylib drawing code in the right layer and clip; `intersectScissorWorld` is recorded correctly.
- **Text gamma** for the font atlases: `rtFonts().setTextGamma(g)` (default 1.3).
- **Mouse wheel latch**: once a window starts scrolling, the wheel stays with it until scrolling stops (`gui.wheelLatchDelay`).
- **WebAssembly build** (Emscripten, WebGL 2) through CMake, with an HTML shell in `web/`.
- Demo: screen-size based window layout, English / Turkish switch, drawn icons, rich text window.
- Project layout: the library sources are directly in `src/` (with `rich/` and `sdf2d/`), demos live in `demos/<name>/` (`demos/showcase` is the former demo app). CMake builds the library as the `axgui` static target and each demo with `axgui_add_demo()`; qmake uses `axGUI.pro` (subdirs) with `src/axGUI.pri` and `libs/raylib.pri`.

### Changed
- The main class is now `axGUI` (was `myNanoGui`).
- Global names cleaned up: `axLog()`, `axFormat()`, `axEpsilon`; `using namespace std` removed from headers.
- Default colors are a dark theme; every built-in widget color now comes from the theme.
- The focused window is drawn on top when retained mode is used (`gui.startFrame(true)`).
- Text is drawn with a per-size font atlas, snapped to whole pixels unless animating or `smoothText` is set.
- Rounded corners use a radius-based segment count (`cornerSegments()`).
- Combo box dropdown: aligned with the box, shadowed rounded panel, chevron, distinct hover and selection colors.
- Tooltips belong to the hovered widget: they appear after `hintTime`, stay while the mouse moves over the same widget, and hide on leave or click.
- Popup windows (`WIN_POPUP`) are kept on screen; the color picker opens right below its button, or above it when there is no room.
- `syntaxColors`, `titleBarH` and `tabStyle` can be changed by the application.

### Fixed
- Window wheel scrolling (`WIN_SCROLLBAR`) and scrolling while a widget has focus.
- Moving and resizing nested windows; windows jittering while being moved; snapping to a window's own children.
- `colorSelector` returning without `endWindow`, ignoring `open`, and not reporting `dragColor` changes.
- Nested windows: cursor position after `endWindow`, clipping, hover / focus state, following the parent's scroll.
- `textBox` clipping (draws outside a scrolled window, cuts tall glyphs in single-line boxes).
- Group box content offset and clipping; its state no longer resets when earlier widgets change; dragging over a window above no longer scrolls it.
- Widgets under an open dropdown or color picker no longer receive mouse input.
- `slider` only starts dragging when pressed on it, and no widget shows hover while another one is being dragged.
- `star()` fill, retained-mode recording and anti-aliasing.
- `drawIcon` uses the icon's own texture; section arrows render again.
- `toColor` clamps channels (light-theme buttons turned black).
- `dragVec2` crash with the default hint.
- `axLog()` output flushes immediately.
- CMake: no `CUSTOMIZE_BUILD`, which made `GetFrameTime()` return 0 with raylib 6.0.

### Known issues
- `axDrawBot::arc`, `cross`, `drawShape`, `fill` / `stroke` still draw directly with raylib (no AA, not recorded in retained mode).
- Tab bar colors (`TabBarStyle`) are not derived from `gui.colors` automatically.
- `checkbox` draws its box at the right edge of the given width; `size.x = 0` overlaps the next item.
- Multi-line `textBox` with `wrapLines` does not wrap long lines yet.
- `WIN_AUTO_NEWLINE` assumes the next item is as wide as the previous one.
- In immediate mode (`startFrame(false)`) windows are drawn in call order.
