#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Download the reviewed QtBase source prerequisite; never a binary or publication."""
import json
from pathlib import Path
import sys
import urllib.request
from release import ROOT, digest

config = json.loads((ROOT / "packaging/qtbase.json").read_text())
destination = Path(sys.argv[1])
destination.mkdir(parents=True, exist_ok=True)
archive = destination / config["filename"]
if not archive.exists():
    with urllib.request.urlopen(config["url"], timeout=120) as response, archive.open("wb") as output:
        while block := response.read(1024 * 1024):
            output.write(block)
if digest(archive) != config["sha256"]:
    raise SystemExit("Qt source SHA-256 mismatch; discard the local download and investigate")
print(archive.resolve())
