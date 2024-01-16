Initially based on part 2 of crafting interpreters.

Features:

- Lists
- Dicts
- Interned strings
- NaN Boxing
- Closures
- GC
- Coroutines and generators

Future goals:

- Everything is an expression
- Native objects
- Lua-like tables
- Metaprogramming / metatables
- Channels as coroutine wakers
- Methods incl. methods for primitive types
- Module system
- Gradual typing (compile time checks only)
- LLVM JIT?

TODO:

- so many things (commented)
- port compiler to Rust
- consider Rust port of VM later, so performance can be compared.
