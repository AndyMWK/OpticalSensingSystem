# Linting this project

`clang-format` and `clang-tidy` run as pre-commit hooks (see `.pre-commit-config.yaml` at
the repo root). They are scoped to hand-written code only:

| In scope | Out of scope |
| --- | --- |
| `Core/Src/modules/*.c` | `Core/Src/{main,stm32g0xx_it,stm32g0xx_hal_msp,syscalls,sysmem,system_stm32g0xx}.c` |
| `Core/Inc/*.h` (ours) | `Core/Inc/{main,stm32g0xx_it,stm32g0xx_hal_conf}.h` |
| `BSP/**` | `Drivers/**` (CMSIS + STM32 HAL) |
| `tests/**` (clang-format only) | `Firmware/device/gen2_fw`, `Firmware/mock` |

CubeMX-generated and vendor files are excluded because CubeMX rewrites them on regeneration
and their naming is ST's, not ours.

## `compile_flags.txt`

clang-tidy has to parse a full translation unit, so it needs the cross-compile flags. There
is no `compile_commands.json` available at commit time — CMake writes it to `build/Debug/`,
which is gitignored, and the hook only looks for it in `build/`, `out/`, `cmake-build-debug/`
and `_build/` relative to the *repo root*.

Instead, `compile_flags.txt` sits at the root of this project. clang-tidy's
`FixedCompilationDatabase` walks up from each source file until it finds one, so it applies
to everything under `target_platform/` with no build step and no CI dependency. Paths inside
it resolve relative to its own directory.

Without it you get ~20 hard errors per file: `'fsm.h' file not found`,
`cmsis_compiler.h: Unknown compiler.`, `unknown type name '__STATIC_INLINE'`. The
`--target=arm-none-eabi` line fixes the CMSIS ones; `--sysroot` is what lets `<string.h>`
and friends resolve.

### ⚠ The `--sysroot` line is machine-specific

```
--sysroot=C:/ST/STM32CubeCLT_1.19.0/GNU-tools-for-STM32/arm-none-eabi
```

That is the newlib installation shipped with STM32CubeCLT on this workstation. On any other
machine — different CubeCLT version, different install path, Linux — **edit this one line**.
Find the right value with:

```
arm-none-eabi-gcc -print-sysroot
```

`compile_flags.txt` has no comment syntax (every line is passed to the compiler verbatim),
which is why this warning lives here instead of inline.

### Keeping the include list current

The `-D` and `-I` lines mirror `MX_Defines_Syms` and `MX_Include_Dirs` in
`cmake/stm32cubemx/CMakeLists.txt`, plus `BSP/Dev/Inc` and `BSP/Port/Inc`, which are not in
the CMake build yet. If CubeMX adds an include path or a symbol, mirror it here too.

## Tool versions

`--version=22.1.8` is pinned on both hooks in `.pre-commit-config.yaml`. Left unpinned, the
hook re-resolves the newest wheel from PyPI on every run and pip-installs it, which races
across parallel hook processes and fails with `Access is denied` on Windows. The cost of
pinning is that the hooks need network access to validate the version — they will not run
offline. To bump, change the version in both `args:` lists and run
`pre-commit clean && pre-commit install --install-hooks`.
