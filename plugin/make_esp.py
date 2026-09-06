#!/usr/bin/env python3
"""
Builds HairPositioner.esp.

Minimal ESL-flagged plugin: one quest carrying the RaceMenu plugin script
plus a Player alias with RaceMenu's own RaceMenuLoad alias script (which is
how RaceMenu drives OnGameReload). Same shape as any RaceMenu slider plugin.
"""
import struct, sys

AUTHOR      = b"qtuna\x00"
EDID        = b"HairPositionerRSM\x00"      # EDID + FULL
SCRIPT_NAME = b"RaceMenuHairPositioner"     # must be 22 bytes to keep VMAD sane
ALIAS_SCRIPT= b"RaceMenuLoad"               # RaceMenu's own alias script
FORMID      = 0x01000800                    # index 01 (Skyrim.esm), ESL-safe 0x800-0xFFF

assert len(SCRIPT_NAME) == 22, "script name length changed; VMAD layout assumes 22"

def sub(sig, data):
    return sig + struct.pack('<H', len(data)) + data

def wstr(b):
    return struct.pack('<H', len(b)) + b

# ---- VMAD (cloned layout) -------------------------------------------------
vmad  = struct.pack('<hhH', 5, 2, 1)          # version, objFormat, scriptCount
vmad += wstr(SCRIPT_NAME) + b'\x00'           # name, status
vmad += struct.pack('<H', 0)                  # propertyCount
vmad += b'\x02'                               # QUST fragment block: unknown
vmad += struct.pack('<h', 0)                  # fragmentCount
vmad += wstr(b'')                             # fragment fileName
vmad += struct.pack('<H', 1)                  # aliasCount
vmad += struct.pack('<HH', 0, 0)              # alias object union: unused, aliasIndex
vmad += struct.pack('<I', FORMID)             # ...owning quest formID
vmad += struct.pack('<hhH', 5, 2, 1)          # alias script: version, objFormat, count
vmad += wstr(ALIAS_SCRIPT) + b'\x00'
vmad += struct.pack('<H', 0)                  # propertyCount

# ---- QUST -----------------------------------------------------------------
qust  = sub(b'EDID', EDID)
qust += sub(b'VMAD', vmad)
qust += sub(b'FULL', EDID)
qust += sub(b'DNAM', bytes([0x11,0x00, 0x00, 0xff]) + b'\x00'*8)   # start game enabled
qust += sub(b'NEXT', b'')
qust += sub(b'ANAM', struct.pack('<I', 1))     # next alias id
qust += sub(b'ALST', struct.pack('<I', 0))     # alias 0
qust += sub(b'ALID', b'Player\x00')
qust += sub(b'FNAM', struct.pack('<I', 0))
qust += sub(b'ALFR', struct.pack('<I', 0x14))  # PlayerRef
qust += sub(b'VTCK', struct.pack('<I', 0))
qust += sub(b'ALED', b'')

qust_rec = b'QUST' + struct.pack('<IIIIHH', len(qust), 0, FORMID, 0, 44, 0) + qust

# GRUP header is exactly 24 bytes: sig, size, label, type, timestamp, vc, unknown
grup = b'GRUP' + struct.pack('<I', 24 + len(qust_rec)) + b'QUST' + \
       struct.pack('<IHHI', 0, 0, 0, 0) + qust_rec
assert len(grup) == 24 + len(qust_rec)

# ---- TES4 -----------------------------------------------------------------
hedr  = struct.pack('<fiI', 1.70, 2, FORMID + 1)
tes4  = sub(b'HEDR', hedr)
tes4 += sub(b'CNAM', AUTHOR)
tes4 += sub(b'MAST', b'Skyrim.esm\x00')
tes4 += sub(b'DATA', struct.pack('<Q', 0))
tes4 += sub(b'INTV', struct.pack('<I', 1))

ESL = 0x200
tes4_rec = b'TES4' + struct.pack('<IIIIHH', len(tes4), ESL, 0, 0, 44, 0) + tes4

out = tes4_rec + grup
open('HairPositioner.esp','wb').write(out)
print(f"wrote HairPositioner.esp  ({len(out)} bytes, ESL-flagged, FormID {FORMID:#010x})")
