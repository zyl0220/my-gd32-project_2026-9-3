from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "third_party" / "compressor_foc_starter"
FIXTURE = ROOT / "tests" / "motor" / "test_field_weakening_archive.c"

# The soft-float math has a larger instruction budget than the register tests.
# This checks ARM numerical behavior; it does not measure MCU execution time.
RUNNER = r'''
from pathlib import Path
import sys
import unicorn
from unicorn import Uc, UcError
from unicorn.arm_const import UC_ARM_REG_PC, UC_ARM_REG_SP

flash_base = 0x00010000
flash_size = 0x00100000
ram_base = 0x20000000
ram_size = 0x00020000
machine = Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB | unicorn.UC_MODE_MCLASS)
machine.mem_map(flash_base, flash_size, unicorn.UC_PROT_ALL)
machine.mem_map(ram_base, ram_size, unicorn.UC_PROT_ALL)
machine.mem_write(flash_base, Path(sys.argv[1]).read_bytes())
machine.mem_write(ram_base, (0xFFFFFFFF).to_bytes(4, "little"))
machine.reg_write(UC_ARM_REG_SP, ram_base + ram_size - 8)
try:
    machine.emu_start(int(sys.argv[2], 0) | 1, flash_base + flash_size, count=100_000_000)
except UcError as error:
    if error.errno != unicorn.UC_ERR_EXCEPTION:
        raise
result = int.from_bytes(machine.mem_read(ram_base, 4), "little")
assert result != 0xFFFFFFFF, f"no result before instruction budget/fault, PC={machine.reg_read(UC_ARM_REG_PC):#x}"
assert result == 0, f"embedded assertion failed at tests/motor/test_field_weakening_archive.c:{result}"
'''


def test_original_field_weakening_on_cortex_m23(tmp_path: Path) -> None:
    assert (VENDOR / "src/field_weakening.c").is_file(), "Original ZIP field-weakening module has not been imported"
    assert (VENDOR / "include/field_weakening.h").is_file()
    gcc = shutil.which("arm-none-eabi-gcc")
    assert gcc is not None
    compiler = Path(gcc)
    toolchain = compiler.parent.parent
    libgcc = next((toolchain / "lib/gcc/arm-none-eabi").glob("*/thumb/v8-m.base/nofp/libgcc.a"))
    libc = toolchain / "arm-none-eabi/lib/thumb/v8-m.base/nofp"
    elf = tmp_path / "field_weakening_archive.elf"
    binary = tmp_path / "field_weakening_archive.bin"
    subprocess.run(
        [gcc, "-mcpu=cortex-m23", "-mthumb", "-Os", "-std=gnu11",
         "-Wall", "-Wextra", "-Werror", "-ffreestanding", "-fno-builtin",
         "-nostdlib", "-ffunction-sections", "-fdata-sections",
         "-I", str(VENDOR / "include"), str(FIXTURE),
         str(VENDOR / "src/field_weakening.c"),
         f"-Wl,-T,{ROOT / 'tests/timer0_pwm/linker.ld'}", "-Wl,--gc-sections",
         "-L", str(libgcc.parent), "-L", str(libc),
         "-Wl,--start-group", "-lm", "-lc", "-lgcc", "-Wl,--end-group",
         "-o", str(elf)],
        check=True, cwd=ROOT, capture_output=True, text=True,
    )
    nm = subprocess.run(
        [str(compiler.with_name("arm-none-eabi-nm.exe")), "--defined-only", str(elf)],
        check=True, capture_output=True, text=True,
    ).stdout
    entry = int(re.search(r"^([0-9a-fA-F]+)\s+T\s+test_entry$", nm, re.M).group(1), 16)
    subprocess.run(
        [str(compiler.with_name("arm-none-eabi-objcopy.exe")), "-O", "binary", str(elf), str(binary)],
        check=True, capture_output=True, text=True,
    )
    env = dict(os.environ)
    env["PYTHONPATH"] = str(ROOT / "build/python-deps")
    subprocess.run(
        [sys.executable, "-c", RUNNER, str(binary), hex(entry)],
        check=True, cwd=ROOT, env=env, capture_output=True, text=True,
    )
