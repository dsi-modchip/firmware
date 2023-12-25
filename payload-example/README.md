# payload

## Introduction

Both payloads exploit stack overflows in the bootroms: after skipping the RSA
signature validation check by exploiting a small oversight in RSA padding
checks and an instruction particularly vulnerable to a fault attack, the
bootroms will think the bootheader information (containing eg. load addresses
and sizes) is valid, and will attempt at loading the bootroms.

Normally, in such a situation, the SHA1 hash verification checks will fail (due
to them not matching the ones in the payload), so every core needs to be taken
over *before* that gets a chance of happening. However, as the bootroms trust
the load addresses etc. completely (as these are normally verified through a
SHA1 hash in the RSA signature, but that's glitched over), we can simply choose
the addresses and sizes of what would normally be the stage2 payload. One
obvious choice for this would be the stack of the respective cores, and that's
what we'll do.

The bootroms have multiple mechanisms for loading a stage2 payload into memory.
After configuring the NWRAM mappings, they will either:
* Have the ARM7 load the payload directly to the correct NWRAM bank, but mapped
  to the ARM7 instead of the correct core.
* Have the ARM7 first load the payload into NWRAM, then decompress it using the
  LZ77 decompression SWI routine (8-bit write with callback read).
* Have the ARM7 load the LZ77-compressed payload, but then use the FIFO to
  send it to the ARM9, which will then be the one performing the decompression.

The ARM9 can be taken over by using the LZ77-via-FIFO approach to have it
decompress data directly onto its stack, while ARM7 control can be gained using
the very first, direct loading method.

However, this is where things get slightly confusing: the ARM9 bootrom will
first try to verify the *ARM7* image, but we want to take it over *immediately*
to prevent it from checking the SHA1 hash of the ARM7 payload. Thus, the "ARM7
stage2 image" will necessarily have to be the *exploit payload* for the ARM9.
From that point on, we can, in this payload, pretend to be the real ARM9 boot
ROM, tell the ARM7 that the loading process went OK, and have it load the "ARM9
stage2 image" (in fact being the *ARM7 exploit payload*) into its own stack.
And at that point, it's game over for Nintendo.

## Setup

### ARM9

We still need to know the exact addresses at which we should be mapping the
payloads. The ARM9 bootrom maps its DTCM onto a part of main RAM also
containing the exception vector redirection addresses (The ARM9 BIOS contains
the real exception vector, but simply redirects most of those on main RAM.
However, main RAM is only enabled by stage2, so normally this area would be
open bus, if not for the DTCM remapping.), more precisely, its addresses look
as follows:

* DTCM start, `.data` and `.bss` sections: `0x02ff8000`. It is 16 KiB (`0x4000`
  bytes) in size, thus ends at `0x02ffbfff`.
* Bootrom exception redirect addresses are at `0x02ffbfc0` and onward.
* Right above that is the IRQ/SWI stack, `0x3c0` bytes in size.
* Then, the regular runtime stack is placed below it, with no clearly-defined
  stack bottom.
* The used parts of the DTCM go up to `0x02ff9200`, so everything below that is
  free for us to use while not overwriting RAM data that could be useful for
  our exploit payload.

The ARM9 also uses ITCM, mapped at address `0` through `0x01ffffff` (though
repeating every 32 KiB, i.e. `0x8000` bytes), and has the MPU enabled, with the
following configuration (considering privileged mode only, as the ROMs never
change privilege, dcache is 4 KiB, icache 8 KiB):

* Region 0: `0x04000000`-`0x08000000` (MMIO), rwx, no cache
* Region 1: `0x03000000`-`0x04000000` (WRAM), rwx, d- and icache, write-back
* Region 2: `0x027e0000`-`0x02800000` (????), ---, no cache
* Region 3: `0x0c000000`-`0x0e000000` (DRAM), ---, no cache
* Region 4: `0x02ff8000`-`0x02ffc000` (DTCM), rw-, no cache
* Region 5: `0x01000000`-`0x02000000` (ITCM), rwx, icache
* Region 6: `0xffff0000`-`0x00000000` (BROM), r-x, d- and icache, write-back
* Region 7: `0x02fff000`-`0x03000000` (????), ---, no cache

On first sight, one could try directly overwriting the stack (in DTCM) and
placing shellcode there, but that won't work, as it's marked as no-execute. One
might think of using ROP to first branch into a part of the ROM that disables
the MPU, and *then* jumps into the shellcode, but no relevant CP15-manipulating
functions touch the stack (instead using the return address stored in `lr`).
Splitting the payload in two regions (one for the stack in the DTCM, one for
the payload code in eg. ITCM) also won't work.

But there's still an opening left. Note that we can control the NWRAM mapping
(as it's part of the stage2 image header of which the verification is skipped),
so we can position it right at the edge of the WRAM address space, right in
front of... MMIO. Then, a lot of data (to completely flush the dcache) can be
written in WRAM, containing the payload and stack overwriting data, and then...
we continue writing into MMIO. What's there? Well, first are the 2D display
control registers, followed at `+0xb0` by... DMA.

With DMA, we can start copies to/from different memory areas by writing to a
few MMIO registers. This can be used to copy a chunk of NWRAM straight to
DTCM... except the TCMs are *tightly-coupled*, so they can't be reached by DMA.

So let's go back to the ROP idea: instead of trying a simple "return-to-ROM"
attack, we could also try creating a more complex ROP chain doing what we need:
At `0xffff1e48`, there's a gadget loading `r4-r12` and `lr` from the stack,
then jumping to `lr`. At `0xffff29ec`, there's the following interesting
sequence of instructions:

```
LAB_ffff29c0:
	mov        r0,r4
	ldmia      sp!,{r4 r5 r6 pc}

LAB_ffff29c8:
	// snip...

LAB_ffff29e4:
	cmp        r0,#0x0
	ldmiaeq    sp!,{r4 r5 r6 pc}
LAB_ffff29ec:
	ldr        r0,[r5,#0x4]
	ldr        r1,[r4,#0x0]
	mov        r2,r0, lsl #0x2
	ldr        r0,[r5,#0x0]
	bl         memcpy_unalign
	ldr        r0,[r5,#0x4]
	cmp        r0,#0x0
	str        r0,[r4,#0x4]
	bne        LAB_ffff2a20
	ldr        r0,[r4,#0x0]
	cmp        r0,#0x0
	movne      r1,#0x0
	strne      r1,[r0,#0x0]
LAB_ffff2a20:
	ldr        r0,[r5,#0xc]
	str        r0,[r4,#0xc]
	b          LAB_ffff29c0
```

At first, when entering at `0xffff29ec`, it will load the parameters for
`memcpy_align()` and call the function. As soon as the latter returns,  the CPU
will continue a bit, eventually leading to the `ldmia sp!, {r4-r6, pc}` at the
top.

The vulnerable return statement in the bootrom code is at `0xffff5200` popping
`r3-r11, pc` from the stack.

We can thus prepare a stack as follows:
* `r3` filler
* `r4`, being a pointer to `r1` for `memcpy_unalign(src, dest, len)`
* `r5`, being a pointer to [`r0` for `memcpy_unalign` followed by `r2>>2` for `memcpy_unalign`]
* `r6-r11`, filler
* `lr`, i.e. `0xffff29ec`
* `r4-r6` for the `ldmia sp!` at `0xffff29c4`, filler
* `pc` for the same `ldmia sp!`, pointing to our payload entrypoint

Do note that the code after the `memcpy_unalign` call, the code will necessarily:
* Store `r2>>2` for the `memcpy_unalign` call at `*(r4+4)`
* Zero out the first byte of the source
* Copy `*(r5+12)` to `*(r4+12)`

### ARM7

This CPU is a lot simpler: no TCMs, no MPU, no caches, only WRAM and (sadly)
NWRAM. But luckily, we can ignore the latter, as the stack is mapped to old
WRAM, which is 64 KiB in size. The bootrom actually uses the mirror at the end
of the WRAM address space, i.e. from `0x03ff0000` to `0x03ffffff`, with a
layout as follows:

* `.data` and `.bss` sections at `0x03ff4000`, up to `0x03ff9714`. Additionally
  it has some sort of "heap" at `0x03ffc000` to `0x03ffc3ff`, which contains
  another copy of the stage2 header read from the boot medium.
* Bootrom redirect addresses at `0x03ffffc0`.
* Below that, a `0x40`-byte SWI stack.
* Below that, a `0x380`-byte IRQ stack.
* Below that, the regular runtime stack without clear limit.

In the worst case (loading our payload after the "heap", `0x03ffc400`), this
would be `0x3800` bytes of space, i.e. 14 KiB. If this isn't enough (eg. for a
FAT and SD/MMC driver to read a homebrew binary from the SD card, or to do
other, heavier system initialization or perhaps a diagnostic process), we could
overwrite the stage2 header from the "heap" and re-request it from the ARM9's
backup in DTCM. This would mean a load address at about `0x03ff9800`, and an
available memory size of `0x6400`, i.e. 25 KiB.

The vulnerable return instruction in this case is at `0x9790`, which loads `r4`
to `r8` from the stack, followed by `lr`, which is then also branched to.

## Payload

To take over the ARM7 as well, the payload running on the ARM9 needs to do the
following:
* Disable MPU
* Keep reading data from the ARM7 FIFO until the latter runs out, to ensure
  proper ARM7 execution continuation.
  * i.e. call `boot9i_LZ77_close()`
* Flush DCache
* By this time, the ARM7 should be getting taken over.
* Wait for `ipc_notifyrecv(42)` to synchronize.

The ARM7 payload should, on its turn, send `ipc_notifyID(42)` to tell the ARM9
that it has been taken over as well.

## Packaging it into a SPI flash image

A SPI flash consists of the following data:
* A 512-byte "filler" area, actually wifi settings. The first 28 bytes must
  have a fixed value!
* The 512-byte boot header.
* Space for firmware, up to `0x1f3ff` (i.e. 124 KiB). (This was old-DS firmware code.)
* 3*512 bytes for WPA2 wifi settings, up to `0x1ffff`.
* `0x1fe00` unused bytes(?) (i.e. 127.5 KiB) (not present in DSi, i.e. no physical backing memory)
* 2*256 bytes of user settings (mirrored to end of lower address space half on DSi)

A SPI flash chip should be at least 128 KiB in size, preferably 256 KiB (i.e.
resp. 1 Mbit, 2 Mbit).

The exploit payload should be stored in the `0x400..0x1f3ff` area. It is
encrypted using AES-CTR, with the following keys and IV:

* KeyX: "Nintendo DS\x00\x01\x#!\0"
* KeyY: from RSA signature message, all-zero due to the exploit
* IV: +blocksz ;; -blocksz ;; -blocksz-1 ;; 0(counter, per-AES-block increment by 1)

