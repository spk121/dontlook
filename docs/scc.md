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

The Stipple VM is a register-based virtual machine with stack support, containing the following components:

1. **Instruction Memory**: Array of opcodes representing the program
2. **Registers**: General-purpose and special-purpose registers for computations
3. **Global Storage**: Fixed-size storage for variables
4. **Stack Frames**: Pre-allocated stack frames (maximum depth 16) for function calls
5. **Program Counter (PC)**: Points to the current instruction
6. **Stack Pointer (SP)**: Points to the current stack frame
7. **Condition Flags**: For comparison and branching operations

#### 3.2 Data Structures

##### 3.2.1 Opcode Format

Each opcode has a uniform structure to comply with MISRA-C restrictions:

```c
typedef struct {
    uint16_t opcode;   /* Operation code */
    uint16_t us1;      /* Unsigned short immediate 1 */
    uint16_t us2;      /* Unsigned short immediate 2 */
    int32_t i1;        /* Signed integer immediate 1 */
    int32_t i2;        /* Signed integer immediate 2 */
    uint32_t ui1;      /* Unsigned integer immediate 1 */
    uint32_t ui2;      /* Unsigned integer immediate 2 */
    float f1;          /* Float immediate 1 */
    float f2;          /* Float immediate 2 */
} instruction_t;
```

**Field Usage by Operation Type:**
- **opcode**: Operation identifier (0-65535)
- **us1, us2**: Used for register selection, variable indices (0-255 range)
- **i1, i2**: Signed integer operands or offsets
- **ui1, ui2**: Unsigned integer operands, addresses, or for bitwise operations
- **f1, f2**: Floating-point operands

**MISRA-C Compliance Note**: Per MISRA-C requirements, bitwise and shift operations are only performed on unsigned types (us1, us2, ui1, ui2). Signed integer types (i1, i2) are not used for bitwise operations.

##### 3.2.2 Registers

The VM provides general-purpose and special-purpose registers:

```c
/* General-purpose signed integer registers */
int32_t A;   /* Accumulator, commonly used for signed integer math */
int32_t B, C, D;   /* General purpose signed integers */

/* General-purpose unsigned integer registers */
uint32_t F;  /* Flags register for overflow, carry, etc. */
uint32_t G, H;  /* General purpose unsigned integers */

/* General-purpose float registers */
float J, K;  /* General purpose floats */

/* Special-purpose registers */
uint16_t SP;  /* Stack pointer (current frame level 0-15) */
uint32_t PC;  /* Program counter (address of next instruction) */
```

**Register Usage Guidelines:**
- **us1** field typically specifies which register to use (0=A, 1=B, 2=C, etc.)
- Signed arithmetic uses A, B, C, D registers
- Unsigned and bitwise operations use F, G, H registers
- Float operations use J, K registers

##### 3.2.3 Global Storage

```c
/* Global signed integer variables */
int32_t intval[256];

/* Global unsigned integer variables */
uint32_t uintval[256];

/* Global string storage pool */
#define STRVAL_LEN 128  /* Maximum 128 strings (indices 0-127); uint8_t type is sufficient */
char strval[STRVAL_LEN][256];

/* Global string variable indices (which string each global string variable references) */
uint8_t strval_idx[256];  /* Indices into strval for global string variables */

/* Global float variables */
float floatval[256];
```

##### 3.2.4 Stack Structure

The VM uses pre-allocated stack frames with a maximum depth of 16. Each frame has dedicated storage for local variables and parameter passing.

