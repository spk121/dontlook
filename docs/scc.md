# Stipple - MISRA-C Compliant Language Interpreter

## Software Design Document

### 1. Overview

Stipple is a MISRA-C compliant interpreted language with syntax roughly similar to POSIX shell. The interpreter consists of a parser/lexer frontend that converts source code into opcodes for a stack-based virtual machine (VM). This document focuses on the VM architecture and design.

### 2. Design Constraints

The VM is designed to comply with MISRA-C guidelines, which impose the following constraints:

- **No dynamic memory allocation**: All data structures use fixed-size arrays
- **No unions**: Opcodes use a fixed format with all possible operand types
- **Type safety**: Explicit type conversions and bounds checking
- **Deterministic behavior**: No undefined or implementation-defined behavior

### 3. VM Architecture

#### 3.1 Core Components

The Stipple VM is a stack-based virtual machine with the following components:

1. **Instruction Memory**: Array of opcodes representing the program
2. **Global Storage**: Fixed-size storage for variables
3. **Stack**: Limited-depth execution stack
4. **Program Counter (PC)**: Points to the current instruction
5. **Condition Flags**: For comparison and branching operations

#### 3.2 Data Structures

##### 3.2.1 Opcode Format

Each opcode has a uniform structure to comply with MISRA-C restrictions:

```c
typedef struct {
    uint16_t opcode;   /* Operation code */
    uint16_t us1;      /* Unsigned short immediate 1 */
    int32_t i1;        /* Integer immediate 1 */
    int32_t i2;        /* Integer immediate 2 */
    int32_t i3;        /* Integer immediate 3 */
    float f1;          /* Float immediate 1 */
    float f2;          /* Float immediate 2 */
} instruction_t;
```

**Field Usage by Operation Type:**
- **opcode**: Operation identifier (0-65535)
- **us1**: Used for small unsigned values, flags, or variable indices
- **i1, i2, i3**: Integer operands, variable indices, or offsets
- **f1, f2**: Floating-point operands

##### 3.2.2 Global Storage

```c
/* Global integer variables */
int32_t intval[256];

/* Global string storage */
#define STRVAL_LEN 128  /* Maximum 128 strings; uint8_t indices (0-127) are sufficient */
char strval[STRVAL_LEN][256];

/* Global float variables */
float floatval[256];
```

##### 3.2.3 Stack Structure

```c
#define STACK_DEPTH 16
#define STACK_VAR_COUNT 16

typedef struct {
    int32_t int_stack[STACK_DEPTH];
    float float_stack[STACK_DEPTH];
    uint8_t str_stack_idx[STACK_DEPTH];  /* Indices into strval */
    int32_t int_vars[STACK_VAR_COUNT];   /* Stack-local integer variables */
    float float_vars[STACK_VAR_COUNT];   /* Stack-local float variables */
    uint8_t str_vars[STACK_VAR_COUNT];   /* Stack-local string indices */
    uint16_t sp;                         /* Stack pointer */
    uint16_t bp;                         /* Base pointer for local variables */
} stack_t;
```

### 4. Opcode Table

The opcodes are organized into categories based on operation type and operand addressing mode.

#### 4.1 Opcode Naming Convention

- **Suffix `_I`**: Immediate operand (value in opcode)
- **Suffix `_G`**: Global variable operand (index in i1)
- **Suffix `_L`**: Local/stack variable operand (index in i1)
- **Suffix `_S`**: Stack-based operand (top of stack)

#### 4.2 Control Flow Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0000 | NOP | No operation | - |
| 0x0001 | HALT | Stop execution | - |
| 0x0002 | JMP | Unconditional jump | i1 = target PC |
| 0x0003 | JZ | Jump if zero flag set | i1 = target PC |
| 0x0004 | JNZ | Jump if zero flag clear | i1 = target PC |
| 0x0005 | JL | Jump if less flag set | i1 = target PC |
| 0x0006 | JG | Jump if greater flag set | i1 = target PC |
| 0x0007 | JLE | Jump if less or equal | i1 = target PC |
| 0x0008 | JGE | Jump if greater or equal | i1 = target PC |
| 0x0009 | CALL | Call subroutine | i1 = target PC |
| 0x000A | RET | Return from subroutine | - |

