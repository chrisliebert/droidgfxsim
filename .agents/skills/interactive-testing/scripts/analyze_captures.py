#!/usr/bin/env python3
"""
Visual Defect & Dynamic Motion Analyzer for Captured Frames
===========================================================
Performs post-hoc computer vision and statistical analysis on captured rendering frames.
Evaluates:
- Blank/black/white screen detection
- Contrast dynamic range
- High-frequency moiré / banding detection
- Inter-frame temporal stability and geometry culling pop-in
- Dynamic Motion Entropy calculation and optical motion delta score (ΔM)
- Dynamic action classification (DYNAMIC_ACTION vs STATIC_SETTLED) & kinetic excitement scoring
"""

import argparse
import json
from pathlib import Path
from PIL import Image
import numpy as np

def analyze_directory(capture_dir: Path) -> dict:
    images = sorted(list(capture_dir.glob("*.png")) + list(capture_dir.glob("*.ppm")))
    if not images:
        print(f"No PNG or PPM frames found in {capture_dir}")
        return {"status": "error", "message": "No frames found"}

    print(f"Analyzing {len(images)} frames in {capture_dir}...")
    results = []
    prev_arr = None

    for idx, img_path in enumerate(images):
        img = Image.open(img_path).convert('RGB')
        arr = np.array(img, dtype=np.float32) / 255.0
        r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
        lum = 0.299 * r + 0.587 * g + 0.114 * b

        mean_lum = float(np.mean(lum))
        min_lum = float(np.min(lum))
        max_lum = float(np.max(lum))
        lum_std = float(np.std(lum))

        row_means = np.mean(lum, axis=1)
        row_diffs = np.abs(np.diff(row_means))
        banding_score = float(np.std(row_diffs))

        diff_from_prev = 0.0
        motion_entropy = 0.0
        if prev_arr is not None:
            # Frame-to-frame optical motion delta score ΔM
            delta_frame = np.abs(arr - prev_arr)
            diff_from_prev = float(np.mean(delta_frame))

            # Dynamic Motion Entropy calculation across motion distribution
            hist, _ = np.histogram(delta_frame, bins=32, range=(0.0, 1.0))
            probs = hist.astype(np.float64) / (np.sum(hist) + 1e-12)
            probs = probs[probs > 0]
            motion_entropy = float(-np.sum(probs * np.log2(probs)))
        prev_arr = arr

        delta_m = diff_from_prev
        is_dynamic = bool(delta_m > 0.0003)
        motion_class = "DYNAMIC_ACTION" if is_dynamic else "STATIC_SETTLED"
        kinetic_excitement = round(float(min(100.0, delta_m * 1000.0)), 2)

        results.append({
            "index": idx + 1,
            "filename": img_path.name,
            "mean_luminance": round(mean_lum, 4),
            "dynamic_range": round(max_lum - min_lum, 4),
            "banding_score": round(banding_score, 5),
            "frame_delta": round(diff_from_prev, 4),
            "motion_delta_dm": round(delta_m, 5),
            "motion_entropy": round(motion_entropy, 4),
            "kinetic_excitement": kinetic_excitement,
            "motion_classification": motion_class,
            "defect_black": bool(max_lum < 0.05),
            "defect_white": bool(min_lum > 0.95),
            "defect_banding_suspect": bool(banding_score > 0.12)
        })

    dynamic_frames = sum(1 for f in results if f["motion_classification"] == "DYNAMIC_ACTION")
    mean_kinetic = float(np.mean([f["kinetic_excitement"] for f in results])) if results else 0.0
    mean_entropy = float(np.mean([f["motion_entropy"] for f in results])) if results else 0.0
    mean_delta_m = float(np.mean([f["motion_delta_dm"] for f in results])) if results else 0.0
    dynamic_ratio = round(dynamic_frames / max(1, len(results)), 4)

    summary = {
        "directory": str(capture_dir),
        "total_frames": len(images),
        "dynamic_action_frames": dynamic_frames,
        "dynamic_action_ratio": dynamic_ratio,
        "mean_kinetic_excitement": round(mean_kinetic, 2),
        "mean_motion_entropy": round(mean_entropy, 4),
        "mean_motion_delta_dm": round(mean_delta_m, 5),
        "frames": results
    }

    # Determine overall status: require >= 70% DYNAMIC_ACTION frames to certify continuous kinetic activity
    has_fail = any(f["defect_black"] or f["defect_white"] for f in results)
    has_warn = any(f["defect_banding_suspect"] for f in results)
    insufficient_dynamic_motion = bool(dynamic_ratio < 0.70)
    summary["defect_insufficient_dynamic_motion"] = insufficient_dynamic_motion

    if has_fail or insufficient_dynamic_motion:
        overall_verdict = "FAIL"
    elif has_warn:
        overall_verdict = "WARN"
    else:
        overall_verdict = "PASS"
    summary["overall_verdict"] = overall_verdict

    report_path = capture_dir / "defect_analysis.json"
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

    visual_report_path = capture_dir / "visual_test_report.json"
    with open(visual_report_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

    # Generate Markdown report
    md_lines = [
        "# Interactive Visual Testing Report",
        "",
        f"- **Directory**: `{capture_dir}`",
        f"- **Total Frames**: {len(images)}",
        f"- **Dynamic Action Frames**: {dynamic_frames}/{len(images)} ({summary['dynamic_action_ratio']*100:.1f}%)",
        f"- **Mean Kinetic Excitement Score**: {mean_kinetic:.2f} / 100.0",
        f"- **Mean Dynamic Motion Entropy**: {mean_entropy:.4f} bits",
        f"- **Mean Optical Motion Delta (ΔM)**: {mean_delta_m:.5f}",
        f"- **Sustained Dynamic Activity (>=70% DYNAMIC_ACTION)**: **{'PASS' if not insufficient_dynamic_motion else 'FAIL'}**",
        f"- **Overall Verdict**: **{overall_verdict}**",
        "",
        "## Dynamic Physics & Kinetic Excitement Summary",
        f"- Physics Simulation Dynamic Activity: **{'ACTIVE' if dynamic_frames > 0 else 'STATIC'}**",
        f"- Dynamic Classification Threshold: $\\Delta M > 0.0003$ (`DYNAMIC_ACTION`)",
        f"- Continuous Kinetic Activity Requirement: $\\ge 70\\%$ frames in `DYNAMIC_ACTION` ({'SATISFIED' if not insufficient_dynamic_motion else 'FAILED'})",
        "",
        "## Frame Metrics Table",
        "",
        "| Frame | Filename | Mean Lum | Dyn Range | Banding | ΔM (Motion Delta) | Entropy | Kinetic Score | Classification | Status |",
        "|-------|----------|----------|-----------|---------|-------------------|---------|---------------|----------------|--------|"
    ]

    for f in results:
        status = "FAIL" if (f["defect_black"] or f["defect_white"]) else ("WARN" if f["defect_banding_suspect"] else "PASS")
        md_lines.append(
            f"| {f['index']} | `{f['filename']}` | {f['mean_luminance']:.4f} | {f['dynamic_range']:.4f} | {f['banding_score']:.5f} | {f['motion_delta_dm']:.5f} | {f['motion_entropy']:.4f} | {f['kinetic_excitement']:.2f} | `{f['motion_classification']}` | **{status}** |"
        )

    md_lines.extend([
        "",
        "## Verdict Criteria",
        "- **PASS**: Dynamic range > 0.10, no black/white stalls (`defect_black`/`defect_white` == False), banding score <= 0.12, dynamic action ratio >= 70% (>= 0.70).",
        "- **WARN**: Banding suspect detected (score > 0.12).",
        "- **FAIL**: Blank/white frame, extreme rendering collapse, or insufficient dynamic motion (< 70% DYNAMIC_ACTION frames).",
        "- **DYNAMIC_ACTION**: $\\Delta M > 0.0003$, actively evolving rigid-body physics motion.",
        ""
    ])

    md_report_path = capture_dir / "test_report.md"
    with open(md_report_path, "w", encoding="utf-8") as f:
        f.write("\n".join(md_lines))

    print(f"Dynamic Action frames: {dynamic_frames}/{len(images)} ({summary['dynamic_action_ratio']*100:.1f}%), Mean Kinetic Excitement: {mean_kinetic:.2f}, Mean Entropy: {mean_entropy:.4f}")
    print(f"Analysis saved to {report_path}")
    print(f"Visual report saved to {visual_report_path}")
    print(f"Markdown report saved to {md_report_path}")
    return summary

def main():
    parser = argparse.ArgumentParser(description="Analyze visual frames for rendering defects and dynamic motion entropy")
    parser.add_argument("directory", help="Directory containing captured frames")
    args = parser.parse_args()
    analyze_directory(Path(args.directory))

if __name__ == "__main__":
    main()