```c
#define STACK_DEPTH 16      /* Maximum 16 nested function calls */
#define STACK_VAR_COUNT 16  /* Each frame has 16 slots for variables */

/* Stack variable type identifiers */
typedef enum {
    SV_INT,              /* Signed integer value */
    SV_UINT,             /* Unsigned integer value */
    SV_FLOAT,            /* Float value */
    SV_GLOBAL_INT_IDX,   /* Reference to global signed int (index in us1) */
    SV_GLOBAL_UINT_IDX,  /* Reference to global unsigned int (index in us1) */
    SV_GLOBAL_FLOAT_IDX, /* Reference to global float (index in us1) */
    SV_GLOBAL_STR_IDX,   /* Reference to global string (index in us1) */
    SV_STACK_INT_IDX,    /* Reference to stack int (frame level in us1, var index in us2) */
    SV_STACK_UINT_IDX,   /* Reference to stack uint (frame level in us1, var index in us2) */
    SV_STACK_FLOAT_IDX,  /* Reference to stack float (frame level in us1, var index in us2) */
    SV_STACK_STR_IDX     /* Reference to stack string (frame level in us1, var index in us2) */
} stack_variable_type_t;

/* Stack variable can hold a value or reference */
typedef struct {
    stack_variable_type_t type;
    uint16_t us1;   /* Used for indices and frame levels */
    uint16_t us2;   /* Used for variable indices within frames */
    int32_t i32;    /* Signed integer value */
    uint32_t u32;   /* Unsigned integer value */
    float f32;      /* Float value */
} stack_variable_t;

/* Complete stack frame structure */
typedef struct {
    stack_variable_t vars[STACK_VAR_COUNT];  /* Variables for this frame */
    int32_t int_locals[STACK_VAR_COUNT];     /* Local signed integers */
    uint32_t uint_locals[STACK_VAR_COUNT];   /* Local unsigned integers */
    float float_locals[STACK_VAR_COUNT];     /* Local floats */
    uint8_t str_locals[STACK_VAR_COUNT];     /* Local string indices */
    uint32_t return_addr;                    /* Return address (PC) */
} stack_frame_t;

/* Stack frame array */
stack_frame_t stack_frames[STACK_DEPTH];
```

**Stack Frame Usage:**
- **vars[]**: Used for passing parameters and return values between caller/callee
- **int_locals[], uint_locals[], float_locals[], str_locals[]**: Frame-local variable storage
- **return_addr**: Saved PC for function return
- Frames are pre-allocated; SP indicates current frame level (0-15)
- When calling a function, caller pushes values into `vars[]` of next frame level
- Callee accesses parameters from its own `vars[]` and uses `*_locals[]` for local variables
- On return, callee places return values in its own `vars[]` for caller to retrieve

### 4. Opcode Table

The opcodes are organized into categories based on operation type and operand addressing mode.

#### 4.1 Opcode Naming Convention

- **Suffix `_I`**: Immediate operand (value in opcode)
- **Suffix `_R`**: Register operand (register specified by us1)
- **Suffix `_G`**: Global variable operand (index in us1 or us2)
- **Suffix `_L`**: Local/stack variable operand (index in us1 or us2)
- **Suffix `_V`**: Stack variable operand (from vars[] array)

#### 4.2 Control Flow Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0000 | NOP | No operation | - |
| 0x0001 | HALT | Stop execution | - |
| 0x0002 | JMP | Unconditional jump | ui1 = target PC |
| 0x0003 | JZ | Jump if zero flag set | ui1 = target PC |
| 0x0004 | JNZ | Jump if zero flag clear | ui1 = target PC |
| 0x0005 | JL | Jump if less flag set | ui1 = target PC |
| 0x0006 | JG | Jump if greater flag set | ui1 = target PC |
| 0x0007 | JLE | Jump if less or equal | ui1 = target PC |
| 0x0008 | JGE | Jump if greater or equal | ui1 = target PC |
| 0x0009 | CALL | Call subroutine | ui1 = target PC |
| 0x000A | RET | Return from subroutine | - |

#### 4.3 Signed Integer Operations

##### 4.3.1 Signed Integer Load/Store Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0010 | LOADI_I | Load immediate to register | us1 = reg (0-3 for A-D), i1 = value |
| 0x0011 | LOADI_G | Load global integer to register | us1 = reg, us2 = global index |
| 0x0012 | LOADI_L | Load local integer to register | us1 = reg, us2 = local index |
| 0x0013 | STOREI_G | Store register to global integer | us1 = reg, us2 = global index |
| 0x0014 | STOREI_L | Store register to local integer | us1 = reg, us2 = local index |
| 0x0015 | MOVI_R | Move between integer registers | us1 = dest reg, us2 = src reg |

##### 4.3.2 Signed Integer Arithmetic Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0020 | ADDI_I | Add immediate to register | us1 = reg, i1 = value |
| 0x0021 | ADDI_R | Add two registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0022 | SUBI_I | Subtract immediate from register | us1 = reg, i1 = value |
| 0x0023 | SUBI_R | Subtract registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0024 | MULI_I | Multiply register by immediate | us1 = reg, i1 = value |
| 0x0025 | MULI_R | Multiply registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0026 | DIVI_I | Divide register by immediate | us1 = reg, i1 = value |
| 0x0027 | DIVI_R | Divide registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0028 | MODI_I | Modulo register by immediate | us1 = reg, i1 = value |
| 0x0029 | MODI_R | Modulo registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x002A | NEGI | Negate integer register | us1 = reg |
| 0x002B | INCI_G | Increment global integer | us1 = global index |
| 0x002C | DECI_G | Decrement global integer | us1 = global index |
| 0x002D | INCI_R | Increment register | us1 = reg |
| 0x002E | DECI_R | Decrement register | us1 = reg |

