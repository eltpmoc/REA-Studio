#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Local-only audit/source/Linux packaging. No Git/network publication commands."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.3.2-alpha"
SOURCE_NAME = f"REA-Studio-v{VERSION}-Source"
ROOT_FILES = {".gitignore", ".gitattributes", "CMakeLists.txt", "CMakePresets.json", "README.md", "CHANGELOG.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "COPYING.md"}
SOURCE_DIRS = {"src", "include", "tests", "fuzz", "docs", "third_party", "scripts", "packaging", "licenses", ".github"}
BAD_SUFFIXES = {".exe", ".dll", ".pdb", ".o", ".obj", ".a", ".lib", ".pyc", ".log", ".pem", ".key", ".pfx", ".p12"}
LICENSE_HASH = "3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986"


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def source_files():
    files = [ROOT / p for p in ROOT_FILES if (ROOT / p).is_file()]
    for directory in sorted(SOURCE_DIRS):
        for path in (ROOT / directory).rglob("*"):
            if "__pycache__" in path.parts:
                continue
            if path.is_symlink():
                raise RuntimeError(f"Source symlink refused: {path.relative_to(ROOT)}")
            if path.is_file():
                files.append(path)
    return sorted(files, key=lambda p: p.relative_to(ROOT).as_posix())


def audit():
    entries = []
    for line in (ROOT / "third_party/SHA256SUMS").read_text().splitlines():
        expected, name = line.split("  ", 1)
        path = ROOT / name
        if not re.fullmatch(r"[0-9a-f]{64}", expected) or not path.resolve().is_relative_to(ROOT / "third_party/zydis"):
            raise RuntimeError("Invalid dependency manifest entry")
        if path.is_symlink() or digest(path) != expected:
            raise RuntimeError(f"Dependency integrity failed: {name}")
        entries.append(name)
    actual = {p.relative_to(ROOT).as_posix() for p in (ROOT / "third_party/zydis").rglob("*") if p.is_file()}
    if len(entries) != 107 or len(set(entries)) != 107 or actual != set(entries):
        raise RuntimeError("Expected exactly the 107 vendored files, without extras")
    if digest(ROOT / "LICENSE") != LICENSE_HASH:
        raise RuntimeError("Canonical GPL-3 license changed; review the license gate")
    for name in ("third_party/zydis/LICENSE", "third_party/zydis/dependencies/zycore/LICENSE"):
        if "Permission is hereby granted" not in (ROOT / name).read_text():
            raise RuntimeError("Missing MIT license")
    if not (ROOT / "licenses/LGPL-3.0.txt").is_file():
        raise RuntimeError("Missing LGPL text")
    files = source_files()
    for path in files:
        rel = path.relative_to(ROOT)
        if path.is_symlink():
            raise RuntimeError(f"Source symlink refused: {rel}")
        if path.suffix.lower() in BAD_SUFFIXES or any(p in {".git", ".aws", ".codex", ".agents", ".env"} for p in rel.parts):
            raise RuntimeError(f"Forbidden publication file: {rel}")
        if path.name.startswith(".env"):
            raise RuntimeError(f"Environment file refused: {rel}")
        if path.suffix.lower() not in {".png"}:
            data = path.read_bytes()
            if re.search(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|(?:gh[pousr]_[A-Za-z0-9]{30,})|AKIA[0-9A-Z]{16}", data):
                raise RuntimeError(f"Potential credential in {rel}; inspect privately")
    # A staged file bypasses ignore rules, so inspect the index separately.
    if (ROOT / ".git").exists():
        result = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, check=True, capture_output=True)
        allowed = {p.relative_to(ROOT).as_posix() for p in files}
        for name in result.stdout.decode().split("\0"):
            if name and name not in allowed:
                raise RuntimeError(f"Indexed path outside source allowlist: {name}")
    print(f"PASS integrity/licenses/publication inventory: 107 dependencies; {len(files)} source files")
    return files


def add_file(archive, path, name):
    info = archive.gettarinfo(str(path), arcname=name)
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    info.mtime = 0
    info.mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
    with path.open("rb") as stream:
        archive.addfile(info, stream)


def write_tar(destination, paths, base):
    with destination.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as zipped:
            with tarfile.open(fileobj=zipped, mode="w") as archive:
                for path, relative in paths:
                    add_file(archive, path, f"{base}/{relative}")


def source_package(output):
    files = audit()
    output.mkdir(parents=True, exist_ok=True)
    destination = output / f"{SOURCE_NAME}.tar.gz"
    write_tar(destination, [(p, p.relative_to(ROOT).as_posix()) for p in files], SOURCE_NAME)
    (output / "SOURCE-MANIFEST.txt").write_text("".join(f"{digest(p)}  {p.relative_to(ROOT).as_posix()}\n" for p in files), encoding="utf-8")
    print(destination)
    return destination


def checksums(output):
    paths = sorted(p for p in output.iterdir() if p.is_file() and (p.name.endswith((".tar.gz", ".zip", ".tar.xz")) or p.name == "RELEASE-NOTES.md"))
    if not paths:
        raise RuntimeError("No artifacts to checksum")
    (output / "SHA256SUMS.txt").write_text("".join(f"{digest(p)}  {p.name}\n" for p in paths), encoding="utf-8")


def linux_package(build, output):
    if platform.system() != "Linux" or platform.machine().lower() not in {"x86_64", "amd64"}:
        raise RuntimeError("Linux x86_64 packaging requires a real matching Linux host")
    cache = (build / "CMakeCache.txt").read_text()
    if "CMAKE_BUILD_TYPE:STRING=Release" not in cache:
        raise RuntimeError("Use a Release build")
    subprocess.run(["ctest", "--test-dir", str(build), "--output-on-failure", "--output-junit", "release-tests.xml"], check=True)
    source = source_package(output)
    base = f"REA-Studio-v{VERSION}-Linux-x86_64"
    with tempfile.TemporaryDirectory(prefix="package-", dir=build) as temporary:
        stage = Path(temporary) / base
        subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage)], check=True)
        shared = stage / "share/rea-studio"
        (shared / "sources").mkdir()
        shutil.copy2(source, shared / "sources" / source.name)
        if (ROOT / "packaging/RELEASE-NOTES.md").is_file():
            shutil.copy2(ROOT / "packaging/RELEASE-NOTES.md", shared / "RELEASE-NOTES.md")
        executable = stage / "bin/rea-studio"
        # Only our freshly built program is passed to ldd, never an analyzed input.
        deps = subprocess.run(["ldd", str(executable)], check=True, capture_output=True, text=True).stdout
        if "not found" in deps:
            raise RuntimeError("Unresolved distribution dependencies")
        (shared / "DEPENDENCIES.txt").write_text(platform.platform()+"\n"+deps, encoding="utf-8")
        env = os.environ.copy()
        env["QT_QPA_PLATFORM"] = "offscreen"
        for key in ("QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "LD_LIBRARY_PATH"):
            env.pop(key, None)
        subprocess.run([str(executable), "--smoke", str(build / "smoke.elf")], env=env, check=True, timeout=30)
        paths = [(p, p.relative_to(stage).as_posix()) for p in sorted(stage.rglob("*")) if p.is_file()]
        write_tar(output / f"{base}.tar.gz", paths, base)
    shutil.copy2(ROOT / "packaging/RELEASE-NOTES.md", output / "RELEASE-NOTES.md")
    checksums(output)
    print(f"PASS Linux install/dependencies/offscreen smoke: {output / (base+'.tar.gz')}")


def qt_notices(archive_path, destination):
    config = json.loads((ROOT / "packaging/qtbase.json").read_text())
    if digest(archive_path) != config["sha256"]:
        raise RuntimeError("Qt source SHA-256 mismatch")
    prefix = f"qtbase-everywhere-src-{config['version']}/"
    count = 0
    with tarfile.open(archive_path) as archive:
        for member in archive:
            if not member.isfile() or not member.name.startswith(prefix):
                continue
            relative = Path(member.name[len(prefix):])
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError("Invalid Qt source member")
            if relative.parts[0] == "LICENSES" or relative.name.startswith(("LICENSE", "COPYING", "COPYRIGHT")) or relative.name == "qt_attribution.json":
                target = destination / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as stream:
                    target.write_bytes(stream.read())
                count += 1
    if count < 10 or not (destination / "LICENSES/LGPL-3.0-only.txt").is_file():
        raise RuntimeError("Qt license/attribution extraction incomplete")
    print(f"PASS {count} Qt license/attribution files")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["audit", "source", "linux", "checksums", "qt-notices"])
    parser.add_argument("--build", type=Path, default=ROOT / "build-release")
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--qt-source", type=Path)
    args = parser.parse_args()
    if args.action == "audit":
        audit()
    elif args.action == "source":
        source_package(args.output.resolve())
        checksums(args.output.resolve())
    elif args.action == "linux":
        linux_package(args.build.resolve(), args.output.resolve())
    elif args.action == "qt-notices":
        qt_notices(args.qt_source, args.output.resolve())
    else:
        checksums(args.output.resolve())


if __name__ == "__main__":
    main()
