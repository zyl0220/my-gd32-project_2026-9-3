# Licensing scope and third-party notices

The root [MIT license](LICENSE) applies to original project code and documentation
for which Yunlong Zhao can grant permission. Existing third-party copyright and
license notices remain in force and must be preserved. The root MIT license does
not relicense third-party material.

## Arm CMSIS

Arm CMSIS core/compiler headers in `Firmware/CMSIS/` carry Apache-2.0 notices.
The complete Apache License 2.0 text is retained in
[`Firmware/CMSIS/LICENSE.TXT`](Firmware/CMSIS/LICENSE.TXT).

## GigaDevice device support and example code

GigaDevice device headers and support code in `Firmware/CMSIS/GD/`,
the standard peripheral library in `Firmware/GD32C2x1_standard_peripheral/`,
and other source/header files with GigaDevice copyright notices retain their
existing three-clause BSD-style redistribution terms and disclaimers.
Those notices are retained in the individual files, including vendor-derived
evaluation and application template files in `src/` and `inc/`.

## Other vendor-generated support files

The startup assembly in `gcc_startup/`, linker support in `ldscripts/`,
`GD32C231.svd`, and other vendor-generated device support are excluded from any
claim that their original contents are owned or relicensed by this project's
maintainer. Any upstream notices and terms applicable to those files continue
to apply.

For files containing both original changes and third-party material, preserve
the third-party notices and comply with their terms as well as the MIT terms
for the original contributions.

## User-provided compressor FOC starter

`third_party/compressor_foc_starter/` preserves all files from the user-provided
`compressor_foc_starter (1).zip`. The production copies are
`inc/motor/field_weakening.h`, `inc/motor/motor_profile.h`, and
`src/motor/field_weakening.c` (only its include path is adapted).
No explicit license file or license grant was present in the supplied archive;
this project does not grant or infer a new license for those files.