##### 4.3.3 Signed Integer Comparison Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0030 | CMPI_I | Compare register with immediate | us1 = reg, i1 = value |
| 0x0031 | CMPI_R | Compare two registers | us1 = reg1, us2 = reg2 |
| 0x0032 | CMPI_G | Compare register with global | us1 = reg, us2 = global index |

#### 4.4 Unsigned Integer Operations

##### 4.4.1 Unsigned Integer Load/Store Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0040 | LOADU_I | Load immediate to unsigned register | us1 = reg (0-2 for F-H), ui1 = value |
| 0x0041 | LOADU_G | Load global unsigned to register | us1 = reg, us2 = global index |
| 0x0042 | LOADU_L | Load local unsigned to register | us1 = reg, us2 = local index |
| 0x0043 | STOREU_G | Store register to global unsigned | us1 = reg, us2 = global index |
| 0x0044 | STOREU_L | Store register to local unsigned | us1 = reg, us2 = local index |
| 0x0045 | MOVU_R | Move between unsigned registers | us1 = dest reg, us2 = src reg |

##### 4.4.2 Unsigned Integer Arithmetic Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0050 | ADDU_I | Add immediate to unsigned register | us1 = reg, ui1 = value |
| 0x0051 | ADDU_R | Add unsigned registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0052 | SUBU_I | Subtract immediate from unsigned register | us1 = reg, ui1 = value |
| 0x0053 | SUBU_R | Subtract unsigned registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0054 | MULU_I | Multiply unsigned register by immediate | us1 = reg, ui1 = value |
| 0x0055 | MULU_R | Multiply unsigned registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0056 | DIVU_I | Divide unsigned register by immediate | us1 = reg, ui1 = value |
| 0x0057 | DIVU_R | Divide unsigned registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0058 | MODU_I | Modulo unsigned register by immediate | us1 = reg, ui1 = value |
| 0x0059 | MODU_R | Modulo unsigned registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x005A | INCU_G | Increment global unsigned | us1 = global index |
| 0x005B | DECU_G | Decrement global unsigned | us1 = global index |
| 0x005C | INCU_R | Increment unsigned register | us1 = reg |
| 0x005D | DECU_R | Decrement unsigned register | us1 = reg |

##### 4.4.3 Unsigned Integer Bitwise Operations (MISRA-C Compliant)

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0060 | ANDU_I | Bitwise AND with immediate | us1 = reg, ui1 = value |
| 0x0061 | ANDU_R | Bitwise AND registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0062 | ORU_I | Bitwise OR with immediate | us1 = reg, ui1 = value |
| 0x0063 | ORU_R | Bitwise OR registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0064 | XORU_I | Bitwise XOR with immediate | us1 = reg, ui1 = value |
| 0x0065 | XORU_R | Bitwise XOR registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0066 | NOTU | Bitwise NOT register | us1 = reg |
| 0x0067 | SHLU_I | Shift left by immediate | us1 = reg, us2 = shift count (unsigned) |
| 0x0068 | SHRU_I | Logical shift right by immediate | us1 = reg, us2 = shift count (unsigned) |
| 0x0069 | SHLU_R | Shift left by register | us1 = dest, us2 = value reg, i1 = shift reg |
| 0x006A | SHRU_R | Logical shift right by register | us1 = dest, us2 = value reg, i1 = shift reg |

##### 4.4.4 Unsigned Integer Comparison Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0070 | CMPU_I | Compare unsigned register with immediate | us1 = reg, ui1 = value |
| 0x0071 | CMPU_R | Compare two unsigned registers | us1 = reg1, us2 = reg2 |
| 0x0072 | CMPU_G | Compare unsigned register with global | us1 = reg, us2 = global index |
| 0x0073 | TSTU_I | Test (AND) unsigned register with immediate | us1 = reg, ui1 = value |
| 0x0074 | TSTU_R | Test (AND) two unsigned registers | us1 = reg1, us2 = reg2 |

#### 4.5 Float Operations

