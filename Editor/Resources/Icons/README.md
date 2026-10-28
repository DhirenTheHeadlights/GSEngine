The Sweep mark's rounded outlines are defined in `Sweep.svg`. `Brand.cppm` draws the generated mesh with editor primitives and uses the theme's accent, text, and icon colors.

Run `python Tools/generate_editor_icon.py` from the repository root after changing `Sweep.svg` or the midnight theme. The script samples the rounded outlines, checks triangulation coverage, and regenerates `Editor/Editor/Source/App/BrandShape.cppm`, `GSEditor.ico`, `GSEditor.png`, and `GSEditor.svg`. Edit the source SVG rather than the generated mesh or exported assets. All outputs share the same sampled contours; there is no runtime triangulation. The ICO contains 16, 24, 32, 48, 64, 128, and 256 pixel images and is embedded by `Editor/Resources/Editor.rc`.

The running window icon is generated from the resolved GUI style when that style changes, including while settings are open and for popout windows. The executable and desktop shortcut use the embedded midnight icon.
