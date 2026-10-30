"""Retain actual PC switching instructions for the isolated crash regression."""
from pathlib import Path
import sys
import capstone
import pefile

source, destination = map(Path, sys.argv[1:])
pe = pefile.PE(str(source))
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True
shift = 0x20000000
blocks = []
for start, end in [(0x491620, 0x491757), (0x491760, 0x4917fb),
                   (0x491800, 0x491838), (0x491840, 0x491a93),
                   (0x579e70, 0x579f54), (0x579f60, 0x579ffc),
                   (0x5b9e30, 0x5b9e46), (0x6e693a, 0x6e6949)]:
    data = bytearray(pe.get_data(start - 0x400000, end - start))
    for instruction in md.disasm(bytes(data), start):
        # Both source and destination of relative branches shift equally.
        # Absolute game globals/immediate pointers must shift explicitly.
        if instruction.group(capstone.CS_GRP_JUMP) or instruction.group(capstone.CS_GRP_CALL):
            continue
        for operand in instruction.operands:
            if operand.type == capstone.CS_OP_MEM:
                value, offset, size = operand.mem.disp, instruction.disp_offset, instruction.disp_size
            elif operand.type == capstone.CS_OP_IMM:
                value, offset, size = operand.imm, instruction.imm_offset, instruction.imm_size
            else:
                continue
            if 0x400000 <= value < 0x980000 and size == 4:
                at = instruction.address - start + offset
                data[at:at+4] = (value + shift).to_bytes(4, 'little')
    blocks.append((start + shift, data))
out = ['// Generated from the owned PC analysis dump; never shipped.', 'inline void copyNativeSwitchCode() {']
for index, (address, data) in enumerate(blocks):
    out += [f'    const unsigned char code{index}[] = {{' + ','.join(hex(v) for v in data) + '};',
            f'    memcpy((void*){hex(address)},code{index},sizeof(code{index}));']
out += ['}']
destination.write_text('\n'.join(out))