##### 4.5.1 Float Load/Store Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0080 | LOADF_I | Load immediate float to register | us1 = reg (0-1 for J-K), f1 = value |
| 0x0081 | LOADF_G | Load global float to register | us1 = reg, us2 = global index |
| 0x0082 | LOADF_L | Load local float to register | us1 = reg, us2 = local index |
| 0x0083 | STOREF_G | Store register to global float | us1 = reg, us2 = global index |
| 0x0084 | STOREF_L | Store register to local float | us1 = reg, us2 = local index |
| 0x0085 | MOVF_R | Move between float registers | us1 = dest reg, us2 = src reg |

##### 4.5.2 Float Arithmetic Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0090 | ADDF_I | Add immediate to float register | us1 = reg, f1 = value |
| 0x0091 | ADDF_R | Add float registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0092 | SUBF_I | Subtract immediate from float register | us1 = reg, f1 = value |
| 0x0093 | SUBF_R | Subtract float registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0094 | MULF_I | Multiply float register by immediate | us1 = reg, f1 = value |
| 0x0095 | MULF_R | Multiply float registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0096 | DIVF_I | Divide float register by immediate | us1 = reg, f1 = value |
| 0x0097 | DIVF_R | Divide float registers | us1 = dest, us2 = src1, i1 = src2 reg |
| 0x0098 | NEGF | Negate float register | us1 = reg |
| 0x0099 | ABSF | Absolute value of float register | us1 = reg |
| 0x009A | SQRTF | Square root of float register | us1 = reg |

##### 4.5.3 Float Comparison Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00A0 | CMPF_I | Compare float register with immediate | us1 = reg, f1 = value |
| 0x00A1 | CMPF_R | Compare two float registers | us1 = reg1, us2 = reg2 |
| 0x00A2 | CMPF_G | Compare float register with global | us1 = reg, us2 = global index |

#### 4.6 String Operations

##### 4.6.1 String Load/Store Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00B0 | LOADS_I | Load string index immediate to local | us1 = local var index, us2 = string pool index |
| 0x00B1 | LOADS_G | Load global string index to local | us1 = local var index, us2 = global str var index |
| 0x00B2 | LOADS_L | Copy local string index | us1 = dest local idx, us2 = src local idx |
| 0x00B3 | STORES_G | Store local string index to global | us1 = local var index, us2 = global str var index |

##### 4.6.2 String Manipulation Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00C0 | CATS | Concatenate two strings | us1 = dest pool idx, us2 = src1 pool idx, i1 = src2 pool idx |
| 0x00C1 | COPYS | Copy string | us1 = dest pool idx, us2 = src pool idx |
| 0x00C2 | LENS | Get string length | us1 = src pool idx, us2 = dest int reg (0-3) |
| 0x00C3 | SUBS | Substring | us1 = dest pool idx, us2 = src pool idx, ui1 = start pos, ui2 = length |
| 0x00C4 | CLRS | Clear string | us1 = pool idx |
| 0x00C5 | CHRS | Get character at index | us1 = string pool idx, ui1 = char idx, us2 = dest int reg |
| 0x00C6 | SETCHRS | Set character at index | us1 = string pool idx, ui1 = char idx, i1 = char value |

##### 4.6.3 String Comparison Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00D0 | CMPS | Compare two strings | us1 = string pool idx 1, us2 = string pool idx 2 |
| 0x00D1 | FINDS | Find substring | us1 = haystack pool idx, us2 = needle pool idx, i1 = dest int reg |

##### 4.6.4 String Conversion Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00E0 | STOI | String to signed integer | us1 = string pool idx, us2 = dest int reg (0-3) |
| 0x00E1 | STOU | String to unsigned integer | us1 = string pool idx, us2 = dest uint reg (0-2) |
| 0x00E2 | STOF | String to float | us1 = string pool idx, us2 = dest float reg (0-1) |
| 0x00E3 | ITOS | Signed integer to string | us1 = src int reg, us2 = dest string pool idx |
| 0x00E4 | UTOS | Unsigned integer to string | us1 = src uint reg, us2 = dest string pool idx |
| 0x00E5 | FTOS | Float to string | us1 = src float reg, us2 = dest string pool idx |

