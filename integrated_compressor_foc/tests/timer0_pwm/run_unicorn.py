"""Run the bare-metal TIMER0 BSP test image in a fresh Unicorn process."""

from pathlib import Path
import sys

import unicorn
from unicorn import Uc, UcError
from unicorn.arm_const import UC_ARM_REG_PC, UC_ARM_REG_SP


FLASH_BASE = 0x00010000
FLASH_SIZE = 0x00100000
RAM_BASE = 0x20000000
RAM_SIZE = 0x00020000
RESULT_SENTINEL = 0xFFFFFFFF


def main() -> int:
    binary = Path(sys.argv[1]).read_bytes()
    entry = int(sys.argv[2], 0)

    machine = Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB | unicorn.UC_MODE_MCLASS)
    machine.mem_map(FLASH_BASE, FLASH_SIZE, unicorn.UC_PROT_ALL)
    machine.mem_map(RAM_BASE, RAM_SIZE, unicorn.UC_PROT_ALL)
    machine.mem_write(FLASH_BASE, binary)
    machine.mem_write(RAM_BASE, RESULT_SENTINEL.to_bytes(4, "little"))
    machine.reg_write(UC_ARM_REG_SP, RAM_BASE + RAM_SIZE - 8)

    try:
        machine.emu_start(entry | 1, FLASH_BASE + FLASH_SIZE, count=2_000_000)
    except UcError as error:
        if error.errno != unicorn.UC_ERR_EXCEPTION:
            raise

    result = int.from_bytes(machine.mem_read(RAM_BASE, 4), "little")
    pc = machine.reg_read(UC_ARM_REG_PC)
    if result == RESULT_SENTINEL:
        print(f"emulation faulted before reporting a result (PC=0x{pc:08X})")
        return 2
    if result != 0:
        print(f"embedded assertion failed at tests/timer0_pwm/test_timer0_pwm.c:{result}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
