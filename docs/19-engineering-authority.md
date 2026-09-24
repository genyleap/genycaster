# 19 — Pinned Modern C++ Engineering Authority

This repository follows `thecompez/ai-for-modern-cpp` at:

```text
326cd850c2cb5e39264b7d2ccf2c29fba6627380
```

The revision is intentionally pinned so Codex, Claude, ChatGPT, and future agents apply the same contract rather than whichever version happens to be current.

## Applied upstream rules

### Modules

- project-owned modules by default;
- `.cppm` for exported contracts;
- `.cpp` for non-trivial implementation;
- standard headers only in global module fragments;
- never `import std;`;
- `FILE_SET CXX_MODULES` and module scanning.

### Naming

- dotted lowercase module names;
- namespace mirrors module identity;
- PascalCase types and enum values;
- lowerCamelCase functions/locals;
- `m_` + lowerCamelCase private members.

### API/error/resource design

- English Doxygen on exported public contracts;
- explicit ownership/lifetime/optionality;
- `std::expected` for recoverable failures;
- deterministic RAII for resources.

### Architecture

- stable abstractions inward;
- volatile provider/SQLite/Qt/OS mechanisms at adapters;
- no vague common/helper buckets;
- no speculative abstraction without demonstrated pressure.

### Qt Quick

- Qt 6 / Qt Quick / QML / Controls;
- application/domain remain C++ modules;
- `.hpp` only for Qt MOC textual boundary;
- explicit customizable Controls style;
- `qt_add_qml_module`;
- strict lint and deterministic smoke verification;
- GUI cannot be called verified from a core-only build.

## Updating the pin

Changing the upstream revision is an architectural policy change. An agent must:

1. inspect the new upstream revision;
2. identify changed normative rules;
3. update this file and `AGENTS.md`;
4. update affected source/CMake/tests/docs;
5. perform clean verification before changing the recorded pin.