#### 4.7 Type Conversion Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x00F0 | ITOF | Convert signed integer to float | us1 = src int reg (0-3), us2 = dest float reg (0-1) |
| 0x00F1 | ITOU | Convert signed integer to unsigned | us1 = src int reg (0-3), us2 = dest uint reg (0-2) |
| 0x00F2 | UTOI | Convert unsigned to signed integer | us1 = src uint reg (0-2), us2 = dest int reg (0-3) |
| 0x00F3 | UTOF | Convert unsigned integer to float | us1 = src uint reg (0-2), us2 = dest float reg (0-1) |
| 0x00F4 | FTOI | Convert float to signed integer (truncate) | us1 = src float reg (0-1), us2 = dest int reg (0-3) |
| 0x00F5 | FTOI_R | Convert float to signed integer (round) | us1 = src float reg (0-1), us2 = dest int reg (0-3) |
| 0x00F6 | FTOU | Convert float to unsigned integer | us1 = src float reg (0-1), us2 = dest uint reg (0-2) |

#### 4.8 Stack Variable Operations

##### 4.8.1 Stack Variable Push/Pop Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0100 | PUSHV_I | Push signed integer value to stack var | us1 = var slot, i1 = value |
| 0x0101 | PUSHV_U | Push unsigned integer value to stack var | us1 = var slot, ui1 = value |
| 0x0102 | PUSHV_F | Push float value to stack var | us1 = var slot, f1 = value |
| 0x0103 | PUSHV_GREF_I | Push global int reference to stack var | us1 = var slot, us2 = global idx |
| 0x0104 | PUSHV_GREF_U | Push global uint reference to stack var | us1 = var slot, us2 = global idx |
| 0x0105 | PUSHV_GREF_F | Push global float reference to stack var | us1 = var slot, us2 = global idx |
| 0x0106 | PUSHV_GREF_S | Push global string reference to stack var | us1 = var slot, us2 = global idx |
| 0x0107 | POPV_I | Pop signed integer value from stack var to register | us1 = var slot, us2 = dest int reg |
| 0x0108 | POPV_U | Pop unsigned integer value from stack var to register | us1 = var slot, us2 = dest uint reg |
| 0x0109 | POPV_F | Pop float value from stack var to register | us1 = var slot, us2 = dest float reg |

#### 4.9 I/O Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0110 | PRINTI | Print signed integer from register | us1 = int reg (0-3) |
| 0x0111 | PRINTU | Print unsigned integer from register | us1 = uint reg (0-2) |
| 0x0112 | PRINTF | Print float from register | us1 = float reg (0-1) |
| 0x0113 | PRINTS | Print string | us1 = string pool idx |
| 0x0114 | PRINTLN | Print newline | - |
| 0x0115 | READI | Read signed integer to register | us1 = dest int reg (0-3) |
| 0x0116 | READU | Read unsigned integer to register | us1 = dest uint reg (0-2) |
| 0x0117 | READF | Read float to register | us1 = dest float reg (0-1) |
| 0x0118 | READS | Read string to pool | us1 = dest string pool idx |

#### 4.10 Memory and System Operations

| Opcode | Name | Description | Operands |
|--------|------|-------------|----------|
| 0x0120 | CLEARI_G | Clear global signed integer | us1 = global idx |
| 0x0121 | CLEARU_G | Clear global unsigned integer | us1 = global idx |
| 0x0122 | CLEARF_G | Clear global float | us1 = global idx |
| 0x0123 | CLEARS_G | Clear global string reference | us1 = global idx |
| 0x0124 | CLEARV | Clear stack variable slot | us1 = var slot |

### 5. VM Execution Model

#### 5.1 Initialization

On VM initialization:
1. All registers are zeroed (A, B, C, D, F, G, H, J, K)
2. All global variables are zeroed (`intval`, `uintval`, `floatval`, `strval_idx`)
3. All strings in pool are cleared (empty strings)
4. Stack pointer (SP) is set to 0 (frame level 0)
5. Program counter (PC) is set to 0
6. Condition flags are cleared
7. All stack frames are zeroed

#### 5.2 Instruction Fetch-Decode-Execute Cycle

```
while PC < program_length and opcode != HALT:
    1. Fetch: instruction = program[PC]
    2. Decode: opcode = instruction.opcode
    3. Execute: Perform operation based on opcode
    4. Increment PC (unless jump/call/return modified it)
    5. Check for errors (stack overflow/underflow, division by zero, etc.)
```

#### 5.3 Register Management

**Register Operations:**
- All arithmetic and logical operations work on registers
- LOAD operations move data from immediates, globals, or locals into registers
- STORE operations move data from registers to globals or locals
- Register values are preserved across instructions unless explicitly modified

**Register Categories:**
- Signed integer registers (A, B, C, D): For signed arithmetic and comparisons
- Unsigned integer registers (F, G, H): For unsigned arithmetic, bitwise ops, addresses
- Float registers (J, K): For floating-point operations
- Special registers (SP, PC): Managed by control flow instructions

