from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "tests" / "timer0_pwm"


def _tool(name: str) -> str:
    executable = os.environ.get(name.upper().replace("-", "_"))
    return executable or name


def _run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    )


def _entry_address(elf: Path) -> int:
    output = _run([_tool("arm-none-eabi-nm"), "--defined-only", str(elf)]).stdout
    match = re.search(r"^([0-9a-fA-F]+)\s+T\s+test_entry$", output, re.MULTILINE)
    assert match is not None, output
    return int(match.group(1), 16)


def test_timer0_pwm_driver_on_cortex_m23(tmp_path: Path) -> None:
    """Wrong timer setup, unsafe output ordering, or bad conversions fail in emulation."""
    elf = tmp_path / "test_timer0_pwm.elf"
    binary = tmp_path / "test_timer0_pwm.bin"
    source = ROOT / "src" / "bsp" / "bsp_timer0_pwm.c"
    compiler_path = Path(shutil.which(_tool("arm-none-eabi-gcc")) or "")
    installed_libgcc = next(
        (compiler_path.parent.parent / "lib" / "gcc" / "arm-none-eabi").glob(
            "*/thumb/v8-m.base/nofp/libgcc.a"
        )
    )
    libgcc = tmp_path / "libgcc.a"
    shutil.copy2(installed_libgcc, libgcc)

    _run(
        [
            _tool("arm-none-eabi-gcc"),
            "-mcpu=cortex-m23",
            "-mthumb",
            "-std=gnu11",
            "-O1",
            "-g",
            "-ffreestanding",
            "-fno-builtin",
            "-fno-tree-loop-distribute-patterns",
            "-ffunction-sections",
            "-fdata-sections",
            "-nostdlib",
            "-I",
            str(FIXTURE / "fakes"),
            "-I",
            str(FIXTURE),
            "-I",
            str(ROOT / "inc"),
            str(FIXTURE / "test_timer0_pwm.c"),
            str(FIXTURE / "fake_gd32.c"),
            str(source),
            f"-Wl,-T,{FIXTURE / 'linker.ld'}",
            "-Wl,--gc-sections",
            str(libgcc),
            "-o",
            str(elf),
        ]
    )
    _run([_tool("arm-none-eabi-objcopy"), "-O", "binary", str(elf), str(binary)])

    entry = _entry_address(elf)
    completed = subprocess.run(
        [
            sys.executable,
            str(FIXTURE / "run_unicorn.py"),
            str(binary),
            hex(entry),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    assert completed.returncode == 0, completed.stdout + completed.stderr
