from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "tests" / "motor"


def test_motor_math_on_cortex_m23(tmp_path: Path) -> None:
    gcc = shutil.which("arm-none-eabi-gcc")
    assert gcc is not None
    libgcc_root = Path(gcc).parent.parent / "lib" / "gcc" / "arm-none-eabi"
    libgcc = next(libgcc_root.glob("*/thumb/v8-m.base/nofp/libgcc.a"))
    elf = tmp_path / "motor_math.elf"
    binary = tmp_path / "motor_math.bin"
    subprocess.run(
        [gcc, "-mcpu=cortex-m23", "-mthumb", "-Os", "-std=gnu11",
         "-ffreestanding", "-fno-builtin", "-nostdlib", "-ffunction-sections",
         "-fdata-sections", "-I", str(ROOT / "inc"),
         str(FIXTURE / "test_motor_math.c"),
         str(ROOT / "src/motor/foc_math.c"), str(ROOT / "src/motor/pi.c"),
         str(ROOT / "src/motor/protection.c"),
         str(ROOT / "src/motor/speed_ctrl.c"),
         str(ROOT / "src/motor/observer.c"),
         f"-Wl,-T,{ROOT / 'tests/timer0_pwm/linker.ld'}", "-Wl,--gc-sections",
         str(libgcc), "-o", str(elf)],
        check=True, cwd=ROOT, capture_output=True, text=True,
    )
    nm = subprocess.run([str(Path(gcc).with_name("arm-none-eabi-nm.exe")),
                         "--defined-only", str(elf)], check=True,
                        capture_output=True, text=True).stdout
    entry = int(re.search(r"^([0-9a-fA-F]+)\s+T\s+test_entry$", nm, re.M).group(1), 16)
    subprocess.run([str(Path(gcc).with_name("arm-none-eabi-objcopy.exe")),
                    "-O", "binary", str(elf), str(binary)], check=True)
    env = dict(os.environ)
    env["PYTHONPATH"] = str(ROOT / "build/python-deps")
    subprocess.run([sys.executable, str(ROOT / "tests/timer0_pwm/run_unicorn.py"),
                    str(binary), hex(entry)], check=True, env=env,
                   capture_output=True, text=True)