#### 5.4 Stack Frame Management

**Stack Frame Operations:**

The VM uses pre-allocated stack frames (maximum depth 16). Each frame is completely independent with its own storage:

- **Frame Structure**:
  ```
  - vars[16]: Parameter passing and return value slots
  - int_locals[16]: Frame-local signed integers
  - uint_locals[16]: Frame-local unsigned integers
  - float_locals[16]: Frame-local floats
  - str_locals[16]: Frame-local string indices
  - return_addr: Saved PC for return
  ```

- **Stack Variable Management**:
  ```c
  // Push value to stack variable (for parameter passing)
  if (var_slot >= STACK_VAR_COUNT) error("invalid var slot");
  stack_frames[SP].vars[var_slot].type = SV_INT;
  stack_frames[SP].vars[var_slot].i32 = value;
  
  // Pop value from stack variable (retrieve parameter/return value)
  if (var_slot >= STACK_VAR_COUNT) error("invalid var slot");
  if (stack_frames[SP].vars[var_slot].type != SV_INT) error("type mismatch");
  value = stack_frames[SP].vars[var_slot].i32;
  ```

- **Overflow check**: Check `SP >= STACK_DEPTH - 1` before CALL
- **Underflow check**: Check `SP == 0` before RET

#### 5.5 Variable Addressing

**Global Variables:**
- Direct indexing: `intval[index]`, `uintval[index]`, `floatval[index]`, `strval_idx[index]`
- Index range: 0-255
- Bounds checking required before access
- Example: `LOADI_G us1=0, us2=5` loads `intval[5]` into register A

**Local Variables:**
- Direct indexing within current frame: `stack_frames[SP].int_locals[index]`
- Index range: 0-15
- Each frame has its own independent local storage
- Example: `STOREI_L us1=1, us2=3` stores register B into `int_locals[3]` of current frame

**String Pool:**
- Global string storage: `strval[pool_index]`
- Pool index range: 0-127
- Global string variables store indices: `strval_idx[var_index]` points to pool entry
- Local string variables store indices: `str_locals[var_index]` points to pool entry

#### 5.6 Function Calls

**CALL Operation:**
1. Check stack overflow: `if (SP >= STACK_DEPTH - 1) error("stack overflow")`
2. Increment SP: `SP++`
3. Zero the new frame's local storage: `memset(&stack_frames[SP], 0, sizeof(stack_frame_t))`
4. Save return address: `stack_frames[SP].return_addr = PC + 1`
5. Set PC to target address: `PC = ui1`

**Parameter Passing:**
- Caller uses PUSHV_* opcodes to place parameters in `stack_frames[SP + 1].vars[]` before CALL
- After CALL, callee accesses parameters from its own `stack_frames[SP].vars[]`
- Parameters can be values (SV_INT, SV_UINT, SV_FLOAT) or references (SV_GLOBAL_*_IDX)

**RET Operation:**
1. Check stack underflow: `if (SP == 0) error("stack underflow")`
2. Restore PC from return address: `PC = stack_frames[SP].return_addr`
3. Decrement SP: `SP--`

**Return Values:**
- Before RET, callee uses PUSHV_* opcodes to place return values in its own `stack_frames[SP].vars[]`
- After RET, caller uses POPV_* opcodes to retrieve return values from `stack_frames[SP + 1].vars[]`

**Example Call Sequence:**
```
// Caller (at frame level 0):
PUSHV_I us1=0, i1=10         // Push param 1 to vars[0] of next frame
PUSHV_I us1=1, i1=20         // Push param 2 to vars[1] of next frame
CALL ui1=<function_addr>      // SP becomes 1

// Callee (now at frame level 1):
POPV_I us1=0, us2=0          // Pop param 1 from vars[0] to register A
POPV_I us1=1, us2=1          // Pop param 2 from vars[1] to register B
ADDI_R us1=0, us2=0, i1=1    // A = A + B
PUSHV_I us1=0, i1=<A>        // Push return value to vars[0]
RET                          // SP becomes 0, PC restored

// Caller (back at frame level 0):
POPV_I us1=0, us2=2          // Pop return value from vars[0] to register C
```

#### 5.7 Condition Flags

The VM maintains condition flags set by comparison operations:
- **Zero (Z)**: Result is zero or values are equal
- **Less (L)**: First operand is less than second
- **Greater (G)**: First operand is greater than second