#### 4.3 Integer Operations

##### 4.3.1 Integer Load/Store Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0010 | LOADI_I | Load immediate integer to stack | i1 = value |
| 0x0011 | LOADI_G | Load global integer to stack | i1 = global index |
| 0x0012 | LOADI_L | Load local integer to stack | i1 = local index |
| 0x0013 | STOREI_G | Store stack top to global integer | i1 = global index |
| 0x0014 | STOREI_L | Store stack top to local integer | i1 = local index |
| 0x0015 | PUSHI | Push integer immediate | i1 = value |
| 0x0016 | POPI | Pop integer (discard) | - |

##### 4.3.2 Integer Arithmetic Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0020 | ADDI_I | Add immediate to stack top | i1 = value |
| 0x0021 | ADDI_S | Add top two stack integers | - |
| 0x0022 | SUBI_I | Subtract immediate from stack top | i1 = value |
| 0x0023 | SUBI_S | Subtract top two stack integers | - |
| 0x0024 | MULI_I | Multiply stack top by immediate | i1 = value |
| 0x0025 | MULI_S | Multiply top two stack integers | - |
| 0x0026 | DIVI_I | Divide stack top by immediate | i1 = value |
| 0x0027 | DIVI_S | Divide top two stack integers | - |
| 0x0028 | MODI_I | Modulo stack top by immediate | i1 = value |
| 0x0029 | MODI_S | Modulo top two stack integers | - |
| 0x002A | NEGI | Negate integer on stack top | - |
| 0x002B | INCI_G | Increment global integer | i1 = global index |
| 0x002C | DECI_G | Decrement global integer | i1 = global index |

##### 4.3.3 Integer Bitwise Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0030 | ANDI_I | Bitwise AND with immediate | i1 = value |
| 0x0031 | ANDI_S | Bitwise AND top two stack values | - |
| 0x0032 | ORI_I | Bitwise OR with immediate | i1 = value |
| 0x0033 | ORI_S | Bitwise OR top two stack values | - |
| 0x0034 | XORI_I | Bitwise XOR with immediate | i1 = value |
| 0x0035 | XORI_S | Bitwise XOR top two stack values | - |
| 0x0036 | NOTI | Bitwise NOT stack top | - |
| 0x0037 | SHLI_I | Shift left by immediate | i1 = shift count |
| 0x0038 | SHRI_I | Shift right by immediate | i1 = shift count |

##### 4.3.4 Integer Comparison Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0040 | CMPI_I | Compare stack top with immediate | i1 = value |
| 0x0041 | CMPI_S | Compare top two stack integers | - |
| 0x0042 | CMPI_G | Compare stack top with global | i1 = global index |
| 0x0043 | TSTI_I | Test (AND) stack top with immediate | i1 = value |
| 0x0044 | TSTI_S | Test (AND) top two stack values | - |

#### 4.4 Float Operations

##### 4.4.1 Float Load/Store Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0050 | LOADF_I | Load immediate float to stack | f1 = value |
| 0x0051 | LOADF_G | Load global float to stack | i1 = global index |
| 0x0052 | LOADF_L | Load local float to stack | i1 = local index |
| 0x0053 | STOREF_G | Store stack top to global float | i1 = global index |
| 0x0054 | STOREF_L | Store stack top to local float | i1 = local index |
| 0x0055 | PUSHF | Push float immediate | f1 = value |
| 0x0056 | POPF | Pop float (discard) | - |

