from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def test_field_weakening_lifecycle_on_cortex_m23(tmp_path: Path) -> None:
    gcc = shutil.which("arm-none-eabi-gcc")
    assert gcc
    elf, binary = tmp_path / "fw.elf", tmp_path / "fw.bin"
    source = ROOT / "tests/motor/test_field_weakening_lifecycle.c"
    command = [gcc, "-mcpu=cortex-m23", "-mthumb", "-Os", "-std=gnu11",
               "-Wall", "-Wextra", "-Werror", "-ffreestanding", "-fno-builtin",
               "-ffunction-sections", "-fdata-sections", "-nostdlib", "-I", str(ROOT / "inc"), str(source)]
    command += ["-include", str(ROOT / "tests/motor/field_weakening_parameters.h")]
    command += [str(ROOT / "src/motor" / name) for name in
                ["field_weakening.c", "field_weakening_adapter.c", "foc.c", "foc_math.c", "pi.c", "observer.c", "current_sample.c", "protection.c", "speed_ctrl.c"]]
    toolchain = Path(gcc).parent.parent
    libgcc = next((toolchain / "lib/gcc/arm-none-eabi").glob("*/thumb/v8-m.base/nofp/libgcc.a"))
    command += ["-L", str(libgcc.parent), "-L", str(toolchain / "arm-none-eabi/lib/thumb/v8-m.base/nofp")]
    command += [f"-Wl,-T,{ROOT / 'tests/timer0_pwm/linker.ld'}", "-Wl,--gc-sections", "-Wl,--start-group", "-lm", "-lc", "-lgcc", "-Wl,--end-group", "-o", str(elf)]
    compiled = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    nm = subprocess.run([str(Path(gcc).with_name("arm-none-eabi-nm.exe")), "--defined-only", str(elf)],
                        check=True, capture_output=True, text=True).stdout
    entry = re.search(r"^([0-9a-fA-F]+)\s+T\s+test_entry$", nm, re.M)
    assert entry
    subprocess.run([str(Path(gcc).with_name("arm-none-eabi-objcopy.exe")), "-O", "binary", str(elf), str(binary)], check=True)
    result = subprocess.run([sys.executable, str(ROOT / "tests/motor/run_motor_image.py"), str(binary), "0x" + entry[1], str(source)],
                            cwd=ROOT, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
