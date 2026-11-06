# Linux shared Qt/provider adapters link static C++ module libraries.
# Enable PIC before producers are created so the ELF linker can relocate
# their module object files. Core-only configurations keep their codegen.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
