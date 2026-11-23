# Retro PC -- Tech Haven Studios' old builds on emulated PCs.
#
#   make            build/retropc (Linux, X11) and build/retropc-headless
#   make windows    build/RetroPC.exe              (needs gcc-mingw-w64-x86-64)
#   make dist       both, and dist/RetroPC-<ver>-windows.zip, -linux-x86_64.tar.gz
#   make test       the suites (needs gcc, python3, xorriso)
#
# emu/ is shared with the HavenDOS kernel, which copies it from here
# (HavenDOS tools/sync_emu.py). It must stay freestanding: `make test`
# runs tools/check_freestanding.py first.
#
# -iquote, not -I, for include/: headers here are included with quotes,
# and the C library's <string.h> must stay the C library's.

CC      = gcc
MINGW   = x86_64-w64-mingw32
CFLAGS  = -O2 -std=gnu99 -Wall -Wextra -Wno-format-truncation -Wno-builtin-declaration-mismatch \
          -iquote include -iquote src -iquote build
EMU     = emu/cpu86.c emu/pcbios.c emu/cpu386.c emu/pc386.c
CORE    = src/retropc.c src/font.c $(EMU)
HDRS    = src/retropc.h src/version.h include/emu86.h include/emu386.h include/pcbios.h \
          include/pc386.h include/keys.h include/types.h include/font8x16.h build/retro_images.h

all: build/retropc build/retropc-headless

build/retro_images.h: images/IMAGES.txt $(wildcard images/*.img images/*.elf images/*.cfg) tools/make_retro_images.py
	@mkdir -p build
	python3 tools/make_retro_images.py $@

build/retropc-headless: $(CORE) $(HDRS) src/host_null.c src/plat_posix.c
	$(CC) $(CFLAGS) -o $@ $(CORE) src/host_null.c src/plat_posix.c

build/retropc: $(CORE) $(HDRS) src/host_x11.c src/plat_posix.c
	$(CC) $(CFLAGS) -o $@ $(CORE) src/host_x11.c src/plat_posix.c -lX11

build/RetroPC.exe: $(CORE) $(HDRS) src/host_win32.c tools/make_retropc_res.py
	python3 tools/make_retropc_res.py build/res
	$(MINGW)-windres build/res/retropc.rc -O coff -o build/res/retropc.res.o
	$(MINGW)-gcc $(CFLAGS) -mwindows -static -o $@ $(CORE) src/host_win32.c \
	    build/res/retropc.res.o -lcomdlg32 -lwinmm
	@# only Windows' own DLLs: a program that needs a runtime installed
	@# first is a program half its users cannot start
	@$(MINGW)-objdump -p $@ | awk '/DLL Name/ {print tolower($$3)}' | sort | \
	    grep -vxE 'kernel32.dll|user32.dll|gdi32.dll|shell32.dll|comdlg32.dll|winmm.dll|msvcrt.dll' \
	    && { echo "RetroPC.exe imports a DLL Windows does not ship"; rm -f $@; exit 1; } || true

windows: build/RetroPC.exe

dist: build/retropc build/retropc-headless build/RetroPC.exe src/README.txt tools/make_retropc_dist.py
	python3 tools/make_retropc_dist.py

test: build/retro_images.h
	@python3 tools/check_freestanding.py
	@echo "── test_emu86 ─────────────────────────────"
	@echo "  the 8086/80186, its BIOS, and K-DOS against QEMU's screens and disk"
	@$(CC) -std=gnu99 -g -fsanitize=address,undefined -iquote include -iquote build \
	       -Wno-builtin-declaration-mismatch -o /tmp/rp_temu tests/test_emu86.c emu/cpu86.c emu/pcbios.c && \
	  python3 tools/run_suite.py --name test_emu86 --min 86 -- /tmp/rp_temu
	@echo "── test_emu386 ────────────────────────────"
	@echo "  the 386: its ALU against this CPU, programs, BOOT OS booted, the keyboard's bytes"
	@$(CC) -std=gnu99 -O2 -iquote include -iquote build -Wall -Wno-builtin-declaration-mismatch \
	       -o /tmp/rp_t386 tests/test_emu386.c && \
	  python3 tools/run_suite.py --name test_emu386 --min 36 -- /tmp/rp_t386
	@echo "── test_fuzz ──────────────────────────────"
	@echo "  a user's CD, kernel or code cannot crash Retro PC: the 0.1.1 fixes, and bounded fuzzing"
	@$(CC) -std=gnu99 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -iquote include -iquote build \
	       -Wno-builtin-declaration-mismatch -o /tmp/rp_tfuzz tests/test_fuzz.c && \
	  python3 tools/run_suite.py --name test_fuzz --min 12 -- /tmp/rp_tfuzz
	@echo "── test_retropc ───────────────────────────"
	@echo "  the program: K-DOS and BOOT OS through it, files, the font"
	@$(CC) $(CFLAGS) -o /tmp/rp_trp $(CORE) src/host_null.c src/plat_posix.c && \
	 $(CC) -O1 -g -fsanitize=address,undefined $(filter-out -O2,$(CFLAGS)) -o /tmp/rp_trp_asan \
	       $(CORE) src/host_null.c src/plat_posix.c && \
	  python3 tools/run_suite.py --name test_retropc --min 46 -- python3 tests/test_retropc.py /tmp/rp_trp /tmp/rp_trp_asan

clean:
	rm -rf build dist

.PHONY: all windows dist test clean
