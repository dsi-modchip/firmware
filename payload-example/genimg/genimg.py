#!/usr/bin/env python3

import argparse, codecs
from struct import pack, unpack
from typing import NamedTuple, Optional
from enum import *
import os.path, subprocess, sys

from Crypto.Cipher import AES
from Crypto.Util import Counter

import hackyelf
from hackyelf import *


AES_KEYSIZE_BYTES = 16
AES_KEYSIZE_BITS = 128

AES_BLOCKSIZE_BYTES = 16
AES_BLOCKSIZE_BITS = 128

KEYX = b"Nintendo DS\x00\x01#!\x00"
KEYY_REGULAR = b"\xec\x07\x00\x00\x34\xe2\x94\x7c\xc3\x0e\x81\x7c\xec\x07\x00\x00"
KEYY_EXPLOIT = b"\0"*AES_KEYSIZE_BYTES

FLASH_HEADER = \
        b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" + \
        b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x57\xff\xff" + \
        b"\xc0\x3f\x00\x00\x00\x00\x00\x00"

# TODO: read these from payload2 ELF_Nhdr!
# MBK1..5
NWRAM_CONFIG = \
    b"\x80\x84\x88\x8c" + \
    b"\x81\x85\x89\x8d" + \
    b"\x91\x95\x99\x9c" + \
    b"\x81\x85\x89\x8d" + \
    b"\x91\x95\x99\x9d"
# ARM7 MBK6..8
NWRAM_CONFIG = NWRAM_CONFIG + \
    b"\xc0\x37\x00\x08" + \
    b"\x00\x30\xc0\x07" + \
    b"\x00\x30\x00\x00"
# ARM9 MBK6..8
NWRAM_CONFIG = NWRAM_CONFIG + \
    b"\x00\x30\x00\x00" + \
    b"\x00\x30\x40\x00" + \
    b"\xb8\x37\xf8\x07"
# MBK9, WRAMCFG
NWRAM_CONFIG = NWRAM_CONFIG + b"\x00\x00\x00\xff"


PADDING_0 = b"\0"*0x20
PADDING_1 = b"\0"*0xbf
RSA_SIG = b"\0"*0x80
PADDING_2 = b"\xff"*0x50
PADDING_3 = b"\0"*0x20

BLOCK_ALIGN   =  0x200
SEGMENT_ALIGN = 0x1000  # flash erase sector size. while the original flash can
                        # do this in 256b units, we're more strict here because
                        # other flash chips could be used

WIFISETTINGS_BOUNDARY = 0x1f400
USERSETTINGS_BOUNDARY = 0x1fa00
USERSETTINGS_BOUNDARY2= 0x3fa00

class BootFlags(IntFlag):
    # note: these two are from the POV of the bootrom, not the exploit
    A9_LZ77       = 1<<0
    A7_LZ77       = 1<<1
    A9_CLK_133MHZ = 1<<2
    LZ77_USE_FIFO = 1<<3
    SPI_USE_8MHZ  = 1<<6
    USE_NAND      = 1<<7

BOOTFLAGS_DEFAULT = BootFlags.A7_LZ77|BootFlags.LZ77_USE_FIFO|BootFlags.A9_CLK_133MHZ  # TODO: SPI_USE_8MHZ?

class FlashSize(IntEnum):
    FS_1MBIT = 0
    FS_2MBIT = 1
    FS_BIG   = 2

assert len(KEYX) == AES_KEYSIZE_BYTES
assert len(KEYY_REGULAR) == AES_KEYSIZE_BYTES
assert len(KEYY_EXPLOIT) == AES_KEYSIZE_BYTES

assert len(FLASH_HEADER) == 0x28
assert len(PADDING_0) == 0x20
assert len(PADDING_1) == 0xbf
assert len(RSA_SIG) == 0x80
assert len(PADDING_2) == 0x50


# NOTE: re: "arm7" and "arm9" terminology is used from the viewpoint of the
#       exploit payload destination, NOT how the ROMs intend to use the data


class ElfInfo(NamedTuple):
    runaddr: int
    codesize: int
    blocksize: int
    data: bytes
    nwram: Optional[bytes] = None


