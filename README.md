# `ln`

`ln` is a modular and portable system for Cortex-M newlib RTOS development in
modern C++ and CMake. It features an `{fmt}`-based logger, a shell (CLI), Lua
(interpreter via CLI), a custom multi-thread stack profiler that works with
[Tracy GUI](https://github.com/wolfpld/tracy) and more. It integrates with some
great third-party libraries ([extern/](extern/)). For more information, see the
documentation below.

- [Architecture](docs/ARCHITECTURE.md)
- [Coding Style](docs/CODING_STYLE.md)
- [Environment](docs/ENVIRONMENT.md)
- [TODO](docs/TODO.md)
- [License](LICENSE) (GPL-2.0-only)

## Getting Started

Clone with submodules:

```shell
git clone --recurse-submodules https://github.com/lukasnee/ln.git
```

Then, in the project root:

```shell
pre-commit install
cmake --workflow tests
cmake --workflow arm-none-eabi-gcc-dbg
```
