# Production Playbook: droidgfxsim

## 1. Build & Release Engineering

### Desktop Application (Windows, Linux, macOS)
Requirements:
- CMake 3.28+
- Modern C++20 compiler (MSVC 2022/2026, Clang 16+, GCC 13+)

Configure and build:
```powershell
# Configure
cmake -S . -B build

# Build Debug
cmake --build build --config Debug

# Build Release
cmake --build build --config Release
```

Run desktop simulation:
```powershell
cd app/src/main/assets
../../../build/bin/Debug/desktop_app.exe
```

Run unit test suite:
```powershell
ctest --test-dir build -C Debug --output-on-failure
```

---

### Android NDK Application
Requirements:
- Android Studio Hedgehog / Iguana / Jellyfish (AGP 8.2+)
- Android NDK 26+
- JDK 17

Build via Gradle:
```powershell
./gradlew assembleDebug
```

Install to connected device / emulator:
```powershell
./gradlew installDebug
```

---

## 2. Automated Regression & Visual Verification

### Running Unit Test Harness
```powershell
cmake --build build --target run_tests
```

### Running Automated Interactive Visual Testing
```powershell
python .agents/skills/interactive-testing/scripts/interactive_visual_test.py --duration 35.0
```

Produces:
- `frame_XXX_YY.Ys.png` (time-series captures)
- `defect_analysis.json` (luminance, banding, and stability metrics)

---

## 3. Concurrent Agent Workstream Rules (Quick Reference)

- Lane L1: Build & Config (`CMakeLists.txt`, `build.gradle`)
- Lane L2: Rendering Backend (`gl2_renderer.*`, `gl3_renderer.*`, `gl_code.*`)
- Lane L3: Scene, Asset Pipeline & Physics (`scene_graph.*`, `wavefront_factory.*`, `simulation.*`, `camera.*`)
- Lane L4: Tests & Validation (`tests/`, `.agents/skills/interactive-testing/`)
- Lane L5: Documentation & Process (`README.md`, `BACKLOG.md`, `TECHNICAL_CRITICAL_REVIEW.md`)

Merge order: **L1 → L2 → L3 → L4 → L5**.