Flags are updated by CMP and TST operations and tested by conditional jumps.

**Flag Register (F):**
The F register (unsigned) is also used to store flag values and overflow/carry results:
- Bit 0: Zero flag
- Bit 1: Less flag
- Bit 2: Greater flag
- Bit 3: Overflow flag (for signed arithmetic)
- Bit 4: Carry flag (for unsigned arithmetic)

#### 5.8 Error Handling

The VM detects and reports the following error conditions:

1. **Stack Overflow**: SP >= STACK_DEPTH - 1 on CALL
2. **Stack Underflow**: SP == 0 on RET
3. **Division by Zero**: Integer or float division by zero
4. **Invalid Variable Index**: Index out of bounds for globals or locals
5. **Invalid String Pool Index**: String pool index >= STRVAL_LEN
6. **Invalid Register**: Register index out of range for type
7. **Invalid Opcode**: Unrecognized opcode value
8. **Invalid PC**: PC out of program bounds
9. **Type Mismatch**: Stack variable type doesn't match expected type

### 6. MISRA-C Compliance Details

#### 6.1 Memory Management

- **Static Allocation**: All arrays are fixed-size and statically allocated
- **Pre-allocated Frames**: Stack frames are pre-allocated (no dynamic allocation)
- **Bounds Checking**: All array accesses are validated against array bounds
- **No Pointer Arithmetic**: Array indexing uses subscript notation only

#### 6.2 Type Safety

- **Separate Type Storage**: Distinct register sets and storage arrays for signed, unsigned, and float types
- **Explicit Type Conversions**: Type conversions use dedicated opcodes (ITOF, ITOU, UTOI, FTOI, etc.)
- **Fixed-Width Types**: Use `int32_t`, `uint32_t`, `uint16_t`, `uint8_t` for integer types
- **Float Requirement**: Implementation requires IEEE 754 single-precision (32-bit) floating point support
- **Enum for Opcodes**: Opcodes defined as enumeration constants
- **Typed Stack Variables**: Stack variables explicitly track type to prevent type confusion

#### 6.3 Bitwise Operations (MISRA-C Compliant)

Per MISRA-C Rule 10.1: "Shift and bitwise operations shall only be performed on operands of essentially unsigned type"

- **Unsigned-Only Bitwise Ops**: All bitwise operations (AND, OR, XOR, NOT, SHL, SHR) only operate on unsigned registers (F, G, H)
- **Unsigned Shift Operands**: Shift count operands use unsigned types (us2 field)
- **No Signed Bitwise**: Signed integer registers (A, B, C, D) cannot be used for bitwise operations
- **Explicit Type**: Bitwise opcodes explicitly use `ui1`, `ui2` for immediate unsigned operands

#### 6.4 Deterministic Behavior

- **Defined Overflow**: Integer operations use well-defined wraparound semantics
- **No Undefined Behavior**: All edge cases explicitly handled
- **Predictable Execution**: No implementation-defined behavior in VM core
- **Pre-allocated Resources**: Fixed stack depth prevents unbounded recursion

#### 6.5 Code Structure

```c
/* Example VM state structure */
typedef struct {
    instruction_t program[1024];     /* Program memory */
    uint16_t program_length;         /* Number of instructions */
    uint32_t pc;                     /* Program counter */
    uint16_t sp;                     /* Stack pointer (frame level) */
    
    /* Registers */
    int32_t A, B, C, D;             /* Signed integer registers */
    uint32_t F, G, H;               /* Unsigned integer registers */
    float J, K;                     /* Float registers */
    
    /* Global storage */
    int32_t intval[256];            /* Global signed integers */
    uint32_t uintval[256];          /* Global unsigned integers */
    float floatval[256];            /* Global floats */
    char strval[128][256];          /* String pool */
    uint8_t strval_idx[256];        /* Global string variable indices */
    
    /* Stack frames */
    stack_frame_t stack_frames[16]; /* Pre-allocated frames */
    
    /* Condition flags */
    uint8_t flags;                  /* Z, L, G flags */
} vm_state_t;
```

### 7. Performance Considerations

#### 7.1 Memory Footprint

- **Instruction Memory**: ~32-36 KB for 1024 instructions (each instruction_t: 2+2+2+4+4+4+4+4+4 = 30 bytes base, may have padding to 32 or 36 bytes)
- **Registers**: 4×4 (int) + 3×4 (uint) + 2×4 (float) + special = ~44 bytes
- **Global Storage**: 
  - 256×4 (intval) = 1024 bytes
  - 256×4 (uintval) = 1024 bytes
  - 256×4 (floatval) = 1024 bytes
  - 128×256 (strval) = 32768 bytes
  - 256×1 (strval_idx) = 256 bytes
  - **Total**: ~36 KB
