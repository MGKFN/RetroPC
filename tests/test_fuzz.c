/* tests/test_fuzz.c -- what a user's file cannot do to Retro PC.  0.1.1.

   Everything Retro PC reads comes from a file somebody else made: a
   CD image's directories and grub.cfg, the kernel on it, the code that
   kernel runs. None of that may crash the program -- or, since emu/
   runs inside it, the HavenDOS kernel. Fuzzing found two ways it could
   (23 Nov 2026), and this suite holds the line:

     1. pc386_iso_read() trusted a directory record's name length, so a
        name longer than its record read past the end of the image.
     2. pc386_boot_kernel() trusted the kernel's sizes: a program header
        whose file size was bigger than its memory size made a memset
        clear nearly 4 GB, and offsets near 2^32 wrapped past the checks.

   Part 1 builds each of those exactly. Part 2 is a bounded mutation
   fuzz of the same three readers and of both CPUs on random code, from
   a fixed seed so a failure repeats. Built with ASAN and UBSAN by the
   Makefile: an out-of-bounds read IS the failure, whatever it returns.

   Longer runs, for after a change to these readers:
       /tmp/rp_tfuzz 50     -- fifty times the default iterations      */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../emu/cpu386.c"
#include "../emu/pc386.c"
#include "retro_images.h"

static int total, fails;
static void ck(const char *what, int ok, long got){
    total++;
    if(ok){ if(getenv("V")) printf("  ok    %s\n", what); }
    else { printf("  FAIL  %s   got %ld\n", what, got); fails++; }
}

static uint64_t g_s = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void){ g_s ^= g_s << 13; g_s ^= g_s >> 7; g_s ^= g_s << 17; return (uint32_t)g_s; }

static uint32_t T;
static uint32_t now(void *c){ (void)c; return T += 3; }
static void rtc(void *c, int *y, int *mo, int *d, int *h, int *mi, int *s){ (void)c; *y = 2026; *mo = 1; *d = 1; *h = *mi = *s = 0; }
static const pc386_host_t H = { now, rtc };

/* ── a small CD: /BOOT/GRUB/GRUB.CFG, made here ────────────────────── */
#define SEC 2048u
static void put32(uint8_t *b, uint32_t v){ b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }
static int dirrec(uint8_t *b, uint32_t lba, uint32_t len, int dir, const char *name){
    int nl = (int)strlen(name), rl = 33 + nl + ((33 + nl) & 1);
    memset(b, 0, (size_t)rl);
    b[0] = (uint8_t)rl; put32(b + 2, lba); put32(b + 10, len);
    b[25] = dir ? 2 : 0; b[32] = (uint8_t)nl; memcpy(b + 33, name, (size_t)nl);
    return rl;
}
static const char CFG[] =
    "menuentry \"BOOT OS [Text Desktop]\" {\n  multiboot /boot/bootos.elf mode=desktop gfx=text\n  boot\n}\n"
    "menuentry \"BOOT OS [Safe Mode]\" {\n  multiboot /boot/bootos.elf mode=safe gfx=text\n  boot\n}\n";
static uint32_t make_cd(uint8_t *iso){
    uint32_t size = 22 * SEC;
    uint8_t *p;
    memset(iso, 0, size);
    p = iso + 16 * SEC; p[0] = 1; memcpy(p + 1, "CD001", 5); p[6] = 1;
    dirrec(p + 156, 18, SEC, 1, "\0");                     /* root: LBA 18 */
    p = iso + 18 * SEC; p += dirrec(p, 18, SEC, 1, "\0"); p += dirrec(p, 18, SEC, 1, "\1");
    dirrec(p, 19, SEC, 1, "BOOT");
    p = iso + 19 * SEC; p += dirrec(p, 19, SEC, 1, "\0"); p += dirrec(p, 18, SEC, 1, "\1");
    dirrec(p, 20, SEC, 1, "GRUB");
    p = iso + 20 * SEC; p += dirrec(p, 20, SEC, 1, "\0"); p += dirrec(p, 19, SEC, 1, "\1");
    dirrec(p, 21, (uint32_t)strlen(CFG), 0, "GRUB.CFG;1");
    memcpy(iso + 21 * SEC, CFG, strlen(CFG));
    return size;
}

