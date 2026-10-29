#!/usr/bin/env python3
"""Create deterministic 12-second media and a hash/codec manifest using host FFmpeg."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--ffprobe", default="ffprobe")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    ffmpeg = shutil.which(args.ffmpeg)
    if not ffmpeg:
        raise SystemExit("FFmpeg with libx264 and libmp3lame is required")
    def generate(name, options):
        subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-threads", "1", *options, str(args.output / name)], check=True)
    audio = ["-f", "lavfi", "-i"]
    generate("audio.wav", audio + ["sine=frequency=440:sample_rate=44100:duration=12", "-ac", "1", "-c:a", "pcm_s16le"])
    generate("audio.mp3", audio + ["sine=frequency=660:sample_rate=44100:duration=12", "-ac", "1", "-c:a", "libmp3lame", "-b:a", "96k"])
    generate("video.mp4", ["-f", "lavfi", "-i", "testsrc2=size=320x180:rate=15:duration=12", "-f", "lavfi", "-i", "sine=frequency=880:sample_rate=44100:duration=12",
        "-c:v", "libx264", "-threads:v", "1", "-profile:v", "baseline", "-level:v", "3.0", "-pix_fmt", "yuv420p", "-b:v", "384k", "-g", "15",
        "-c:a", "aac", "-b:a", "96k", "-ac", "1", "-movflags", "+faststart", "-shortest"])
    generate("video-tail-index.mp4", ["-i", str(args.output / "video.mp4"), "-c", "copy"])
    (args.output / "unsupported.mp4").write_bytes(b"This is deliberately not a media container.\n")
    manifest = {"version": 1, "durationSeconds": 12, "audioFrequenciesHz": {"audio.wav": 440, "audio.mp3": 660, "video.mp4": 880}, "files": {}}
    for path in sorted(args.output.iterdir()):
        data = path.read_bytes()
        item = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        if path.name != "unsupported.mp4":
            probe = subprocess.run([args.ffprobe, "-v", "error", "-show_streams", "-show_format", "-of", "json", str(path)], capture_output=True, text=True, check=True)
            value = json.loads(probe.stdout)
            item["streams"] = [{key: stream[key] for key in ("codec_name", "profile", "codec_type", "width", "height", "sample_rate", "channels") if key in stream} for stream in value["streams"]]
            item["duration"] = value["format"]["duration"]
        manifest["files"][path.name] = item
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
