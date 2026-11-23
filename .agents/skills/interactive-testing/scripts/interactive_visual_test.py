#!/usr/bin/env python3
"""
Interactive Visual Testing Framework for droidgfxsim
=====================================================
Executes an automated visual testing session on droidgfxsim.
Controls camera movement via virtual inputs, triggers dynamic interactive actions
(firing cannonballs into crates, switching presets, toggling wireframe),
captures visual frames over an extended duration (>=30 seconds),
evaluates images for visual defects, computes dynamic motion entropy,
and produces a structured test report with artifacts.
"""

import argparse
import ctypes
from ctypes import wintypes
import json
import math
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from PIL import Image
import numpy as np

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_CLOSE = 0x0010
VK_ESCAPE = 0x1B
VK_SPACE = 0x20
VK_UP = 0x26
VK_DOWN = 0x28
VK_LEFT = 0x25
VK_RIGHT = 0x27
VK_W = 0x57
VK_S = 0x53
VK_A = 0x41
VK_D = 0x44
VK_2 = 0x32
VK_F1 = 0x70
VK_RETURN = 0x0D

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ('biSize', wintypes.DWORD),
        ('biWidth', wintypes.LONG),
        ('biHeight', wintypes.LONG),
        ('biPlanes', wintypes.WORD),
        ('biBitCount', wintypes.WORD),
        ('biCompression', wintypes.DWORD),
        ('biSizeImage', wintypes.DWORD),
        ('biXPelsPerMeter', wintypes.LONG),
        ('biYPelsPerMeter', wintypes.LONG),
        ('biClrUsed', wintypes.DWORD),
        ('biClrImportant', wintypes.DWORD)
    ]

def find_app_window(pid: int, timeout: float = 15.0) -> wintypes.HWND:
    start = time.time()
    found_hwnd = None

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)

    def enum_cb(hwnd, lparam):
        nonlocal found_hwnd
        p = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid:
            length = user32.GetWindowTextLengthW(hwnd)
            buf = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buf, length + 1)
            title = buf.value.lower()
            if 'simulator' in title or 'droidgfx' in title:
                found_hwnd = hwnd
                return False
        return True

    cb = WNDENUMPROC(enum_cb)
    while time.time() - start < timeout:
        user32.EnumWindows(cb, 0)
        if found_hwnd and user32.IsWindowVisible(found_hwnd):
            return found_hwnd
        time.sleep(0.3)
    return None

def capture_window(hwnd: wintypes.HWND, output_path: Path) -> bool:
    rect = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    if w <= 0 or h <= 0:
        return False

    hdc_window = user32.GetDC(hwnd)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_window)
    hbm = gdi32.CreateCompatibleBitmap(hdc_window, w, h)
    gdi32.SelectObject(hdc_mem, hbm)

    # PrintWindow with PW_RENDERFULLCONTENT
    user32.PrintWindow(hwnd, hdc_mem, 2)

    bi = BITMAPINFOHEADER()
    bi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bi.biWidth = w
    bi.biHeight = -h  # top-down
    bi.biPlanes = 1
    bi.biBitCount = 32
    bi.biCompression = 0

    buf_size = w * h * 4
    buf = (ctypes.c_char * buf_size)()
    gdi32.GetDIBits(hdc_mem, hbm, 0, h, buf, ctypes.byref(bi), 0)

    gdi32.DeleteObject(hbm)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(hwnd, hdc_window)

    arr = np.frombuffer(buf, dtype=np.uint8).reshape((h, w, 4))
    rgb = arr[:, :, [2, 1, 0]]  # BGRA to RGB
    img = Image.fromarray(rgb)
    img.save(output_path)
    return True

def send_key(hwnd: wintypes.HWND, vk_code: int, duration: float = 0.05):
    scancode = user32.MapVirtualKeyW(vk_code, 0)
    lparam_down = 1 | (scancode << 16)
    lparam_up = 1 | (scancode << 16) | (1 << 30) | (1 << 31)
    user32.PostMessageW(hwnd, WM_KEYDOWN, vk_code, lparam_down)
    time.sleep(duration)
    user32.PostMessageW(hwnd, WM_KEYUP, vk_code, lparam_up)

