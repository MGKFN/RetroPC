# Retro PC roadmap

Kyan, 23 November 2026: "make it as its own repo also add 64 bit 32 bit and all other stuff... i would like to also use ISOS like windows". He chose to go **from old to new, step by step**: each step boots something real that the step before could not, and is tested against it before the next one starts.

**1.0 is the first version that boots a real installation CD and installs from it.** Everything before is a pre-release.

## Why the order matters

Today's two PCs each skip something a real PC has. The 8086 has a BIOS but only an 8086. The 386 has a 32-bit CPU but no BIOS: it does GRUB's job itself and jumps straight into a Multiboot kernel. A Windows CD needs both at once: a 386-or-later CPU that **starts in real mode** with a BIOS, boots the CD, and then switches itself into protected mode. So step one is to make them one PC.

Every instruction is interpreted, at about 70 million a second on a laptop. That is plenty for DOS, Windows 3.1, 95 and 98, and probably enough for XP. Windows 10 and 11 would take hours to boot that way, so they need the host CPU's own virtualization (step 8).

## The steps

| Version | What it adds | What it should then boot |
|---|---|---|
| **0.1** (now) | The 8086 PC with a floppy; the 386 Multiboot loader; text mode; Windows and Linux programs | K-DOS Beta 1.1b, BOOT OS 0.3.1 / 0.5.0 / 0.5.2 |
| **0.2** One PC | The 386 starts in real mode at F000:FFF0 with the BIOS from the 8086 PC: real mode, protected mode and the switch between them in one CPU. INT 13h floppy and hard-disk images. | MS-DOS and FreeDOS boot floppies; K-DOS on the 386 |
| **0.3** Disks | IDE hard disks (raw `.img` files), an ATAPI CD drive, El Torito CD boot from the BIOS | FreeDOS install CD; **HavenDOS's own ISO through its real GRUB** |
| **0.4** Graphics | VGA (mode 13h, the planar 16-colour modes), VBE with a linear framebuffer, the PS/2 mouse, the PC speaker | Windows 3.1; BOOT OS's graphics entries; **HavenDOS's desktop inside Retro PC** (and so HavenDOS inside HavenDOS) |
| **0.5** A 486 | Paging, the x87 FPU, rings 0-3, the TSS and task switches, virtual-8086 mode | 32-bit Linux; Windows 95 |
| **0.6** A Pentium | CPUID, MMX, TSC, a PCI bus with a PIIX IDE controller, a Sound Blaster 16, an RTL8139 network card | Windows 98 SE; Windows 2000 |
| **1.0** | Whatever Windows XP's setup turns out to need (ACPI tables, an APIC), and speed | **A Windows XP installation CD, installed onto a Retro PC hard disk** |
| **1.x** 64-bit | Long mode, SSE and SSE2, NX | 64-bit K-DOS (multiboot2); 64-bit Linux |
| **2.0** At full speed | A second engine that runs the guest on the host CPU itself: Windows Hypervisor Platform on Windows, KVM on Linux. The emulated devices stay; the CPU becomes the real one. UEFI firmware, AHCI or NVMe disks. | Windows 10 and 11 |

Each step ends with:

- a test in `make test` that boots the thing in the right-hand column, headless, and checks its screen;
- `tools/check_freestanding.py` still passing, so HavenDOS can take the new `emu/`;
- a release.

## What stays the same

- **Nothing reaches a real disk.** Every disk is a file the program owns.
- **A failure explains itself on screen.** An instruction the CPU does not have, a device that is not there, or a mode that cannot be shown gets a sentence, never a silent hang.
- **No Microsoft software in the repo or the downloads.** You bring your own CD and licence.

## HavenDOS

The HavenDOS kernel runs `emu/` too (Apps > Retro PC). It copies the files from this repo with its own `tools/sync_emu.py`. A HavenDOS release that takes a new `emu/` says which Retro PC version it came from.
