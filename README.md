# minicc

A small compiler written from scratch in C++17 (single file, ~200 lines) for a tiny integer language.

Pipeline: **lexer → recursive-descent parser → AST → constant folding / dead-branch elimination → three-address-code IR → dead-code elimination → backend** (IR interpreter, IR dump, or PTX-style assembly).

## Language
Integer variables, `+ - * / %`, comparisons, `if/else`, `while`, `print`. Variables must be assigned before use.
```
let a = 48; let b = 18;
while (b != 0) { let t = a % b; a = b; b = t; }
print a;   # 6
```

## Usage
```
make
./minicc run  tests/gcd.mc          # interpret the IR
./minicc ir   tests/fold.mc         # show optimized IR
./minicc ir   tests/fold.mc --no-opt
./minicc ptx  tests/gcd.mc          # PTX-style assembly (virtual 64-bit registers, setp/selp/bra)
./run_tests.sh                      # every test, with and without optimizations
```

## Optimizations
- **Constant folding** at the AST level (guards against folding division by zero).
- **Dead-branch elimination**: `if` with a constant condition, `while (0)`.
- **Dead-code elimination** on the IR to a fixpoint: values that are never read (dead stores and their temporaries) are removed.

## Notes / limitations
- The PTX output is *PTX-style*: it uses PTX instruction forms and syntax, but has not been checked with `ptxas`, and `print` is emitted as a comment.
- No register allocation (one virtual register per variable/temp), no scoping, 64-bit signed integers only.