static void part1(void){
    static uint8_t cd[22 * SEC];
    pc386_entry_t e[PC386_ENTRY_MAX]; const char *why = 0;
    uint32_t size = make_cd(cd);
    int n = pc386_iso_menu(cd, size, e, PC386_ENTRY_MAX, &why);
    ck("the CD made here reads: two entries off /boot/grub/grub.cfg", n == 2 && !strcmp(e[0].path, "/boot/bootos.elf"), n);

    /* 1. a name longer than its record, in a record at the very end of
       the image: the image is cut 40 bytes into the root directory,
       and the record there claims a 200-byte name. Copied into a buffer
       of exactly that size, so ASAN sees any read past it. */
    {
        uint32_t cut = 18 * SEC + 40;
        uint8_t *b = malloc(cut);
        const uint8_t *data;
        memcpy(b, cd, cut);
        b[18 * SEC + 0] = 34; b[18 * SEC + 32] = 200;      /* first record: 34 bytes, name 200 */
        n = pc386_iso_read(b, cut, "/boot/grub/grub.cfg", &data);
        ck("a directory record whose name runs past the image: refused, nothing read past it", n < 0, n);
        free(b);
    }
    /* and inside the image, a name longer than its record */
    {
        uint8_t *b = malloc(size);
        const uint8_t *data;
        memcpy(b, cd, size);
        b[18 * SEC + 34 + 34 + 32] = 255;                  /* BOOT's record: name 255, record 38 */
        n = pc386_iso_read(b, size, "/boot/grub/grub.cfg", &data);
        ck("a directory record whose name is longer than the record: not matched", n < 0, n);
        free(b);
    }
    /* an LBA that wraps 32 bits when multiplied by 2048 */
    {
        uint8_t *b = malloc(size);
        const uint8_t *data;
        memcpy(b, cd, size);
        put32(b + 16 * SEC + 156 + 2, 0x00200000u + 18u);  /* 0x200012 * 2048 == 18*2048 + 2^32 */
        n = pc386_iso_read(b, size, "/boot/grub/grub.cfg", &data);
        ck("a root LBA that wraps in 32 bits is not read as a small one", n < 0, n);
        free(b);
    }

    /* 2. the kernel loader */
    {
        static pc386_t p;
        uint8_t *mem = malloc(4u << 20);
        const retro_image_t *im = &retro_images[1];          /* BOOT OS 0.3.1, an ELF */
        uint8_t *k = malloc(im->size);
        pc386_entry_t en; memset(&en, 0, sizeof en); strcpy(en.path, "/boot/k.elf");
        uint32_t phoff = le32(im->data + 28);
        int r;

        pc386_init(&p, mem, 4u << 20, &H, 0);
        ck("BOOT OS 0.3.1 loads as it is", pc386_boot_kernel(&p, im->data, (int)im->size, &en) == 0, 0);

        memcpy(k, im->data, im->size);
        for(uint32_t i = 0; i < le16(k + 44); i++){         /* every PT_LOAD: file bigger than memory */
            uint8_t *ph = k + phoff + i * le16(k + 42);
            if(le32(ph) == 1) put32(ph + 20, le32(ph + 16) ? le32(ph + 16) - 1 : 0);
        }
        pc386_init(&p, mem, 4u << 20, &H, 0);
        r = pc386_boot_kernel(&p, k, (int)im->size, &en);
        ck("a program header with file size > memory size: refused (0.1.0 cleared ~4 GB)",
           r != 0 && strstr(p.notice, "damaged"), r);

        memcpy(k, im->data, im->size);
        for(uint32_t i = 0; i < le16(k + 44); i++){         /* offset + size wraps past 2^32 */
            uint8_t *ph = k + phoff + i * le16(k + 42);
            if(le32(ph) == 1){ put32(ph + 4, 0xFFFFF000u); put32(ph + 16, 0x2000u); put32(ph + 20, 0x2000u); }
        }
        pc386_init(&p, mem, 4u << 20, &H, 0);
        r = pc386_boot_kernel(&p, k, (int)im->size, &en);
        ck("a segment whose offset + size wraps 32 bits: refused, nothing copied", r != 0, r);

        memcpy(k, im->data, im->size);
        put32(k + 28, 0xFFFFFFF0u);                          /* program headers past the end */
        pc386_init(&p, mem, 4u << 20, &H, 0);
        pc386_boot_kernel(&p, k, (int)im->size, &en);
        ck("program headers placed past 2^32: nothing read out of the file", 1, 0);

        /* the a.out kludge, with its header the last 16 bytes of the file */
        {
            uint8_t a[64]; memset(a, 0, sizeof a);
            put32(a + 48, 0x1BADB002u); put32(a + 52, 0x10000u); put32(a + 56, (uint32_t)-(int32_t)(0x1BADB002u + 0x10000u));
            pc386_init(&p, mem, 4u << 20, &H, 0);
            r = pc386_boot_kernel(&p, a, 64, &en);
            ck("an a.out header cut short by the end of the file: refused", r != 0 && strstr(p.notice, "cut short"), r);
        }
        free(k); free(mem);
    }
}

