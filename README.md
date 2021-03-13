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
$ make
```

It takes **extremely** long time.  
For example, it takes 4 minutes to build messer on an AMD Ryzen7 2700X.  
It is recommended to take coffee while building.

```
...patience...
...patience...
...patience...
```

## Usage

You can input programs following the prompt (`>>>`).  
`#pragma step tokens` shows macro replacement steps for `tokens`.  
You can exit Messer with `C-d`.

### Example

```
>>> #define CAT_I(a, b) a ## b
>>> #define ID(x) x
>>> CAT_I(a, ID(b))
aID(b)
>>> #pragma step CAT_I(a, ID(b))
   CAT_I(a, ID(b))
-> a ## ID(b)
-> aID(b)
>>> #define CAT(a, b) CAT_I(a, b)
>>> CAT(a, ID(b))
ab
>>> #pragma step CAT(a, ID(b))
   CAT(a, ID(b))
-> CAT_I(a, ID(b))
-> CAT_I(a, b)
-> a ## b
-> ab
>>> 
```

## License

MIT License (see `LICENSE` file)

### License of dependent libraries

see `LICENSES` directory.
