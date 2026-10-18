#!/usr/bin/env python3
"""Sign the deployed macOS bundle from its nested code outwards."""

import argparse
import os
from pathlib import Path
import plistlib
import shutil
import subprocess


def sign_bundle(app, identity, keychain=None):
    contents = app / "Contents"
    with (contents / "Info.plist").open("rb") as stream:
        executable = contents / "MacOS" / plistlib.load(stream)["CFBundleExecutable"]

    # MacOS is reserved for code. Keep the frontend's existing resource paths.
    for name in ("patch", "resources", "scripts", "compat", "panic.json"):
        source = contents / "MacOS" / name
        if source.exists() and not source.is_symlink():
            destination = contents / "Resources" / name
            shutil.move(str(source), str(destination))
            source.symlink_to("../Resources/" + name)

    command = ["codesign", "--force", "--sign", identity, "--options", "runtime"]
    command += ["--timestamp=none" if identity == "-" else "--timestamp"]
    if keychain:
        command += ["--keychain", keychain]

    magic = {bytes.fromhex(value) for value in (
        "feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca",
        "cafebabf", "bfbafeca",
    )}
    for directory, _, files in os.walk(contents, topdown=False, followlinks=False):
        directory = Path(directory)
        for name in sorted(files):
            path = directory / name
            if path.is_symlink() or path == executable:
                continue
            with path.open("rb") as stream:
                is_macho = stream.read(4) in magic
            if is_macho:
                subprocess.run(command + [str(path)], check=True)
        if directory.suffix in (".framework", ".app", ".xpc", ".bundle"):
            subprocess.run(command + [str(directory)], check=True)

    entitlements = Path(__file__).resolve().parents[1] / "src/emu/qt/EKA2L1.entitlements"
    subprocess.run(command + ["--entitlements", str(entitlements), str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("identity")
    parser.add_argument("--keychain")
    args = parser.parse_args()
    sign_bundle(args.app.resolve(), args.identity, args.keychain)