##### 4.4.2 Float Arithmetic Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0060 | ADDF_I | Add immediate to stack top | f1 = value |
| 0x0061 | ADDF_S | Add top two stack floats | - |
| 0x0062 | SUBF_I | Subtract immediate from stack top | f1 = value |
| 0x0063 | SUBF_S | Subtract top two stack floats | - |
| 0x0064 | MULF_I | Multiply stack top by immediate | f1 = value |
| 0x0065 | MULF_S | Multiply top two stack floats | - |
| 0x0066 | DIVF_I | Divide stack top by immediate | f1 = value |
| 0x0067 | DIVF_S | Divide top two stack floats | - |
| 0x0068 | NEGF | Negate float on stack top | - |
| 0x0069 | ABSF | Absolute value of float | - |
| 0x006A | SQRTF | Square root of float | - |

##### 4.4.3 Float Comparison Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0070 | CMPF_I | Compare stack top with immediate | f1 = value |
| 0x0071 | CMPF_S | Compare top two stack floats | - |
| 0x0072 | CMPF_G | Compare stack top with global | i1 = global index |

#### 4.5 String Operations

##### 4.5.1 String Load/Store Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0080 | LOADS_I | Load string index immediate | i1 = string index |
| 0x0081 | LOADS_G | Load global string index | i1 = global index |
| 0x0082 | LOADS_L | Load local string index | i1 = local index |
| 0x0083 | STORES_G | Store string index to global | i1 = global index |
| 0x0084 | STORES_L | Store string index to local | i1 = local index |
| 0x0085 | PUSHS | Push string index | i1 = string index |
| 0x0086 | POPS | Pop string index (discard) | - |

##### 4.5.2 String Manipulation Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0090 | CATS | Concatenate two strings | i1 = dest idx, i2 = src1 idx, i3 = src2 idx |
| 0x0091 | CATS_S | Concatenate stack strings | i1 = dest idx |
| 0x0092 | COPYS | Copy string | i1 = dest idx, i2 = src idx |
| 0x0093 | LENS | Get string length | i1 = string idx (result to int stack) |
| 0x0094 | SUBS | Substring | i1 = dest idx, i2 = src idx, i3 = start pos, us1 = length (uint16_t) |
| 0x0095 | CLRS | Clear string | i1 = string idx |
| 0x0096 | CHRS | Get character at index | i1 = string idx, i2 = char idx |
| 0x0097 | SETCHRS | Set character at index | i1 = string idx, i2 = char idx, i3 = char value |

##### 4.5.3 String Comparison Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00A0 | CMPS | Compare two strings | i1 = string idx 1, i2 = string idx 2 |
| 0x00A1 | CMPS_S | Compare stack string indices | - |
| 0x00A2 | FINDS | Find substring | i1 = haystack idx, i2 = needle idx |

##### 4.5.4 String Conversion Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00B0 | STOI | String to integer | i1 = string idx (result to int stack) |
| 0x00B1 | STOF | String to float | i1 = string idx (result to float stack) |
| 0x00B2 | ITOS | Integer to string | i1 = dest string idx (int from stack) |
| 0x00B3 | FTOS | Float to string | i1 = dest string idx (float from stack) |

#### 4.6 Type Conversion Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00C0 | ITOF | Convert integer to float | - |
| 0x00C1 | FTOI | Convert float to integer (truncate) | - |
| 0x00C2 | FTOI_R | Convert float to integer (round) | - |

#### 4.7 Stack Manipulation Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00D0 | DUP_I | Duplicate top integer | - |
| 0x00D1 | DUP_F | Duplicate top float | - |
| 0x00D2 | DUP_S | Duplicate top string index | - |
| 0x00D3 | SWAP_I | Swap top two integers | - |
| 0x00D4 | SWAP_F | Swap top two floats | - |
| 0x00D5 | SWAP_S | Swap top two string indices | - |
| 0x00D6 | ROT3_I | Rotate top 3 integers | - |
| 0x00D7 | ROT3_F | Rotate top 3 floats | - |

