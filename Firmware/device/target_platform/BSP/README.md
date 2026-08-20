This is for a future feature in which a BSP layer exists between application code and STMicroelectornics' HAL layer. 

This is done such that different versions of the boards can be supported with different peripherals and IC's. 

BSP/HAL -> thin wrapper for the actual HAL functions. 

BSP/

---

Code under `BSP/` is linted by the pre-commit hooks. If clang-tidy reports include errors,
see [../LINTING.md](../LINTING.md) — the `--sysroot` line in `compile_flags.txt` is
machine-specific and may need adjusting.