def main():
    parser = argparse.ArgumentParser(description="Automated Interactive Visual Testing for droidgfxsim")
    parser.add_argument("--binary", default="build/bin/Debug/desktop_app.exe", help="Path to desktop executable")
    parser.add_argument("--duration", type=float, default=35.0, help="Test duration in seconds")
    parser.add_argument("--interval", type=float, default=2.5, help="Frame capture interval")
    parser.add_argument("--output-dir", default=None, help="Output directory")
    parser.add_argument("--no-build", action="store_true", help="Skip build step")
    parser.add_argument("--vulkan", action="store_true", help="Launch desktop_app with Vulkan renderer")
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[4]
    if args.output_dir:
        out_dir = Path(args.output_dir)
    else:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_dir = project_root / "artifacts" / f"visual_test_{timestamp}"
    out_dir.mkdir(parents=True, exist_ok=True)

    exe_path = project_root / args.binary
    if not exe_path.exists():
        candidates = [
            project_root / "build" / "native" / "Debug" / "desktop_app.exe",
            project_root / "build" / "Debug" / "desktop_app.exe",
            project_root / "build" / "desktop_app.exe"
        ]
        for c in candidates:
            if c.exists():
                exe_path = c
                break

    if not exe_path.exists():
        print(f"Error: Executable not found at {exe_path}. Build project first.")
        sys.exit(1)

    cmd = [str(exe_path)]
    if args.vulkan:
        cmd.append("--vulkan")

    print(f"Starting droidgfxsim with command: {' '.join(cmd)}...")
    assets_dir = project_root / "app" / "src" / "main" / "assets"
    proc = subprocess.Popen(cmd, cwd=str(assets_dir))

    try:
        hwnd = find_app_window(proc.pid, timeout=12.0)
        if not hwnd:
            print("Failed to locate application window.")
            proc.terminate()
            sys.exit(1)

        print(f"Found application window (HWND: {hwnd}). Running dynamic trajectory test ({args.duration}s)...")
        user32.SetForegroundWindow(hwnd)
        user32.SetFocus(hwnd)
        start_time = time.time()
        last_capture = 0.0
        frame_idx = 0
        triggered_actions = set()

        while time.time() - start_time < args.duration:
            elapsed = time.time() - start_time
            if elapsed - last_capture >= args.interval:
                frame_path = out_dir / f"frame_{frame_idx:03d}_{elapsed:.1f}s.png"
                if capture_window(hwnd, frame_path):
                    print(f"Captured frame {frame_idx:03d} at {elapsed:.1f}s -> {frame_path.name}")
                    frame_idx += 1
                last_capture = elapsed

            # Discrete dynamic actions at designated timeline events:
            # At t=3.0s: Cannonball shot #1 into crate stacks (VK_SPACE)
            if elapsed >= 3.0 and "cannon_1" not in triggered_actions:
                send_key(hwnd, VK_SPACE, 0.08)
                triggered_actions.add("cannon_1")
                print(f"[{elapsed:.1f}s] ACTION: Fired Spacebar cannonball #1 into crate stacks (VK_SPACE)")

            # At t=8.0s: Cannonball shot #2 into rolling ball cascade (VK_SPACE)
            if elapsed >= 8.0 and "cannon_2" not in triggered_actions:
                send_key(hwnd, VK_SPACE, 0.08)
                triggered_actions.add("cannon_2")
                print(f"[{elapsed:.1f}s] ACTION: Fired Spacebar cannonball #2 into rolling ball cascade (VK_SPACE)")

            # At t=15.0s: Cannonball shot #3 into debris field (VK_SPACE)
            if elapsed >= 15.0 and "cannon_3" not in triggered_actions:
                send_key(hwnd, VK_SPACE, 0.08)
                triggered_actions.add("cannon_3")
                print(f"[{elapsed:.1f}s] ACTION: Fired Spacebar cannonball #3 into debris field (VK_SPACE)")

            # At t=22.0s: Cannonball shot #4 into scattered debris field (VK_SPACE)
            if elapsed >= 22.0 and "cannon_4" not in triggered_actions:
                send_key(hwnd, VK_SPACE, 0.08)
                triggered_actions.add("cannon_4")
                print(f"[{elapsed:.1f}s] ACTION: Fired Spacebar cannonball #4 into scattered debris field (VK_SPACE)")

            # At t=28.0s: Cannonball shot #5 into secondary cascade (VK_SPACE)
            if elapsed >= 28.0 and "cannon_5" not in triggered_actions:
                send_key(hwnd, VK_SPACE, 0.08)
                triggered_actions.add("cannon_5")
                print(f"[{elapsed:.1f}s] ACTION: Fired Spacebar cannonball #5 into secondary cascade (VK_SPACE)")

            # Cinematic swooping camera navigation tracking kinetic cascade & debris:
            step_pulse = (int(elapsed * 10) % 3 == 0)
            if step_pulse:
                if 0.0 <= elapsed < 3.0:
                    # Swoop forward and track falling, bouncing balls descending ramps
                    send_key(hwnd, VK_UP, 0.04)
                    send_key(hwnd, VK_S, 0.04)
                elif 3.0 <= elapsed < 8.0:
                    # Zoom in on crate stacks impact and pan across crate explosion debris
                    send_key(hwnd, VK_UP, 0.04)
                    send_key(hwnd, VK_D, 0.05)  # Lateral pan across debris field
                elif 8.0 <= elapsed < 15.0:
                    # Crane ascent tracking rolling ball cascade & scattering spheres
                    send_key(hwnd, VK_W, 0.04)
                    send_key(hwnd, VK_LEFT, 0.05)  # Track flying spheres and cascade
                elif 15.0 <= elapsed < 22.0:
                    # Sweep around arena to observe continuous cascade scattering in debris field
                    send_key(hwnd, VK_RIGHT, 0.04)
                    send_key(hwnd, VK_UP, 0.04)
                elif 22.0 <= elapsed < 28.0:
                    # Orbiting inspection of new preset layout and dynamic cascade
                    send_key(hwnd, VK_LEFT, 0.05)
                    send_key(hwnd, VK_W, 0.04)
                elif 28.0 <= elapsed <= 35.0:
                    # Overview pull-back sweeping across entire arena to verify kinetic stability
                    send_key(hwnd, VK_DOWN, 0.04)
                    send_key(hwnd, VK_W, 0.04)

            time.sleep(0.08)

        print("Testing complete. Sending close signal...")
        send_key(hwnd, VK_ESCAPE, 0.1)
        time.sleep(1.0)
    finally:
        if proc.poll() is None:
            proc.terminate()
            proc.wait()

    # Analyze captured frames
    from analyze_captures import analyze_directory
    report = analyze_directory(out_dir)
    print("Interactive visual testing session finished.")

if __name__ == "__main__":
    main()