#### 4.8 I/O Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00E0 | PRINTI | Print integer from stack | - |
| 0x00E1 | PRINTF | Print float from stack | - |
| 0x00E2 | PRINTS | Print string | i1 = string idx |
| 0x00E3 | PRINTLN | Print newline | - |
| 0x00E4 | READI | Read integer to stack | - |
| 0x00E5 | READF | Read float to stack | - |
| 0x00E6 | READS | Read string | i1 = dest string idx |

#### 4.9 Memory and System Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00F0 | ALLOC_FRAME | Allocate stack frame | i1 = local var count |
| 0x00F1 | FREE_FRAME | Free stack frame | - |
| 0x00F2 | CLEARI_G | Clear global integer | i1 = global idx |
| 0x00F3 | CLEARF_G | Clear global float | i1 = global idx |
| 0x00F4 | CLEARS_G | Clear global string | i1 = global idx |

### 5. VM Execution Model

#### 5.1 Initialization

On VM initialization:
1. All global variables are zeroed (`intval`, `floatval`)
2. All strings are cleared (empty strings)
3. Stack pointer (SP) is set to 0
4. Base pointer (BP) is set to 0
5. Program counter (PC) is set to 0
6. Condition flags are cleared

#### 5.2 Instruction Fetch-Decode-Execute Cycle

```
while PC < program_length and opcode != HALT:
    1. Fetch: instruction = program[PC]
    2. Decode: opcode = instruction.opcode
    3. Execute: Perform operation based on opcode
    4. Increment PC (unless jump/call/return modified it)
    5. Check for errors (stack overflow/underflow, division by zero, etc.)
```

#### 5.3 Stack Management

**Integer Stack Operations:**
- Push: `int_stack[sp] = value; sp++;`
- Pop: `sp--; value = int_stack[sp];`
- Overflow check: `sp >= STACK_DEPTH`
- Underflow check: `sp == 0`

**Float Stack Operations:**
- Similar to integer stack, using `float_stack` array

**String Stack Operations:**
- Stack stores indices into the `strval` array
- Actual string data remains in `strval`
- String operations manipulate the indexed strings

#### 5.4 Variable Addressing

**Global Variables:**
- Direct indexing: `intval[index]`, `floatval[index]`, `strval[index]`
- Index range: 0-255
- Bounds checking required before access

**Local Variables:**
- Accessed relative to base pointer: `int_vars[bp + index]`
- Index range: 0-15
- Frame allocation updates BP and reserves space

#### 5.5 Function Calls

**CALL Operation:**
1. Push current PC + 1 to a return address stack
2. Push current BP to BP stack
3. Set PC to target address (i1)
4. Allocate new stack frame (via ALLOC_FRAME)

**RET Operation:**
1. Free current stack frame (via FREE_FRAME)
2. Restore BP from BP stack
3. Pop return address from return address stack
4. Set PC to return address

#### 5.6 Condition Flags

The VM maintains condition flags set by comparison operations:
- **Zero (Z)**: Result is zero or values are equal
- **Less (L)**: First operand is less than second
- **Greater (G)**: First operand is greater than second

Flags are updated by CMP and TST operations and tested by conditional jumps.

#### 5.7 Error Handling

The VM detects and reports the following error conditions:

1. **Stack Overflow**: SP exceeds STACK_DEPTH
2. **Stack Underflow**: Pop operation when SP == 0
3. **Division by Zero**: Integer or float division by zero
4. **Invalid Variable Index**: Index out of bounds for globals or locals
5. **Invalid String Index**: String index >= STRVAL_LEN
6. **Invalid Opcode**: Unrecognized opcode value
7. **Invalid PC**: PC out of program bounds

### 6. MISRA-C Compliance Details

#### 6.1 Memory Management

- **Static Allocation**: All arrays are fixed-size and statically allocated
- **No malloc/free**: The `ALLOC_FRAME`/`FREE_FRAME` opcodes manage logical frames, not dynamic memory
- **Bounds Checking**: All array accesses are validated against array bounds
- **No Pointer Arithmetic**: Array indexing uses subscript notation only

