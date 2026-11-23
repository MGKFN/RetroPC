# Retro PC

An retro PC emulator with some cool features: Tech Haven Studios' old builds on emulated PCs, for Windows and Linux.

Retro PC started inside [HavenDOS](https://github.com/MGKFN/HavenDOS) (Apps > Retro PC) to run the studio's oldest surviving builds. It is also a program of its own, and it is growing into a full PC emulator that boots real installation CDs. [ROADMAP.md](ROADMAP.md) has the steps.

## What it runs today (0.1)

Two emulated PCs, both text mode:

- **an 8086** (with the 80186 and 386 16-bit additions), a PC BIOS and a floppy: **K-DOS Beta 1.1b** (admin / 1234), and any boot floppy written for a PC BIOS;
- **a 32-bit 386** that loads a Multiboot kernel from a CD image's GRUB menu, the way GRUB does: **BOOT OS 0.3.1, 0.5.0 and 0.5.2** (bootos / 1234).

The four builds are built in. Your own `.IMG` and `.ISO` files go in the `RETRO` folder beside the program.

Not yet: graphics modes, a real BIOS boot for the 386, hard disks and CD drives, 64-bit. Those are the roadmap. Windows and Linux installation CDs do not boot yet.

## Download

[Releases](https://github.com/MGKFN/RetroPC/releases):

| File | For |
|---|---|
| `RetroPC-<ver>-windows.zip` | Windows: unzip, run `RetroPC.exe`. Nothing to install. Windows warns the first time because it is not signed: More info, then Run anyway. |
| `RetroPC-<ver>-linux-x86_64.tar.gz` | Linux: `./retropc` (X11, or XWayland) and `./retropc-headless` |

## Using it

| In the list | |
|---|---|
| Up / Down, Enter | choose, boot |
| O | open a file |
| R | look in `RETRO` again |
| Q, Esc | quit |

| Inside an old PC | |
|---|---|
| F4 | back to the list (quits if you started it on a file) |
| Alt+Enter | full screen |

```
RetroPC game.img          boot a file
RetroPC --boot KDOS11B    boot a built-in image (KDOS11B, BOOTOS31, BOOTOS50, BOOTOS52)
RetroPC --mem 64          the 386's memory in MB (16)
```

On Windows you can also drop a file on the window or on `RetroPC.exe`.

**Hard disks** are files: `RETRO/<ID>.HDD` for a built-in image, `<name>.HDD` beside a file you opened. They are the same files HavenDOS keeps, so they can be copied either way. Nothing ever reaches a real disk, and a floppy image is never written back.

## Building

```
make            # build/retropc and build/retropc-headless   (gcc, libx11-dev)
make windows    # build/RetroPC.exe                          (gcc-mingw-w64-x86-64)
make dist       # the two downloads in dist/
make test       # the suites                                 (also gcc-multilib, xorriso)
```

## The tree

| Path | What it is |
|---|---|
| `emu/` | The emulated PCs: `cpu86.c` + `pcbios.c` (the 8086 PC), `cpu386.c` + `pc386.c` (the 386 PC). **Shared with the HavenDOS kernel**, which copies them from here, so they stay freestanding: no C library, no libgcc. `tools/check_freestanding.py` enforces it. |
| `include/` | Their headers, `keys.h` (HavenDOS key codes) and the 8x16 font |
| `src/` | The program: `retropc.c`, the glyphs (`font.c`), and one host per platform: `host_win32.c`, `host_x11.c`, `host_null.c` (no window: scripted keys, a virtual clock) |
| `images/` | The built-in builds, each with a SHA-256 in `IMAGES.txt` |
| `tests/` | `test_emu86.c`, `test_emu386.c`, `test_retropc.py`; `emu86_sst.c` with `tools/check_emu86_sst.py` runs the SingleStepTests 8088 suite (dev only, downloads it) |

## Licence

MIT, see [LICENSE](LICENSE). Retro PC ships no Microsoft software. To boot a Windows CD you need your own copy and licence.
