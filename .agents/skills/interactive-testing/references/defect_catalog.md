# Visual Defect Catalog & Rubric for droidgfxsim

This catalog outlines the primary visual and simulation defects encountered in real-time 3D rendering and physics simulation in `droidgfxsim`, along with diagnostic signatures and automated detection criteria.

---

## 1. Blank / Black / White Screen (Pipeline Stalls or Clear-Only)
- **Visual Appearance**: Window displays a solid black, gray, or white frame with no geometry visible.
- **Root Cause**: Shaders failed to compile; matrix inversion failure in `Camera` or `TransformNode`; near/far clipping plane discarding all vertices; incorrect viewport configuration.
- **Metric Signature**: `max_luminance < 0.05` (Black) or `min_luminance > 0.95` (White).

---

## 2. Frustum Culling Pop-in / Disappearing Geometry
- **Visual Appearance**: Cubes or ground plane suddenly disappear when camera pans or rotates slightly away from world origin.
- **Root Cause**: Bounding sphere radius calculated via inaccurate centroid/AABB instead of Welzl minimal enclosing sphere; failure to transform bounding sphere into world space; incorrect sign in Gribb-Hartmann plane extraction.
- **Metric Signature**: Sudden drop in mean luminance or frame delta $> 0.40$ during smooth camera rotation.

---

## 3. Buffer Overrun / Memory Corruption Artifacts
- **Visual Appearance**: Exploding polygons, spikes shooting to infinity, flickering geometry triangles, driver crashes.
- **Root Cause**: Passing byte count (`sizeof(GLuint) * size`) instead of index count to `glDrawElements`, causing reads beyond allocated index memory buffer.
- **Metric Signature**: Extreme variance in luminance between adjacent frames or crash during frame rendering.

---

## 4. Inverted Matrix / Physics Drift
- **Visual Appearance**: Convex hull shapes fly away or deform into distorted shapes during physics simulation.
- **Root Cause**: Transposed vector-matrix multiplication (`v * M` instead of `M * v`) or recursive state mutation in convex hull shape generation.
- **Metric Signature**: Temporal delta collapses to 0 (physics freeze) or blows up to extreme frame-to-frame variance without user input.

---

## 5. Missing / Corrupted Texture Mapping
- **Visual Appearance**: Meshes render with default flat white or corrupted memory patterns instead of texture image.
- **Root Cause**: Failure to detect 1, 2, 3, or 4 channel textures; incorrect alignment in `stb_image` memory loading; unassigned material ID mapping.
- **Metric Signature**: Contrast dynamic range $< 0.10$ in textured scenes.
