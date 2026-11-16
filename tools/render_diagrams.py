#!/usr/bin/env python3
"""Render the architecture Mermaid sources; independent of the plugin build."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
DIAGRAMS = ROOT / "docs" / "diagrams"
NAMES = ("components", "signal-flow", "audio-callback")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mmdc", default="mmdc", help="Mermaid CLI executable (rendered with 12.2.0)")
    parser.add_argument("--browser", type=Path, help="Optional Chrome/Chromium executable; uses a temporary profile")
    parser.add_argument("--png-dir", type=Path, help="Also render PNG previews into this directory")
    args = parser.parse_args()
    renderer = shutil.which(args.mmdc)
    if renderer is None:
        parser.error("Mermaid CLI was not found; see docs/diagrams/README.md for setup")
    if args.browser and not args.browser.is_file():
        parser.error(f"Browser executable not found: {args.browser}")
    if args.png_dir:
        args.png_dir.mkdir(parents=True, exist_ok=True)

    version = subprocess.check_output([renderer, "--version"], text=True).strip()
    print(f"Rendering architecture diagrams with Mermaid CLI {version}", flush=True)
    with tempfile.TemporaryDirectory(prefix="pyramid-mermaid-") as temporary:
        options = ["--configFile", str(DIAGRAMS / "mermaid-config.json"),
                   "--backgroundColor", "white", "--no-font-embed", "--quiet"]
        if args.browser:
            config = Path(temporary) / "puppeteer.json"
            config.write_text(json.dumps({"executablePath": str(args.browser.resolve()),
                                          "userDataDir": str(Path(temporary) / "profile")}), encoding="utf-8")
            options += ["--puppeteerConfigFile", str(config)]
        for name in NAMES:
            command = [renderer, *options, "--input", str(DIAGRAMS / f"{name}.mmd"),
                       "--svgId", f"pyramid-{name}"]
            subprocess.run([*command, "--output", str(DIAGRAMS / f"{name}.svg")], check=True)
            if args.png_dir:
                subprocess.run([*command, "--size", "2400", "--output",
                                str(args.png_dir / f"{name}.png")], check=True)
            print(f"Rendered {name}", flush=True)


if __name__ == "__main__":
    main()
