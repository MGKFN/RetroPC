#!/usr/bin/env python3
"""check_freestanding.py -- the emulators still fit inside a kernel.

emu/*.c is compiled twice: here, into the Retro PC program, and inside
the HavenDOS kernel (Apps > Retro PC), which copies them from this repo
with its tools/sync_emu.py. The kernel is -m32 -ffreestanding with no C
library and no libgcc. So an emulator that calls printf, or divides two
64-bit numbers on a 32-bit build (a hidden call to __udivdi3), works
perfectly here and stops HavenDOS from linking.

This builds emu/*.c the kernel's way and refuses any outside symbol
that is not on the list below -- the few the kernel provides.

    python3 tools/check_freestanding.py
"""
import glob, os, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# what HavenDOS's kernel/string.c provides and the emulators may use
ALLOWED = {"memcpy", "memset", "memmove", "memcmp"}


def main():
    tmp = tempfile.mkdtemp(prefix="rpfs_")
    objs, defined, undefined = [], set(), set()
    for src in sorted(glob.glob(os.path.join(ROOT, "emu", "*.c"))):
        o = os.path.join(tmp, os.path.basename(src)[:-2] + ".o")
        r = subprocess.run(["gcc", "-m32", "-O2", "-std=gnu99", "-ffreestanding", "-fno-builtin",
                            "-fno-pie", "-fno-stack-protector", "-Wall",
                            "-iquote", os.path.join(ROOT, "include"), "-c", src, "-o", o],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"check_freestanding: {os.path.relpath(src, ROOT)} does not build the kernel's way:\n{r.stderr}")
        objs.append(o)
    for o in objs:
        for ln in subprocess.run(["nm", o], capture_output=True, text=True).stdout.splitlines():
            parts = ln.split()
            if len(parts) == 2 and parts[0] == "U":
                undefined.add(parts[1])
            elif len(parts) == 3 and parts[1] in "TDBRC":
                defined.add(parts[2])
    outside = sorted(undefined - defined - ALLOWED)
    if outside:
        sys.exit("check_freestanding: emu/ calls what a kernel does not have: " + ", ".join(outside) +
                 "\n  (a 64-bit divide on -m32 is __udivdi3/__umoddi3; use the CPU's own helper)")
    print(f"check_freestanding: ok, emu/ builds -m32 -ffreestanding and needs only "
          f"{', '.join(sorted(undefined - defined))}")


main()