- **Stack Frames**: 16 frames × (16×52 bytes/stack_variable + 16×4 + 16×4 + 16×4 + 16×1 + 4) ≈ 16 × 1.1 KB ≈ 18 KB
- **Total**: Approximately 86-90 KB base footprint (depending on struct padding)

#### 7.2 Execution Speed

- **Register-based**: Faster than pure stack-based due to reduced memory access
- **Single-cycle operations**: Most arithmetic, logical, and load/store operations
- **Multi-cycle operations**: String operations, I/O operations, function calls
- **Cache-friendly**: All data in fixed arrays with predictable access patterns
- **Predictable timing**: No dynamic allocation or unbounded loops

### 8. Example Program

Here's a simple example showing how a program would be encoded with the register-based architecture:

```
# High-level pseudocode:
# x = 10
# y = 20
# z = x + y
# print(z)

# Assembled opcodes:
0: LOADI_I   us1=0, i1=10        # Load 10 into register A
1: STOREI_G  us1=0, us2=0        # Store A to global[0] (x)
2: LOADI_I   us1=1, i1=20        # Load 20 into register B
3: STOREI_G  us1=1, us2=1        # Store B to global[1] (y)
4: LOADI_G   us1=0, us2=0        # Load global[0] into register A
5: LOADI_G   us1=1, us2=1        # Load global[1] into register B
6: ADDI_R    us1=2, us2=0, i1=1  # C = A + B
7: STOREI_G  us1=2, us2=2        # Store C to global[2] (z)
8: PRINTI    us1=2               # Print register C
9: HALT                          # Stop
```

**Example with Function Call:**

```
# High-level: result = add(5, 3)
# where add(a, b) { return a + b; }

# Main program:
0: PUSHV_I   us1=0, i1=5         # Push param 1 (value 5) to next frame's vars[0]
1: PUSHV_I   us1=1, i1=3         # Push param 2 (value 3) to next frame's vars[1]
2: CALL      ui1=10              # Call function at address 10
3: POPV_I    us1=0, us2=0        # Pop return value from vars[0] to register A
4: PRINTI    us1=0               # Print result
5: HALT

# Function 'add' at address 10:
10: POPV_I   us1=0, us2=0        # Pop param 1 to register A
11: POPV_I   us1=1, us2=1        # Pop param 2 to register B
12: ADDI_R   us1=0, us2=0, i1=1  # A = A + B
13: PUSHV_I  us1=0, i1=<A>       # Push return value (from A) to vars[0]
14: RET                          # Return to caller
```

### 9. Future Extensions

Potential enhancements while maintaining MISRA-C compliance:

1. **Extended String Operations**: Regular expression matching, formatting functions
2. **Array Operations**: Support for array indexing and iteration within MISRA-C constraints
3. **Debugging Support**: Breakpoint and trace opcodes for development
4. **Optimization**: Peephole optimization of opcode sequences, dead code elimination
5. **Error Recovery**: Structured exception handling with try/catch opcodes
6. **File I/O**: File operations for persistent storage with error handling
7. **Interoperability**: Controlled foreign function interface for safe C library calls
8. **Additional Registers**: More general-purpose registers if needed for complex operations

### 10. Conclusion

The Stipple VM provides a robust, MISRA-C compliant execution environment for a shell-like scripting language. The register-based architecture with pre-allocated stack frames ensures predictable behavior and compliance with safety-critical coding standards. Key features include:

- **MISRA-C Compliance**: Strict separation of signed/unsigned types, bitwise operations only on unsigned, no dynamic allocation
- **Register-based Design**: Efficient execution with 9 general-purpose registers (4 signed int, 3 unsigned int, 2 float)
- **Pre-allocated Frames**: Maximum stack depth of 16 with complete type safety
- **Comprehensive Opcode Set**: 100+ opcodes supporting integer (signed/unsigned), floating-point, and string operations
- **Type Safety**: Explicit type tracking in stack variables prevents type confusion
- **Deterministic Behavior**: Fixed resource allocation and explicit error handling

The architecture supports both value passing and reference passing through the typed stack variable system, enabling efficient parameter and return value handling while maintaining MISRA-C compliance.