/* ── part 2: bounded fuzzing ─────────────────────────────────────── */
static void mutate(uint8_t *b, uint32_t *n, uint32_t cap, uint32_t hot){
    int k = 1 + (int)(rnd() % 8);
    while(k--){
        uint32_t at = *n ? ((rnd() & 3) && hot ? rnd() % (hot < *n ? hot : *n) : rnd() % *n) : 0;
        switch(rnd() % 5){
        case 0: if(*n) b[at] ^= (uint8_t)(1u << (rnd() % 8)); break;
        case 1: if(*n) b[at] = (uint8_t[]){0, 1, 0x7F, 0x80, 0xFF, 0x22, 0x0A, 0x20}[rnd() % 8]; break;
        case 2: if(*n) b[at] = (uint8_t)rnd(); break;
        case 3: if(*n) *n = rnd() % (*n + 1); break;
        case 4: if(*n < cap){ memmove(b + at + 1, b + at, *n - at); b[at] = (uint8_t)rnd(); (*n)++; } break;
        }
    }
}

static void part2(int scale){
    static uint8_t cd[22 * SEC], buf[22 * SEC + 64];
    pc386_entry_t e[PC386_ENTRY_MAX]; const char *why;
    uint32_t size = make_cd(cd);
    int bad = 0;
    for(long i = 0; i < 20000L * scale; i++){
        uint32_t n = size;
        memcpy(buf, cd, size);
        /* mostly in the volume descriptor and the directories */
        mutate(buf, &n, sizeof buf, 0);
        if(rnd() & 1) for(int j = 0; j < 4; j++) buf[16 * SEC + rnd() % (6 * SEC)] = (uint8_t)rnd();
        uint8_t *h = malloc(n ? n : 1); memcpy(h, buf, n);
        int k = pc386_iso_menu(h, n, e, PC386_ENTRY_MAX, &why);
        if(k > PC386_ENTRY_MAX) bad++;
        for(int j = 0; j < k; j++)
            if(!memchr(e[j].title, 0, sizeof e[j].title) || !memchr(e[j].path, 0, sizeof e[j].path)) bad++;
        k = pc386_cfg_menu((const char *)h, (int)n, e, PC386_ENTRY_MAX, &why);
        if(k > PC386_ENTRY_MAX) bad++;
        free(h);
    }
    ck("20000 mutated CDs and grub.cfgs: no crash, every entry NUL-terminated", bad == 0, bad);

    {
        static pc386_t p;
        uint8_t *mem = malloc(4u << 20);
        const retro_image_t *im = &retro_images[1];
        uint8_t *k = malloc(im->size + 64);
        pc386_entry_t en; memset(&en, 0, sizeof en); strcpy(en.path, "/boot/k.elf");
        long loads = 0;
        for(long i = 0; i < 1500L * scale; i++){
            uint32_t n = im->size;
            memcpy(k, im->data, n);
            mutate(k, &n, im->size + 64, 4096);          /* mostly the headers */
            uint8_t *h = malloc(n ? n : 1); memcpy(h, k, n);
            pc386_init(&p, mem, 4u << 20, &H, 0);
            if(pc386_boot_kernel(&p, h, (int)n, &en) == 0){
                loads++;
                for(int r = 0; r < 2; r++){ int x = pc386_run(&p, 5000); if(x == X86_FAULT || x == X86_HALT || x == PC386_STOPPED) break; }
            }
            free(h);
        }
        ck("1500 mutated kernels loaded and run: no crash", 1, loads);

        for(long i = 0; i < 150L * scale; i++){
            pc386_init(&p, mem, 4u << 20, &H, 0);
            pc386_boot_kernel(&p, im->data, (int)im->size, &en);
            for(int j = 0; j < 4096; j++) mem[(p.entry + (uint32_t)j) & ((4u << 20) - 1)] = (uint8_t)rnd();
            for(int r = 0; r < 4; r++){ int x = pc386_run(&p, 20000); if(x == X86_FAULT || x == X86_HALT || x == PC386_STOPPED) break; }
        }
        ck("150 runs of 4 KB of random code on the 386: no crash", 1, 0);
        free(k); free(mem);
    }
}

int main(int argc, char **argv){
    int scale = argc > 1 ? atoi(argv[1]) : 1;
    if(scale < 1) scale = 1;
    printf("test_fuzz -- what a user's CD, kernel or code cannot do to Retro PC\n");
    part1();
    part2(scale);
    printf("\n%d/%d passed\n", total - fails, total);
    return fails ? 1 : 0;
}
