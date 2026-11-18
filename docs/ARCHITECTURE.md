# Architecture

- Features are organized into modular folders under the `ln/` directory. A
  feature folder and its C++ namespace share the same name, see
  [Casing](CODING_STYLE.md#casing).
- Everything under [`ln/`](ln/) must be platform-independent wherever possible,
  unless the feature is inherently platform-specific.
- Port-specific code should reside under the `port/` directory. Ports are
  managed in CMake.
- [`core`](ln/core/) is a special component - the only shared foundation for all
  other modules. It contains ubiquitous generic features for embedded systems
  while trying to govern good coding practices with uncompromised performance in
  mind, as well as interfaces for modularity.
- Modules can relate to each other **only** through well-defined interfaces in
  [`core`](ln/core/) to provide additional combined functionality.

By following these rules, it should be possible to cherry-pick any meaningful
feature from `ln/` together with [`core`](ln/core/) and one of the `port/`
implementations to use it in a project.
