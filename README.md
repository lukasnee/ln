# `ln`

`ln` is an opinionated collection of reusable modern C++ software for embedded
RTOS development. It tries to maintain high feature value and uncompromised
performance by adhering to good coding practices. And tries to integrate nicely
with like-minded third-party libraries ([extern/](extern/)). It tries it's best
to maintain feature modularity via [`core`](ln/core/) abstractions and
portability via [`port`](port/) layers. It features a logger, a shell (CLI),
Lua, a profiler, etc.

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