def align_to(to_align: int, alignment: int) -> int:
    misalign = to_align % alignment
    to_add = 0 if misalign == 0 else (alignment - misalign)

    return (to_align + to_add), to_add


def derive_normal_key(keyx: bytes, keyy: bytes) -> bytes:
    keyx_i = int.from_bytes(keyx, 'little')
    keyy_i = int.from_bytes(keyy, 'little')

    key_xy = keyx_i ^ keyy_i
    nkey = (key_xy + 0xFFFEFB4E295902582A680F5F1A4F3E79) & ((1<<AES_KEYSIZE_BITS)-1)

    if keyy == KEYY_REGULAR:
        # can check algo against known good values
        KNOWN_KEY_XY = b"\xa2\x6e\x6e\x74\x51\x8c\xf0\x13\xe3\x4a\xd2\x7c\xed\x24\x21\x00"
        KNOWN_AFTER_ADD = b"\x1b\xad\xbd\x8e\xb0\x9b\x58\x3e\x3b\x4d\x2b\xa6\x3b\x20\x20\x00"

        assert key_xy == int.from_bytes(KNOWN_KEY_XY, 'little')
        assert nkey == int.from_bytes(KNOWN_AFTER_ADD, 'little')

    nkey = nkey << 42
    nkey = (nkey & ((1<<AES_KEYSIZE_BITS)-1)) | (nkey >> AES_KEYSIZE_BITS)

    return nkey.to_bytes(byteorder='little', length=AES_KEYSIZE_BYTES)

def derive_iv(blocksz: int) -> bytes:
    r = bytearray(AES_BLOCKSIZE_BYTES)
    r[ 0: 4] = pack('<I', ( blocksz)&0xffffffff)
    r[ 4: 8] = pack('<I', (-blocksz)&0xffffffff)
    r[ 8:12] = pack('<I', (~blocksz)&0xffffffff)
    r[12:16] = b"\0\0\0\0"  # discarded by aes_ctr_crypt!
    return r


def aes_ctr_crypt(key: bytes, iv: bytes, pt: bytes) -> bytes:
    assert len(key) == AES_KEYSIZE_BYTES
    assert len(iv) == AES_BLOCKSIZE_BYTES
    assert len(pt) % BLOCK_ALIGN == 0

    aes = AES.new(key[::-1], AES.MODE_ECB)
    r = bytearray(len(pt))

    ivi = int.from_bytes(iv, 'little')
    for bind in range(0, len(pt), AES_BLOCKSIZE_BYTES):
        ptbl  = pt[bind:(bind+AES_BLOCKSIZE_BYTES)]

        ivbl  = ivi.to_bytes(byteorder='little', length=AES_BLOCKSIZE_BYTES)
        ivbl  = aes.encrypt(ivbl[::-1])

        block = bytes(ptbl[i] ^ ivbl[15-i] for i in range(AES_BLOCKSIZE_BYTES))
        r[bind:(bind+AES_BLOCKSIZE_BYTES)] = block

        ivi += 1
    return r


