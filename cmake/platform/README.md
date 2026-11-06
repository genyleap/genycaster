# Platform-specific CMake configuration

The shared build is composed in the root CMakeLists.txt. Every deployment
target has exactly one platform configuration file named configure.cmake:

| Target | Responsibilities |
| --- | --- |
| Android | NDK compilation, test policy, target OpenSSL, portable Boost, Qt TLS, ELF alignment |
| iOS | iOS SDK boundary; package metadata is in properties/iOS |
| macOS | Objective-C++26 compiler and SDK verification, WebKit integration, .app casing migration |
| Windows | Boost.JSON linkage policy |
| Linux | Position-independent code for Qt presentation libraries |
| WebAssembly | Browser sandbox policy and portable Boost headers |

These files group platform behaviors by concern and expose named procedures
that the shared CMake composition calls at the correct lifecycle stage.
Do not create extra named .cmake files for individual bugs or dependencies
within platform directories.

Packaging (icons, manifests, identity, entitlements) is separately owned by
properties/<platform>/Configure.cmake and must not be confused with compiler
and linker configuration.

No build path reduces C++26, disables diagnostic categories or downgrades
native SDK security requirements to make a build appear green.