#### 6.2 Type Safety

- **No Unions**: Separate stacks for int, float, and string indices
- **Explicit Casts**: Type conversions use dedicated opcodes (ITOF, FTOI)
- **Fixed-Width Types**: Use `int32_t`, `uint16_t`, `uint8_t` for integer types
- **Float Requirement**: Implementation requires IEEE 754 single-precision (32-bit) floating point support
- **Enum for Opcodes**: Opcodes defined as enumeration constants

#### 6.3 Deterministic Behavior

- **Defined Overflow**: Integer operations use well-defined wraparound
- **No Undefined Behavior**: All edge cases explicitly handled
- **Predictable Execution**: No implementation-defined behavior in VM core

#### 6.4 Code Structure

```c
/* Example VM state structure */
typedef struct {
    instruction_t program[1024];     /* Program memory */
    uint16_t program_length;         /* Number of instructions */
    uint16_t pc;                     /* Program counter */
    stack_t stack;                   /* Execution stack */
    int32_t intval[256];            /* Global integers */
    float floatval[256];            /* Global floats */
    char strval[128][256];          /* Global strings */
    uint8_t flags;                  /* Condition flags (Z, L, G) */
    uint16_t return_stack[16];      /* Return addresses */
    uint16_t return_sp;             /* Return stack pointer */
    uint16_t bp_stack[16];          /* Base pointer stack */
    uint16_t bp_sp;                 /* BP stack pointer */
} vm_state_t;
```

### 7. Performance Considerations

#### 7.1 Memory Footprint

- **Instruction Memory**: ~24-28 bytes × 1024 = ~24-28 KB (each instruction_t is nominally 24 bytes: 2+2+4+4+4+4+4, but may be 28 bytes with struct padding for alignment)
- **Global Storage**: 256×4 (intval) + 256×4 (floatval) + 128×256 (strval) = 1024 + 1024 + 32768 = ~34 KB
- **Stack Storage**: ~2 KB per execution context
- **Total**: Approximately 60-64 KB base footprint (depending on struct padding)

#### 7.2 Execution Speed

- **Single-cycle operations**: Most arithmetic and logical operations
- **Multi-cycle operations**: String operations, I/O operations
- **No cache misses**: All data in fixed arrays (cache-friendly)
- **Predictable timing**: No dynamic allocation or unbounded loops

### 8. Example Program

Here's a simple example showing how a program would be encoded:

```
# High-level pseudocode:
# x = 10
# y = 20
# z = x + y
# print(z)

# Assembled opcodes:
0: LOADI_I   i1=10          # Load 10
1: STOREI_G  i1=0           # Store to global[0] (x)
2: LOADI_I   i1=20          # Load 20
3: STOREI_G  i1=1           # Store to global[1] (y)
4: LOADI_G   i1=0           # Load x
5: LOADI_G   i1=1           # Load y
6: ADDI_S                   # Add stack values
7: STOREI_G  i1=2           # Store to global[2] (z)
8: LOADI_G   i1=2           # Load z
9: PRINTI                   # Print integer
10: HALT                    # Stop
```

### 9. Future Extensions

Potential enhancements while maintaining MISRA-C compliance:

1. **Extended String Operations**: Regular expression matching, formatting
2. **Array Operations**: Support for array indexing and iteration
3. **Debugging Support**: Breakpoint and trace opcodes
4. **Optimization**: Peephole optimization of opcode sequences
5. **Error Recovery**: Exception handling opcodes
6. **File I/O**: File operations for persistent storage
7. **Interoperability**: Foreign function interface for C library calls

### 10. Conclusion

The Stipple VM provides a robust, MISRA-C compliant execution environment for a shell-like scripting language. The fixed opcode format and static memory allocation ensure predictable behavior and compliance with safety-critical coding standards. The comprehensive opcode set supports integer, floating-point, and string operations suitable for systems programming and scripting tasks.
