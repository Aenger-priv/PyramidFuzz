#!/usr/bin/env python3

import argparse
import base64
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path


DEFAULT_MODEL = "gpt-image-1"
ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PROMPT_FILE = ROOT / "assets" / "prompts" / "pyramid_fuzz_art_prompt.txt"
DEFAULT_OUTPUT = ROOT / "assets" / "generated" / "pyramid-fuzz-panel.png"
DEFAULT_METADATA = ROOT / "assets" / "generated" / "pyramid-fuzz-panel.json"


def load_prompt(prompt_arg: str | None) -> str:
    if prompt_arg:
        prompt_path = Path(prompt_arg).expanduser().resolve()
    else:
        prompt_path = DEFAULT_PROMPT_FILE

    return prompt_path.read_text(encoding="utf-8").strip()


def build_request(prompt: str, size: str, background: str, moderation: str) -> bytes:
    payload = {
        "model": DEFAULT_MODEL,
        "prompt": prompt,
        "size": size,
        "background": background,
        "moderation": moderation,
    }
    return json.dumps(payload).encode("utf-8")


def generate_image(api_key: str, payload: bytes) -> dict:
    request = urllib.request.Request(
        "https://api.openai.com/v1/images/generations",
        data=payload,
        method="POST",
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        },
    )

    with urllib.request.urlopen(request, timeout=300) as response:
        return json.loads(response.read().decode("utf-8"))


def save_outputs(response_json: dict, prompt: str, output_path: Path, metadata_path: Path) -> None:
    image_b64 = response_json["data"][0]["b64_json"]
    image_bytes = base64.b64decode(image_b64)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    metadata_path.parent.mkdir(parents=True, exist_ok=True)

    output_path.write_bytes(image_bytes)

    metadata = {
        "model": DEFAULT_MODEL,
        "prompt": prompt,
        "revised_prompt": response_json["data"][0].get("revised_prompt"),
        "output_file": str(output_path.relative_to(ROOT)),
    }
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate Pyramid Fuzz pedal artwork with the OpenAI Images API.")
    parser.add_argument("--prompt-file", help="Path to a UTF-8 prompt file.")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT), help="Output PNG path.")
    parser.add_argument("--metadata", default=str(DEFAULT_METADATA), help="Output JSON metadata path.")
    parser.add_argument("--size", default="1536x1024", help="Image size to request from the API.")
    parser.add_argument("--background", default="transparent", choices=["transparent", "opaque", "auto"])
    parser.add_argument("--moderation", default="low", choices=["low", "auto"])
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    api_key = os.environ.get("OPENAI_API_KEY")

    if not api_key:
        print("OPENAI_API_KEY is not set.", file=sys.stderr)
        return 1

    prompt = load_prompt(args.prompt_file)
    output_path = Path(args.output).expanduser().resolve()
    metadata_path = Path(args.metadata).expanduser().resolve()

    try:
        response_json = generate_image(
            api_key,
            build_request(prompt, args.size, args.background, args.moderation),
        )
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        print(body, file=sys.stderr)
        return exc.code or 1
    except urllib.error.URLError as exc:
        print(f"Network error: {exc}", file=sys.stderr)
        return 1

    save_outputs(response_json, prompt, output_path, metadata_path)
    print(f"Saved image to {output_path}")
    print(f"Saved metadata to {metadata_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