def get_ei(pl: bytes, elf: ELF, compr: bool) -> ElfInfo:
    assert elf.etype  == ET_EXEC
    assert elf.mach   == EM_ARM
    assert elf.eclass == ELFCLASS32

    addr = None
    size = 0
    data = b""

    for phdr in elf.phdrs:
        # TODO: check for PT_NOTE for NWRAM config!
        assert phdr.ptype not in {PT_DYNAMIC, PT_INTERP}
        if phdr.memsz > 0 and phdr.ptype == PT_LOAD:
            sz = min(phdr.filesz, phdr.memsz)
            if size != 0:  # already had a previous one
                # can only grow a single continuous region up or down
                if phdr.paddr > addr:
                    assert addr + size == phdr.paddr, "Malformed ELF!"
                    addr = addr
                elif phdr.paddr < addr:
                    assert phdr.paddr + sz == addr, "Malformed ELF!"
                    addr = phdr.paddr
                else:
                    assert False, "wut"
            else:
                addr = phdr.paddr

            size += sz
            data += pl[phdr.off:(phdr.off+sz)]

    if addr is None:
        raise ValueError("E: ELF file seems to have no data.")

    csize = size
    if compr:
        # sorry to non-linux people out there, but im using memfd_create here
        assert sys.platform == 'linux'

        fdcin = os.memfd_create("lzss-compression-in" )
        fdcout= os.memfd_create("lzss-compression-out")
        pid = os.getpid()
        pathin = "/proc/%d/fd/%d" % (pid, fdcin )
        pathout= "/proc/%d/fd/%d" % (pid, fdcout)

        os.write(fdcin, data)
        os.lseek(fdcin, 0, os.SEEK_SET)  # rewind to beginning

        scriptdir = os.path.split(os.path.realpath(sys.argv[0]))[0]
        subprocess.run([scriptdir+"/lzss","-ewn", pathin, pathout], check=True)

        csize = os.lseek(fdcout, 0, os.SEEK_END) - 4 # get end
        #print("csize",csize)
        os.lseek(fdcout, 4, os.SEEK_SET)  # rewind to beginnind BUT SKIP HEADER
        data = os.read(fdcout, csize)

        #with open('out.lzss', 'wb') as f:
        #    f.write(data)

    # add alignemnt stuff
    blksz, to_add = align_to(csize, BLOCK_ALIGN)
    data = data + b"\0"*to_add

    return ElfInfo(addr, size, blksz, data)

def genhdr(a17: ElfInfo, a19: ElfInfo, flags: int) -> bytes:
    # ARM9 payload (== A9 sploit brom thinks is for A7) comes first
    off19 = SEGMENT_ALIGN
    assert off19 + a19.blocksize < WIFISETTINGS_BOUNDARY

    off17 = off19 + a19.blocksize
    off17, _ = align_to(off17, SEGMENT_ALIGN)

    # skip this region if A7 pl is big. implies we need a 2 Mbit flash.
    while off17 >= WIFISETTINGS_BOUNDARY and off17 < USERSETTINGS_BOUNDARY:
        off17 += SEGMENT_ALIGN

    assert off17 + a17.blocksize < USERSETTINGS_BOUNDARY2

    header = bytearray(b"\xff"*SEGMENT_ALIGN)
    header[0:len(FLASH_HEADER)] = FLASH_HEADER
    header[0x200:0x220] = PADDING_0

    # used for ARM9 by bootroms, but our ARM7 exploit needs to be put here
    header[0x220:0x224] = pack('<I', off17)
    header[0x224:0x228] = pack('<I', a17.codesize)
    header[0x228:0x22c] = pack('<I', a17.runaddr)
    header[0x22c:0x230] = pack('<I', a17.blocksize)

    # and vice versa
    header[0x230:0x234] = pack('<I', off19)
    header[0x234:0x238] = pack('<I', a19.codesize)
    header[0x238:0x23c] = pack('<I', a19.runaddr)
    header[0x23c:0x240] = pack('<I', a19.blocksize)

    header[0x240:0x2ff] = PADDING_1
    header[0x2ff] = flags
    header[0x300:0x380] = RSA_SIG
    header[0x380:0x3b0] = a17.nwram or a19.nwram or NWRAM_CONFIG
    header[0x3b0:0x400] = PADDING_2
    #header[0x3b0:0x3c0] = PADDING_2
    #header[0x3c0:0x3e0] = b"\0"*0x20  # TODO: payload2 info here!
    #header[0x3e0:0x400] = PADDING_3

    return header, off17, off19


