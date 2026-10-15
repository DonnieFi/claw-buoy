# Enclosure v5 audit

This is the audit for the v5 enclosure in this directory. It is a fit draft, not a confirmed print-ready hardware assembly. It also lists what changed from the earlier v4 draft (v4 files are not included in this repository). The v4 body/back collision check covered only plastics, not boards or connectors.

## Findings corrected from v4 in v5

- ESP32 tie tunnel roof spanned 26 mm; it now spans 2.8 mm across each 6 mm-wide support. Feed ties along the case height through each rail, around the PCB width.
- AHT20 tie tunnel roof spanned 8 mm; it now spans 2.8 mm. Feed one tie through both bars around the PCB length, keeping the sensing element uncovered.
- Bare bars did not positively locate the boards. Added side/end stops; ties remain mandatory for retention.
- V4 cover screw holes had only 0.6 mm of material between their edges and the cover perimeter. The rear opening now has a 1 mm wider rabbet; the plate is 116.6 x 66.6 mm, and screw-hole edge margin is 1.6 mm.
- The main wall remains 2.4 mm, with a 1.4 mm rim at the final 2.4 mm of depth only. Plate clearance stays 0.3 mm per side. This is a screw-fastened clearance fit, not a snap fit.
- Added a stepped 45-degree underside to the seating ledge and a gabled 45-degree roof to the divider cable opening.
- Trimmed rear-cover screw bosses where they touched the estimated screen PCB corner envelope. Pilot depth near the back remains available for the cover screws.

## Fastening

Two printed parts: body and back. No carrier, tray, or printed clips.

- Four cover clearance holes, diameter 3.4 mm; four body pilots, diameter 2.7 mm. Use suitable approximately 3 mm plastic thread-forming screws. An 8 mm under-head length gives approximately 5.6 mm engagement after the 2.4 mm plate. Pilot suitability depends on the actual screw and print material. Machine screws require tapping; these holes are not inserts or printed threads. Screw heads stand proud of the flush plate.
- Four screen pilots, diameter 2 mm, spaced 77 x 44 mm. The posts have 5 mm pilot depth above a closed 3 mm front wall. Use short suitable approximately 2.5 mm plastic thread-forming screws with heads compatible with the PCB holes. Choose under-head length as actual PCB thickness plus approximately 3 mm engagement, and below PCB thickness plus 5 mm. No screw point may reach the display glass or front wall.
- ESP32 uses two ties, nominal width at most 2.5 mm and thickness at most 1.2 mm. Route gently over clear PCB areas, not over components or the antenna. No assumed ESP32 screw holes.
- AHT20 uses one similar tie around both supporting bars and the board. Do not cover its sensing element or pinch connectors.

## Hardware envelope checks

Screen PCB: estimated 93 x 55 x 1.6 mm at x4..97, y7.5..62.5, z8..9.6. Owner-measured screw spacing is approximately 77 x 44 mm, centered at x50.5 y35; actual PCB-to-hole and active-area offset remain unmeasured.

Owner-measured screen depth estimate: front glass to board approximately 4 mm, to pin ends approximately 14 mm. With the PCB support face at z8, glass nominally reaches z4, pin ends z18. ESP32 PCB nominal front-facing surface reaches z28.4, giving approximately 10.4 mm to bare screen pin ends before any ESP32 component or connector intrusion. This does not establish clearance to boxed jumper housings or bent wires.

ESP32 PCB: estimate 65 x28 x1.6 mm; nominally supported 4 mm above inside back. If jumper sockets face the back, 4 mm may be insufficient; if they face the screen, their reach must fit the above gap. Exact component contact and wire routing remain unverified.

AHT20 PCB: owner-measured 10 x 15 mm; sensor bay approximately 15.6 mm wide. Stops and bars fit the nominal PCB; right-angle jumper housing envelope remains unmeasured.

## Verification

- Both exported STLs are watertight, consistent in winding, positive in volume and each a single connected solid.
- Boolean overlap is below 0.001 cubic mm for body/back, screen PCB/body, ESP32 PCB/back, AHT20 PCB/back and both rear PCB envelopes/body.
- STL cross sections at representative floor, rail, hole, bridge and ledge heights are closed.
- Bridges are approximately 2.8 mm across tie tunnels and 3 mm across vents; ledge and cable roof transition at approximately 45 degrees.
- CuraEngine was present but produced empty G-code with its supplied defaults, so slicer verification did NOT succeed. These checks are geometry checks, not a completed print or a validated printer profile. No G-code is supplied.

## Printing and assembly

Body front face down; back exterior face down, rails facing up. Start with 0.2 mm layers, 0.4 mm nozzle, at least three walls and 4–5 solid top/bottom layers in your known PLA profile. These are starting settings, not a validated job. Inspect slicer layer preview at tie roofs, ledge and divider roof; actual bridging capability varies. Clear tie passages after printing and test back clearance before loading boards.

Dry-fit screen and cable plugs, fasten screen, tie ESP32 and AHT20 to back, check side USB reach and internal wires, then fasten back. Do not tighten PCB screws hard enough to bow the board. Use the v5 body and back together.

## Regenerating the STL files

`build.py` rebuilds `body.stl` and `back.stl` from the parametric geometry and re-runs the clearance and watertightness assertions; `render.py` redraws `preview.png` from the STL files. With Python 3.10 or newer:

```bash
cd hardware/enclosure/v5
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
python3 build.py    # rewrites body.stl and back.stl
python3 render.py   # optional: rewrites preview.png
```

`enclosure.scad` is the OpenSCAD form of the same geometry. The checked-in STL files were regenerated with this recipe (numpy 2.5.4, manifold3d 3.5.4, trimesh 5.1.1, networkx 3.7) and came out byte-identical.
