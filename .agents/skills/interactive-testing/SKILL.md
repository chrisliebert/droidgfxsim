---
name: interactive-testing
description: >-
  Build, run, and visually test droidgfxsim scenes interactively.
  Launches the desktop simulation, navigates the camera through dynamic trajectories,
  captures high-fidelity visual frames over extended sessions (30s+),
  and runs automated computer vision analysis for visual artifacts
  (blank frames, clipping, texture faults, motion degradation).
---

# Interactive Visual Testing Skill for droidgfxsim

This skill provides an automated, agentic framework for verifying the visual rendering and simulation quality of `droidgfxsim`. It executes interactive, camera-navigated rendering sessions for 30+ seconds, captures time-series visual frames, and inspects them for defects.

## Overview & Capabilities

- **Automated Compilation**: Ensures binaries and asset paths are valid before test runs.
- **Renderer Verification**: Supports OpenGL 2 (`gl2`) and OpenGL 3 (`gl3`) renderers.
- **Dynamic Camera Trajectory**: Drives the camera via arrow keys / WASD and mouse look across diverse viewpoints (stacked physics cubes, ground plane, zoom, pitch sweeps).
- **Time-Series Captures**: Samples high-resolution frames every 2.5 seconds over 30+ seconds.
- **Automated Defect Diagnostics**: Analyzes luminance, contrast dynamic range, banding frequency, and inter-frame temporal motion to flag rendering defects.

---

## Running an Interactive Test

To run a 35-second visual test session on `app/src/main/assets/data.xml`:

```powershell
python .agents/skills/interactive-testing/scripts/interactive_visual_test.py --duration 35.0 --interval 2.5
```

### Script Arguments

| Flag | Default | Description |
|------|---------|-------------|
| `--binary` | `build/bin/Debug/desktop_app.exe` | Path to desktop executable |
| `--duration` | `35.0` | Test run duration in seconds (must be $\ge 30\text{s}$) |
| `--interval` | `2.5` | Sampling period in seconds between frame captures |
| `--output-dir` | Auto-generated | Path to directory where PNG frames and reports are saved |
| `--no-build` | False | Skips compilation phase before launching |

---

## Camera Trajectory Phases (35s Session)

1. **Phase 1 ($0\text{s} - 5\text{s}$)**: Scene load, asset loading (`test.obj`, `data.xml`), initial physics stabilization.
2. **Phase 2 ($5\text{s} - 10\text{s}$)**: Move forward towards the tower of falling physics cubes (`Up Arrow` / `Key W`).
3. **Phase 3 ($10\text{s} - 15\text{s}$)**: Pitch camera downward to inspect ground plane contact and cube collisions.
4. **Phase 4 ($15\text{s} - 20\text{s}$)**: Yaw right around the scene (`Right Arrow` / Mouse Right).
5. **Phase 5 ($20\text{s} - 25\text{s}$)**: Elevate camera viewpoint (`Key W` in fly mode).
6. **Phase 6 ($25\text{s} - 30\text{s}$)**: Continuous 360-degree yaw sweep to evaluate frustum culling and bounding sphere stability.
7. **Phase 7 ($30\text{s} - 35\text{s}$)**: Move backward for full simulation overview (`Down Arrow` / `Key S`).

---

## Output Artifacts & Diagnostics

Every test session produces:
- `frame_XXX_YY.Ys.png`: High-resolution visual snapshots at each time step.
- `visual_test_report.json`: Machine-readable metrics for each frame.
- `test_report.md`: Human-readable summary table with verdict (`PASS`, `WARN`, `FAIL`).

Refer to the [Visual Defect Catalog](./references/defect_catalog.md) for full descriptions of defect classifications and thresholds.