def do_genimg(args):
    payload17 = args.payload17.read()
    payload19 = args.payload19.read()
    elf17 = hackyelf.parse(payload17)
    elf19 = hackyelf.parse(payload19)
    elf17 = get_ei(payload17, elf17, (args.bootflags & BootFlags.A9_LZ77) != 0)
    elf19 = get_ei(payload19, elf19, (args.bootflags & BootFlags.A7_LZ77) != 0)

    tpl = None if args.template is None else args.template.read()

    # ARM7 payload comes first
    hdr, off17, off19 = genhdr(elf17, elf19, args.bootflags)
    assert len(hdr) == SEGMENT_ALIGN

    size17_seg, _ = align_to(elf17.blocksize, SEGMENT_ALIGN)
    size19_seg, _ = align_to(elf19.blocksize, SEGMENT_ALIGN)

    keyy = KEYY_REGULAR if args.use_regular_keyY else KEYY_EXPLOIT
    nkey = derive_normal_key(KEYX, keyy)
    iv_7 = derive_iv(elf17.blocksize)
    iv_9 = derive_iv(elf19.blocksize)

    if args.use_regular_keyY:
        NORMALKEY_KNOWN_GOOD = b"\x98\xee\x80\x80\x00\x6c\xb4\xf6\x3a\xc2\x6e\x62\xf9\xec\x34\xad"
        assert nkey == NORMALKEY_KNOWN_GOOD

    print("keyX:", codecs.encode(KEYX, 'hex'))
    print("keyY:", codecs.encode(keyy, 'hex'))
    print("NormalKey:", codecs.encode(nkey, 'hex'))
    print("ARM7 IV:", codecs.encode(iv_7, 'hex'))
    print("ARM9 IV:", codecs.encode(iv_9, 'hex'))

    neededsize = (SEGMENT_ALIGN + size17_seg + size19_seg)
    flashsize = FlashSize.FS_1MBIT
    totalsize = 0x20000

    if off19 >= WIFISETTINGS_BOUNDARY:
        flashsize = FlashSize.FS_2MBIT
        totalsize = 0x40000

    img = bytearray(b"\xff"*totalsize)
    img[0:len(hdr)] = hdr

    if tpl is not None:
        assert len(tpl) in {0x20000, 0x40000}

        img[0:0x200] = tpl[0:0x200]
        img[WIFISETTINGS_BOUNDARY:USERSETTINGS_BOUNDARY] = \
            tpl[WIFISETTINGS_BOUNDARY:USERSETTINGS_BOUNDARY]

        userset_tpl = tpl[USERSETTINGS_BOUNDARY:]
        if len(tpl) > 0x20000:
            userset_tpl = tpl[USERSETTINGS_BOUNDARY2:]

        if flashsize >= FlashSize.FS_2MBIT:
            img[USERSETTINGS_BOUNDARY2:0x40000] = userset_tpl
        else:
            img[USERSETTINGS_BOUNDARY :0x20000] = userset_tpl

    img[off17:(off17+elf17.blocksize)] = aes_ctr_crypt(nkey, iv_7, elf17.data)
    img[off19:(off19+elf19.blocksize)] = aes_ctr_crypt(nkey, iv_9, elf19.data)

    msgs = {
        FlashSize.FS_1MBIT: "Fits in a 1 Mbit flash image, all good! (need %.1f KiB)"%(neededsize/1024),
        FlashSize.FS_2MBIT: "Needs a 2 Mbit flash chip! (need %.1f KiB)"%(neededsize/1024),
        FlashSize.FS_BIG  : "Needs a BIG flash chip! (need %.2f Mibit)"%(8*neededsize/(1024*1024)),
    }
    args.output.write(img)
    print("Writing flash image done. %s" % msgs[flashsize])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--output', type=argparse.FileType('wb'),
                        help="Output file")

    parser.add_argument('--use-regular-keyY', default=False, action='store_true',
                        help="Use the retail keyY for "+\
                        "encrypting the payload binaries, instead of the "+\
                        "exploit (all-zero) one.")
    parser.add_argument('--bootflags', type=int, default=BOOTFLAGS_DEFAULT,
                        help="Boot flags. Default is " + hex(BOOTFLAGS_DEFAULT))
    parser.add_argument('--template', type=argparse.FileType('rb'), default=None,
                        help="Template firmware dump to incorporate user settings from")

    parser.add_argument('payload17', type=argparse.FileType('rb'),
                        help="ARM7 payload1 input ELF file")
    parser.add_argument('payload19', type=argparse.FileType('rb'),
                        help="ARM9 payload1 input ELF file")

    #parser.add_argument('payload27', type=argparse.FileType('rb'),
    #                    help="ARM7 payload2 input ELF file", default=None)
    #parser.add_argument('payload29', type=argparse.FileType('rb'),
    #                    help="ARM9 payload2 input ELF file", default=None)

    args = parser.parse_args()
    return do_genimg(args)

if __name__ == '__name__' or True:
    r = main()
    if r is not None and isinstance(r, int):
        exit(r)

