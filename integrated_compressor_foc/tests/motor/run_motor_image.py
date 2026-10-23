"""Execute a bare-metal motor test image; synthetic values never reach hardware."""
from pathlib import Path
import sys
import unicorn
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_PC

binary, entry, source = Path(sys.argv[1]).read_bytes(), int(sys.argv[2], 0), sys.argv[3]
machine = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB | unicorn.UC_MODE_MCLASS)
machine.mem_map(0x10000, 0x100000)
machine.mem_map(0x20000000, 0x20000)
machine.mem_write(0x10000, binary)
machine.mem_write(0x20000000, b"\xff" * 4)
machine.reg_write(UC_ARM_REG_SP, 0x2001fff8)
try:
    machine.emu_start(entry | 1, 0x110000, count=40_000_000)
except unicorn.UcError as error:
    if error.errno != unicorn.UC_ERR_EXCEPTION:
        raise
result = int.from_bytes(machine.mem_read(0x20000000, 4), "little")
if result:
    print(f"{source}:{result}: embedded assertion failed; PC=0x{machine.reg_read(UC_ARM_REG_PC):08x}")
    raise SystemExit(1)
print(f"PASS: {source}")
