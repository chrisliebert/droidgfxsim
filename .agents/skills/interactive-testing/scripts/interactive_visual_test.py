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
    extended_flag = (1 << 24) if vk_code in (VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT) else 0
    lparam_down = 1 | (scancode << 16) | extended_flag
    lparam_up = 1 | (scancode << 16) | extended_flag | (1 << 30) | (1 << 31)
    user32.PostMessageW(hwnd, WM_KEYDOWN, vk_code, lparam_down)
    time.sleep(duration)
    user32.PostMessageW(hwnd, WM_KEYUP, vk_code, lparam_up)

def unfocus_imgui(hwnd: wintypes.HWND):
    rect = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    cx = max(500, int((rect.right - rect.left) * 0.75))
    cy = max(200, int((rect.bottom - rect.top) * 0.50))
    lparam = ((cy & 0xFFFF) << 16) | (cx & 0xFFFF)
    user32.PostMessageW(hwnd, 0x0201, 0x0001, lparam)  # WM_LBUTTONDOWN
    time.sleep(0.03)
    user32.PostMessageW(hwnd, 0x0202, 0, lparam)       # WM_LBUTTONUP
    time.sleep(0.05)

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
    stdout_log_path = out_dir / "app_stdout.log"
    stdout_file = open(stdout_log_path, "w", encoding="utf-8")
    proc = subprocess.Popen(cmd, cwd=str(assets_dir), stdout=stdout_file, stderr=subprocess.STDOUT)

    try:
        hwnd = find_app_window(proc.pid, timeout=12.0)
        if not hwnd:
            print("Failed to locate application window.")
            proc.terminate()
            stdout_file.close()
            sys.exit(1)

        print(f"Found application window (HWND: {hwnd}). Running dynamic trajectory test ({args.duration}s)...")
        user32.SetForegroundWindow(hwnd)
        user32.SetFocus(hwnd)
        unfocus_imgui(hwnd)
        start_time = time.time()
        last_capture = 0.0
        last_nav = -1.0
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

            # One-shot Spacebar cannonball shots into cascading 3D models & crates:
            if elapsed >= 24.0 and "cannon_1" not in triggered_actions:
                triggered_actions.add("cannon_1")
                print(f"[{elapsed:.1f}s] Firing Spacebar cannonball #1 into cascading 3D models & crates")
                send_key(hwnd, VK_SPACE, 0.08)

            if elapsed >= 26.5 and "cannon_2" not in triggered_actions:
                triggered_actions.add("cannon_2")
                print(f"[{elapsed:.1f}s] Firing Spacebar cannonball #2 into cascading 3D models & crates")
                send_key(hwnd, VK_SPACE, 0.08)

            # 5-phase 35s calibrated camera trajectory (initial camera at (0, 6.2, 30.0) looking -Z):
            # Phase 1 (0-7s): Move close to pad & lower y -> 2.2m to inspect ball & prop ground contacts
            # Phase 2 (7-15s): Pan left (VK_LEFT, VK_W) -> inspect hillside undulations, Boulder_00 & Torus_00 on Ramp_Left
            # Phase 3 (15-23s): Pan right (VK_RIGHT) -> inspect Ramp_Right, Crystal, Glider, Rook & right hills
            # Phase 4 (23-29s): Re-center & fire Spacebar cannonballs (24.0s & 26.5s) into cascading 3D models & crates
            # Phase 5 (29-35s): Pull back & elevate -> wide terrain overview of rolling hills & continuous 3D model stream
            if elapsed - last_nav >= 0.45:
                last_nav = elapsed
                if 0.0 <= elapsed < 7.0:
                    if elapsed < 2.2:
                        # 4 steps: y 6.2 -> 2.2, z 30 -> 26
                        send_key(hwnd, VK_S, 0.03)
                        send_key(hwnd, VK_UP, 0.03)
                    elif elapsed < 4.5:
                        # 5 steps: z 26 -> 21 close-up on ground pad contacts
                        send_key(hwnd, VK_UP, 0.03)
                    else:
                        # Subtle lateral close-up sweep across resting balls, crates, and rooks
                        send_key(hwnd, VK_D, 0.03)
                elif 7.0 <= elapsed < 15.0:
                    if elapsed < 10.6:
                        # Elevate y 2.2 -> ~9.2, yaw left (+0.1 rad/step -> +0.7 rad) toward left hills & Ramp_Left
                        send_key(hwnd, VK_W, 0.03)
                        send_key(hwnd, VK_LEFT, 0.03)
                    else:
                        # Pull slightly back & strafe left to frame rolling hills + Prop_Boulder_00 + Ramp_Left
                        send_key(hwnd, VK_DOWN, 0.03)
                        send_key(hwnd, VK_A, 0.03)
                elif 15.0 <= elapsed < 23.0:
                    if elapsed < 21.0:
                        # Pan smoothly right across center toward Ramp_Right, Crystal, Glider, Rook & right hills
                        send_key(hwnd, VK_RIGHT, 0.03)
                        send_key(hwnd, VK_D, 0.03)
                    else:
                        send_key(hwnd, VK_UP, 0.03)
                elif 23.0 <= elapsed < 29.0:
                    if elapsed < 24.0:
                        # Swing aim back toward center arena before cannonball #1
                        send_key(hwnd, VK_LEFT, 0.03)
                        send_key(hwnd, VK_A, 0.03)
                    elif elapsed < 26.4:
                        send_key(hwnd, VK_LEFT, 0.03)
                        send_key(hwnd, VK_S, 0.02)
                    else:
                        send_key(hwnd, VK_DOWN, 0.03)
                elif 29.0 <= elapsed <= 35.0:
                    # Pull back to wide terrain overview inspecting rolling hills & continuous mixed stream
                    if elapsed < 32.5:
                        send_key(hwnd, VK_DOWN, 0.03)
                        send_key(hwnd, VK_DOWN, 0.03)
                        send_key(hwnd, VK_W, 0.02)
                    else:
                        send_key(hwnd, VK_DOWN, 0.03)

            time.sleep(0.05)

        print("Testing complete. Sending close signal...")
        send_key(hwnd, VK_ESCAPE, 0.1)
        time.sleep(1.0)
    finally:
        if proc.poll() is None:
            proc.terminate()
            proc.wait()
        stdout_file.close()

    # Parse stdout log for floor tunneling defect detection
    tunnel_events = 0
    if stdout_log_path.exists():
        with open(stdout_log_path, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if "Floor tunneling detected:" in line:
                    import re
                    m = re.search(r"Floor tunneling detected:\s*(\d+)", line)
                    if m:
                        tunnel_events += int(m.group(1))
                    else:
                        tunnel_events += 1

    print(f"Floor tunneling containment check: {'PASS (0 defects)' if tunnel_events == 0 else f'FAIL ({tunnel_events} defects)'}")

    # Analyze captured frames
    from analyze_captures import analyze_directory
    report = analyze_directory(out_dir, floor_tunneling_count=tunnel_events)
    print(f"Output directory: {out_dir}")
    print(f"Overall verdict: {report.get('overall_verdict', 'UNKNOWN')}")
    print("Interactive visual testing session finished.")
    if report.get("overall_verdict") == "FAIL":
        sys.exit(1)

if __name__ == "__main__":
    main()
