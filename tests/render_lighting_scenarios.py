#!/usr/bin/env python3
"""Render independent lighting cases and validate their images and replay sidecars."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess


def read_json(path):
    return json.loads(path.read_text())


def capture(binary, root, arguments, log_path):
    with log_path.open("w") as log:
        result = subprocess.run([str(binary), *arguments], cwd=root, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Capture failed ({result.returncode}): {log_path}\n{log_path.read_text()[-3000:]}")


def run(args):
    root = Path(__file__).resolve().parent.parent
    manifest_path = args.manifest.resolve()
    manifest = read_json(manifest_path)
    base = read_json(manifest_path.parent / manifest["base_config"])
    cases = [read_json(manifest_path.parent / name) for name in manifest["cases"]]
    if args.case:
        cases = [case for case in cases if case["name"] == args.case]
        if not cases:
            raise ValueError(f"Unknown case: {args.case}")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    results = {}
    names = set()
    for case in cases:
        name = case["name"]
        if not re.fullmatch(r"[a-z0-9_]+", name) or name in names:
            raise ValueError(f"Case names must be unique filename-safe identifiers: {name}")
        names.add(name)
        scene = copy.deepcopy(base)
        scene["scenario_name"] = name
        for index, overrides in case.get("planet_overrides", {}).items():
            scene["planets"][int(index)].update(overrides)
        if case.get("look_at_sun", False):
            scene["surface_camera"].pop("direction_ned", None)
            scene["surface_camera"].pop("up_ned", None)
        scene["surface_camera"].update(case.get("surface_camera", {}))
        scene["surface_camera"]["simulation_time_seconds"] = case["simulation_time_seconds"]
        scene["lighting"].update(case.get("lighting", {}))
        scene["skybox"].update(case.get("skybox", {}))
        resolved = output / f"{name}.config.json"
        resolved.write_text(json.dumps(scene, indent=2) + "\n")
        image = output / f"{name}.png"
        width, height = manifest["render_size"]
        capture(args.binary.resolve(), root, ["--config", str(resolved), "--surface-capture", str(image),
                "--render-size", str(width), str(height)], output / f"{name}.log")
        metadata_path = Path(str(image) + ".json")
        metadata = read_json(metadata_path)
        metrics = metadata["render"]
        assert metadata["surface_camera"]["simulation_time_seconds"] == case["simulation_time_seconds"], name
        assert metrics["width"] == width and metrics["height"] == height, name
        assert metrics["terrain_pixels"] > width * height * 0.05, f"{name}: missing terrain geometry"
        assert metrics["sky_pixels"] > width * height * 0.05, f"{name}: missing sky"
        assert metrics["sky_interior_pixels"] > width * height * 0.05, f"{name}: missing sky away from body edges"
        for key, (low, high) in case["expected"].items():
            if not low <= metrics[key] <= high:
                raise AssertionError(f"{name}: {key}={metrics[key]} outside [{low}, {high}]; see {metadata_path}")
        digest = hashlib.sha256(image.read_bytes()).hexdigest()
        if case.get("verify_replay", False):
            replay = output / f"{name}.replay.png"
            # No dependency on the base file or the working config. Resolution,
            # camera, timestamp, lighting and terrain are restored from the sidecar.
            capture(args.binary.resolve(), root, ["--config", str(output / "unused-missing-config.json"),
                "--replay", str(metadata_path), "--surface-capture", str(replay)], output / f"{name}.replay.log")
            replay_metadata = read_json(Path(str(replay) + ".json"))
            assert replay_metadata["render"] == metrics, f"{name}: replay metrics differ"
            assert hashlib.sha256(replay.read_bytes()).hexdigest() == digest, f"{name}: replay PNG differs"
            snippet = output / f"{name}.camera.json"
            snippet.write_text(json.dumps(metadata["surface_camera"]) + "\n")
            console_replay = output / f"{name}.console-replay.png"
            capture(args.binary.resolve(), root, ["--config", str(resolved), "--replay", str(snippet),
                "--surface-capture", str(console_replay), "--render-size", str(width), str(height)],
                output / f"{name}.console-replay.log")
            assert hashlib.sha256(console_replay.read_bytes()).hexdigest() == digest, f"{name}: console snippet replay differs"
        results[name] = {"sha256": digest, **metrics}
        print(f"{name}: t={case['simulation_time_seconds']:g}s, exposure={metrics['exposure']:.3f}, "
              f"terrain luminance={metrics['terrain_mean_display_luminance']:.4f}, "
              f"terrain pixels={metrics['terrain_pixels']}", flush=True)
    if {"night_moon", "night_no_moon", "night_manual_exposure"} <= results.keys():
        lit = results["night_moon"]["terrain_mean_display_luminance"]
        assert results["night_moon"]["terrain_pixels"] == results["night_no_moon"]["terrain_pixels"]
        assert lit > results["night_no_moon"]["terrain_mean_display_luminance"] + 0.05
        assert lit > 4 * results["night_manual_exposure"]["terrain_mean_display_luminance"] + 0.04
    if {"daylight", "daylight_no_moon"} <= results.keys():
        assert results["daylight"]["terrain_pixels"] == results["daylight_no_moon"]["terrain_pixels"]
        assert abs(results["daylight"]["terrain_mean_display_luminance"] -
                   results["daylight_no_moon"]["terrain_mean_display_luminance"]) < 0.05
    if {"daylight", "night_moon"} <= results.keys():
        assert results["daylight"]["sky_interior_mean_display_luminance"] < 0.0001
        assert results["night_moon"]["sky_interior_mean_display_luminance"] > 10 * results["daylight"]["sky_interior_mean_display_luminance"]
    for comparison in manifest.get("comparisons", []):
        left_case, left_metric = comparison["left"]
        right_case, right_metric = comparison["right"]
        if left_case not in results or right_case not in results:
            continue
        left = results[left_case][left_metric]
        right = results[right_case][right_metric] * comparison.get("scale", 1.0)
        operator = comparison["operator"]
        if operator not in ("<", ">"):
            raise ValueError(f"Unsupported comparison: {operator}")
        assert (left < right if operator == "<" else left > right), (
            f"{left_case}.{left_metric}={left} must be {operator} {right_case}.{right_metric} * "
            f"{comparison.get('scale', 1.0)}={right}")
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"Validated {len(results)} lighting scenarios; artifacts: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).parent / "scenarios/lighting/manifest.json")
    parser.add_argument("--output-dir", type=Path, default=Path("build/lighting-scenarios"))
    parser.add_argument("--case", help="Render one named case instead of the full manifest")
    run(parser.parse_args())
