# Messer

Messer is an interactive environment for evaluating C preprocessing macros conforming to C++17 standard with showing macro replacement steps.

## Prerequisites

- You need to install below manually:
    - C++17 supported GNU C++ Compiler
    - Boost
        - Boost.Coroutine2
        - Boost.Preprocessor
- You can get the following prerequisites as git-submodules:
    - Veiler
    - Linse

## Build

```shell-session
$ g++ -std=c++17 -Wall -Wextra -pedantic-errors -O3 -march=native -omesser -Ilinse messer.cpp -lboost_context -lstdc++fs
```

## License

MIT License (see `LICENSE` file)

### License of dependent libraries

see `LICENSES` directory.
