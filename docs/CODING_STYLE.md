# Coding Style

Rules that the formatter and `clang-tidy` cannot enforce. Formatting itself is
handled by `.clang-format`.

## General

- Code should always have a good reason to exist.
- Never rush adding more API (methods, virtual functions, options) than currently
needed.

## Casing

- Namespace: `snake_case`.
- Class and Struct: `PascalCase`.
- Function and Method: `snake_case`.
- Variable: `snake_case`.
- Constant: `SCREAMING_SNAKE_CASE`.
- Enum: `PascalCase`.
- Enum Value: `snake_case`.

> The scheme is mostly drawn from [Rust Naming](https://rust-lang.github.io/api-guidelines/naming.html#casing-conforms-to-rfc-430-c-case).

## File Tree, Namespaces and Classes

- File tree should mirror the namespace and class structure. For example, the
  [`ln/drivers`](../ln/drivers) folder contains features that are in the
  `ln::drivers` namespace.
- A source file should describe a single class and should match the class name.
  If multiple small related classes in a single file makes more sense than
  splitting them down, then these should be under a common namespace which
  reflects the file name.
- File content should always be encapsulated within the appropriate namespace.
  In general, multiple namespaces in the same file adjacent to each other should
  be avoided.
- A class name states the class's role, so a reader knows what it does without
  opening it. Descriptiveness, clarity in ownership is preferred over brevity.
  The name must not imply more than the class does, such as owning storage or
  controlling hardware it only reads from.

## Functions, Methods and Variables

- Accessor and query methods use a `get_` prefix. Do not name a getter after the
bare noun. For example, use `get_free()` instead of ambiguous `free()`.

## Includes

List `#include`s in the following groups, separated by one blank line, in this
order:

1. Project-specific: headers that are part of the current project.
2. General local libraries: `ln/...`, the main library the project is built on
   (`ln/algo`, `ln/drivers`, `ln/log.h`, ...).
3. Port-specific: `ln/port/...`.
4. Third-party: vendor and external libraries (`stm32f3xx_hal.h`, `fmt`,
   Catch2, ...).
5. C and C++ standard library (`<cstdint>`, `<span>`, ...).

Alphabetical ordering is recommended within each group.

```cpp
#include "project_specific_header.h"

#include "ln/algo/CircularDmaReader.hpp"
#include "ln/drivers/EventDrivenStream.hpp"
#include "ln/log.h"

#include "ln/port/IrqLock.hpp"

#include "stm32f3xx_hal.h"

#include <cstddef>
#include <cstdint>
#include <span>
```

## Control Flow

Prefer early return. Handle each failure with its own `if (!ok) { return false; }`
and end with the success return, instead of nested `if/else` or a `result`
variable.

```cpp
bool process(const Input &input) {
    if (!validate(input)) {
        return false;
    }
    if (!prepare()) {
        return false;
    }
    if (!commit(input)) {
        return false;
    }
    return true;
}
```
