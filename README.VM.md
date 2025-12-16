# Stipple Virtual Machine

## Overview

This directory contains the implementation of the Stipple VM - a MISRA-C compliant stack-based virtual machine for executing Stipple bytecode programs.

## Files

- `src/vm.c` - Core VM implementation
- `src/vm-main.c` - Command-line interface for running bytecode files
- `src/stipple.h` - VM public interface and type definitions
- `docs/sdd.md` - Comprehensive Software Design Document
- `Makefile` - Build system

## Building

```bash
make
```

This produces `build/stipple-vm` executable.

## Usage

```bash
./build/stipple-vm <bytecode_file>
```

Example:
```bash
# Create a simple bytecode program (HALT instruction only)
printf '\x01\x00\x00\x00' > program.bin

# Run it
./build/stipple-vm program.bin
```

## Architecture

The VM implements a stack-based architecture with:

- **64KB instruction memory** for bytecode programs
- **256 global variables** (typed, 8 bytes each)
- **256 memory buffers** (256 bytes each) for arrays/strings
- **32 stack frames** for function calls, each containing:
  - 16 stack variables for parameters/temporaries
  - 64 local variables
  - Return value slot
  - Return address

## Instruction Set

The VM supports ~60 instructions across several categories:

- **Control Flow**: NOP, HALT, JMP, JZ, JNZ, JLT, JGT, JLE, JGE, CALL, RET
- **Load/Store**: LOAD_G, LOAD_L, LOAD_S, LOAD_I_*, STORE_G, STORE_L, STORE_S
- **Arithmetic**: ADD, SUB, MUL, DIV, MOD, NEG (for I32, U32, F32)
- **Bitwise**: AND, OR, XOR, NOT, SHL, SHR (unsigned only, MISRA-C compliant)
- **Comparison**: CMP_I32, CMP_U32, CMP_F32
- **Type Conversion**: I32_TO_U32, U32_TO_I32, I32_TO_F32, etc.
- **Buffer Operations**: BUF_READ, BUF_WRITE, BUF_LEN, BUF_CLEAR
- **String Operations**: STR_CAT, STR_COPY, STR_LEN, STR_CMP, STR_CHR
- **I/O**: PRINT_*, PRINTLN, READ_*

## Current Implementation Status

**Implemented:**
- Core VM infrastructure (init, reset, load_program, step, run)
- Basic instruction execution framework
- Essential instructions: NOP, HALT, LOAD_I_*, PRINT_*, PRINTLN
- Arithmetic: ADD_I32, SUB_I32, MUL_I32, DIV_I32
- Comparisons: CMP_I32, CMP_U32
- Control flow: JMP, JZ, JNZ
- Load/Store: LOAD_G, LOAD_L, STORE_G, STORE_L
- Error handling and validation
- Debug functions (dump_state, disassemble_instruction)

**Remaining Work:**
- Complete remaining ~40 instruction handlers
- Implement CALL/RET with full stack frame management
- Buffer and string operation handlers
- Remaining arithmetic/bitwise operations
- I/O operations (READ_*)
- Comprehensive test suite

## Testing

Basic tests are included in the build:

```bash
make test
```

Create custom bytecode programs using Python's struct module (see examples in test scripts).

## MISRA-C Compliance

The VM adheres to MISRA-C guidelines:

- No dynamic memory allocation
- Fixed-size arrays with bounds checking
- Tagged unions for variant types
- Explicit type conversions
- Bitwise operations on unsigned types only
- Portable instruction encoding (no bitfields)

## References

- See `docs/sdd.md` for complete specification
- See `docs/assembler-sdd.md` for assembler/bytecode format details

## License

See LICENSE file in repository root.
