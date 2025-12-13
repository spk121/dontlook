# Stipple - MISRA-C Compliant Language Interpreter

## Software Design Document

### 1. Overview

Stipple is a MISRA-C compliant interpreted language with syntax roughly similar to POSIX shell. The interpreter consists of a parser/lexer frontend that converts source code into opcodes for a stack-based virtual machine (VM). This document describes the VM architecture and design.

### 2. Design Constraints

The VM is designed to comply with MISRA-C guidelines, which impose the following constraints:

- **No dynamic memory allocation**: All data structures use fixed-size arrays
- **Limited use of unions**: Unions are used sparingly and only with type tags for variant types
- **Type safety**: Explicit type conversions, bounds checking, and type tags for all variant types
- **Deterministic behavior**: No undefined or implementation-defined behavior
- **Portable code**: Avoid bitfields and platform-specific assumptions

### 3. VM Architecture

#### 3.1 Core Components

The Stipple VM is a stack-based virtual machine encapsulated in a single `vm_state_t` structure containing:

1. **Instruction Memory**: Fixed 64KB array for program bytecode
2. **Global Variables**: 256 typed global variables
3. **Global Memory Buffers**: 256 typed memory buffers for arrays/strings
4. **Stack Frames**: Pre-allocated stack frames (maximum depth 32) for function calls
5. **Program Counter (PC)**: Points to the current instruction
6. **Stack Pointer (SP)**: Points to the current stack frame (0-31)
6. **Condition Flags**: Comparison result flags (Zero, Less, Greater)
7. **Error State**: Last error code for diagnostics

All VM state is contained in a single structure, enabling multiple VM instances and simplifying debugging.

#### 3.2 Data Structures

##### 3.2.1 Global Variables

Global variables are stored in a fixed array with explicit type tracking:

```c
#define G_VARS_COUNT 256

typedef enum {
    V_VOID = 0,         /* Variable is unused */
    V_I32,              /* Signed 32-bit integer */
    V_U32,              /* Unsigned 32-bit integer */
    V_FLOAT,            /* IEEE 754 single-precision float */
    V_U8,               /* 4 unsigned 8-bit characters */
    V_U16,              /* 2 unsigned 16-bit integers */
    V_UC,               /* Unicode codepoint (as signed 32-bit) */
    V_GLOBAL_VAR_IDX,   /* Reference to global variable */
    V_STACK_VAR_IDX,    /* Reference to stack variable */
    V_BUF_IDX,          /* Reference to memory buffer */
    V_BUF_POS           /* Position within memory buffer */
} var_value_type_t;

typedef struct {
    var_value_type_t type;
    union {
        int32_t i32;                    /* V_I32 */
        uint32_t u32;                   /* V_U32, V_UC, V_GLOBAL_VAR_IDX, etc. */
        float f32;                      /* V_FLOAT */
        uint8_t u8x4[4];                /* V_U8 */
        uint16_t u16x2[2];              /* V_U16 */
        stack_var_ref_t stack_var_ref;  /* V_STACK_VAR_IDX */
    } val;
} var_value_t;
```

**Type Usage:**
- **V_VOID**: Unused variable slot
- **V_I32**: Single signed 32-bit integer (most common integer type)
- **V_U32**: Single unsigned 32-bit integer
- **V_FLOAT**: IEEE 754 single-precision floating-point value
- **V_U8**: Four 8-bit unsigned characters packed in one variable (for small strings or byte sequences)
- **V_U16**: Two 16-bit unsigned integers packed in one variable
- **V_UC**: Unicode codepoint (stored as signed 32-bit integer per Unicode standard)
- **V_GLOBAL_VAR_IDX**: Index reference to another global variable (for indirect access)
- **V_STACK_VAR_IDX**: Reference to a stack variable (contains frame_idx and var_idx)
- **V_BUF_IDX**: Index reference to a memory buffer
- **V_BUF_POS**: Position/offset within a memory buffer (for buffer indexing)

The `var_value_t` structure is 8 bytes (4 bytes for type enum + 4 bytes for union value).

##### 3.2.2 Global Memory Buffers

Memory buffers provide array storage with explicit type tracking:

```c
#define G_MEMBUF_LEN 256
#define G_MEMBUF_COUNT 256

typedef enum {
    MB_VOID = 0,    /* Buffer is unused */
    MB_U8,          /* Array of 256 unsigned 8-bit values */
    MB_U16,         /* Array of 128 unsigned 16-bit values */
    MB_I32,         /* Array of 64 signed 32-bit values */
    MB_U32,         /* Array of 64 unsigned 32-bit values */
    MB_FLOAT        /* Array of 64 float values */
} membuf_type_t;

typedef struct {
    membuf_type_t type;
    union {
        uint8_t u8x256[G_MEMBUF_LEN];
        uint16_t u16x128[G_MEMBUF_LEN/2];
        int32_t i32x64[G_MEMBUF_LEN/4];
        uint32_t u32x64[G_MEMBUF_LEN/4];
        float f32x64[G_MEMBUF_LEN/4];
    } buf;
} membuf_t;
```

**Buffer Type Usage:**
- **MB_VOID**: Unused buffer slot
- **MB_U8**: 256 bytes (used for strings, byte arrays, or raw data)
- **MB_U16**: 128 16-bit unsigned integers
- **MB_I32**: 64 32-bit signed integers
- **MB_U32**: 64 32-bit unsigned integers
- **MB_FLOAT**: 64 single-precision floats

All buffers are 256 bytes in size, but the element count varies by type. Strings are stored in MB_U8 buffers with null termination.

##### 3.2.3 Instruction Format

Instructions have a variable-length format with a portable header and optional payload. The header avoids bitfields for portability across compilers and platforms.

```c
typedef enum {
    IMM_TYPE_NONE = 0,
    IMM_TYPE_UCHAR = 1,
    IMM_TYPE_USHORT = 2,
    IMM_TYPE_UINT = 3,
    IMM_TYPE_INT = 4,
    IMM_TYPE_FLOAT = 5,
    IMM_TYPE_STACK_VAR_REF = 6,
    IMM_TYPE_GLOBAL_REF = 7,
    IMM_TYPE_MEMBUF_REF = 8,
    IMM_TYPE_MEMBUF_POS = 9
} imm_type_t;

typedef struct {
    uint8_t opcode;   /* Operation code */
    uint8_t operand;  /* Operand: specializes the opcode */
    uint8_t flags;    /* Bits 0-3: payload_len, Bits 4-7: imm_type1 */
    uint8_t types;    /* Bits 0-3: imm_type2, Bits 4-7: imm_type3 */
} instruction_header_t;

/* Accessor macros for portability */
#define INSTR_PAYLOAD_LEN(hdr)  ((hdr).flags & 0x0Fu)
#define INSTR_IMM_TYPE1(hdr)    (((hdr).flags >> 4) & 0x0Fu)
#define INSTR_IMM_TYPE2(hdr)    ((hdr).types & 0x0Fu)
#define INSTR_IMM_TYPE3(hdr)    (((hdr).types >> 4) & 0x0Fu)

typedef union instruction_payload {
    uint8_t u8x4[4];
    uint16_t u16x2[2];
    uint32_t u32;
    int32_t i32;
    float f32;
    stack_var_ref_t stack_var_ref;
    index_t global_var_idx;
    index_t membuf_idx;
    pos_t membuf_pos;
} instruction_payload_t;
```

**Instruction Sizes:**
- **Tiny (4 bytes)**: Header only, no payload (payload_len = 0)
- **Small (8 bytes)**: Header + 1 payload word (payload_len = 1)
- **Medium (12 bytes)**: Header + 2 payload words (payload_len = 2)
- **Large (16 bytes)**: Header + 3 payload words (payload_len = 3)

```c
typedef struct tiny_instruction {
    instruction_header_t header;
} tiny_instruction_t;

typedef struct small_instruction {
    instruction_header_t header;
    instruction_payload_t imm1;
} small_instruction_t;

typedef struct medium_instruction {
    instruction_header_t header;
    instruction_payload_t imm1;
    instruction_payload_t imm2;
} medium_instruction_t;

typedef struct large_instruction {
    instruction_header_t header;
    instruction_payload_t imm1;
    instruction_payload_t imm2;
    instruction_payload_t imm3;
} large_instruction_t;
```

**Immediate Type Interpretation:**
- The `imm_typeN` fields in the header indicate how to interpret the corresponding `immN` payload
- `IMM_TYPE_NONE`: No immediate value in this slot
- `IMM_TYPE_UCHAR`, `IMM_TYPE_USHORT`, `IMM_TYPE_UINT`, `IMM_TYPE_INT`, `IMM_TYPE_FLOAT`: Literal values
- `IMM_TYPE_STACK_VAR_REF`: Reference to a stack variable via stack_var_ref_t structure
- `IMM_TYPE_GLOBAL_REF`: Index to a global variable
- `IMM_TYPE_MEMBUF_REF`: Index to a memory buffer
- `IMM_TYPE_MEMBUF_POS`: Position/offset within a memory buffer

**Portability Note**: The header uses byte fields with bit-manipulation macros instead of bitfields to ensure consistent layout across compilers and platforms.

**Stack Variable Reference Encoding:**

When `IMM_TYPE_STACK_VAR_REF` is used, the immediate value is interpreted as a `stack_var_ref_t` structure:

```c
typedef struct {
    uint16_t frame_idx; /* Stack frame index (0-31) */
    uint16_t var_idx;   /* Variable index inside the stack frame */
} stack_var_ref_t;
```

This allows instructions to reference:
- **Stack variables in other frames**: `{frame_idx=1, var_idx=0}` refers to `stack_frames[1].stack_vars[0]`
- **Local variables**: When var_idx refers to the locals array
- **Return value**: Special handling required as it's not part of indexed arrays

##### 3.2.4 Stack Structure

The VM uses pre-allocated stack frames with a maximum depth of 32. Each frame contains stack variables, local variables, and a return value slot.

```c
#define STACK_DEPTH 32           /* Maximum 32 nested function calls */
#define STACK_VAR_COUNT 16       /* Each frame has 16 slots for parameters/temp values */
#define STACK_LOCALS_COUNT 64    /* Maximum 64 local variables per frame */

typedef struct {
    var_value_t stack_vars[STACK_VAR_COUNT];  /* Variables for parameter passing */
    var_value_t locals[STACK_LOCALS_COUNT];   /* Local variables */
    var_value_t ret_val;                      /* Return value */
    uint32_t return_addr;                     /* Return address (PC) */
} stack_frame_t;
```

**Stack Frame Components:**

- **stack_vars[]**: Used for passing parameters to functions and temporary values
  - When calling a function, the caller writes parameter values into the callee's stack_vars[]
  - Each stack_var is a full var_value_t with type tag, supporting all data types
  - Maximum 16 stack variables per frame
  - These variables are preserved across CALL operations

- **locals[]**: Frame-local variable storage
  - Used for function local variables that persist throughout function execution
  - Each local is a full var_value_t with type tag
  - Maximum 64 local variables per frame
  - Locals are zeroed when entering a function

- **ret_val**: Return value from the function
  - Single var_value_t to hold the function's return value
  - The callee sets this before returning
  - The caller reads this after the function returns
  - Supports all data types through the type tag

- **return_addr**: Saved program counter
  - Stores the PC value to return to after RET instruction
  - Set automatically by CALL instruction

##### 3.2.5 VM State Structure

The complete VM state is encapsulated in a single structure:

```c
#define FLAG_ZERO    0x01u  /* Zero flag (Z) - values are equal */
#define FLAG_LESS    0x02u  /* Less flag (L) - first < second */
#define FLAG_GREATER 0x04u  /* Greater flag (G) - first > second */

typedef struct {
    /* Global storage */
    var_value_t g_vars[G_VARS_COUNT];
    membuf_t g_membuf[G_MEMBUF_COUNT];

    /* Stack */
    stack_frame_t stack_frames[STACK_DEPTH];
    uint8_t sp;  /* Stack pointer (current frame index, 0-31) */

    /* Program execution */
    uint8_t program[PROGRAM_MAX_SIZE];  /* Instruction memory */
    uint32_t program_len;               /* Length of loaded program */
    uint32_t pc;                        /* Program counter */

    /* Condition flags */
    uint8_t flags;  /* Comparison flags (Z, L, G) */

    /* Error state */
    vm_status_t last_error;
} vm_state_t;
```

This encapsulation provides:
- Support for multiple VM instances
- Clear ownership and lifetime semantics
- Simplified debugging (single pointer to inspect)
- Better cache locality

### 4. Error Handling

The VM uses explicit error codes for all operations:

```c
typedef enum {
    VM_OK = 0,                    /* Operation successful */
    VM_ERR_STACK_OVERFLOW,        /* Stack depth exceeded */
    VM_ERR_STACK_UNDERFLOW,       /* Return from bottom frame */
    VM_ERR_DIV_BY_ZERO,           /* Division or modulo by zero */
    VM_ERR_INVALID_OPCODE,        /* Unrecognized opcode */
    VM_ERR_TYPE_MISMATCH,         /* Operation type doesn't match variable type */
    VM_ERR_BOUNDS,                /* Array/buffer access out of bounds */
    VM_ERR_INVALID_GLOBAL_IDX,    /* Global variable index out of range */
    VM_ERR_INVALID_LOCAL_IDX,     /* Local variable index out of range */
    VM_ERR_INVALID_STACK_VAR_IDX, /* Stack variable index out of range */
    VM_ERR_INVALID_BUFFER_IDX,    /* Buffer index out of range */
    VM_ERR_INVALID_BUFFER_POS,    /* Position out of bounds for buffer type */
    VM_ERR_INVALID_PC,            /* Program counter out of bounds */
    VM_ERR_INVALID_INSTRUCTION,   /* Malformed instruction */
    VM_ERR_PROGRAM_TOO_LARGE,     /* Program exceeds maximum size */
    VM_ERR_HALT                   /* HALT instruction executed (not an error) */
} vm_status_t;
```

All VM operations return `vm_status_t` to indicate success or failure. The VM also stores the last error in `vm_state_t.last_error` for debugging.

### 5. Instruction Set

The opcodes are organized into categories based on operation type. Instructions use the most appropriate size (tiny, small, medium, or large) based on their operand requirements.

#### 5.1 Opcode Naming Convention

- **Prefix indicates operation type**: LOAD, STORE, ADD, SUB, MUL, DIV, CMP, JMP, etc.
- **Suffix `_G`**: Operates on global variable
- **Suffix `_L`**: Operates on local variable
- **Suffix `_S`**: Operates on stack variable
- **Suffix `_B`**: Operates on memory buffer
- **Suffix `_I`**: Uses immediate value
- **Type suffix**: `_I32`, `_U32`, `_F32` indicates the data type

#### 5.2 Control Flow Operations

All control flow operations are **tiny instructions** (4 bytes) except CALL and JMP which are **small instructions** (8 bytes) due to the address operand.

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x00 | NOP | Tiny | No operation | - |
| 0x01 | HALT | Tiny | Stop execution | - |
| 0x02 | JMP | Small | Unconditional jump | imm1 = target PC (uint) |
| 0x03 | JZ | Small | Jump if zero | imm1 = target PC (uint) |
| 0x04 | JNZ | Small | Jump if not zero | imm1 = target PC (uint) |
| 0x05 | JLT | Small | Jump if less than | imm1 = target PC (uint) |
| 0x06 | JGT | Small | Jump if greater than | imm1 = target PC (uint) |
| 0x07 | JLE | Small | Jump if less or equal | imm1 = target PC (uint) |
| 0x08 | JGE | Small | Jump if greater or equal | imm1 = target PC (uint) |
| 0x09 | CALL | Small | Call subroutine | imm1 = target PC (uint) |
| 0x0A | RET | Tiny | Return from subroutine | - |

#### 5.3 Variable Load/Store Operations

##### 5.3.1 Load Operations

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x10 | LOAD_G | Small | Load global variable to stack var | operand = stack var slot, imm1 = global idx |
| 0x11 | LOAD_L | Small | Load local variable to stack var | operand = stack var slot, imm1 = local idx |
| 0x12 | LOAD_S | Small | Load from another frame's stack var | operand = dest slot, imm1 = stack_var_ref |
| 0x13 | LOAD_I_I32 | Small | Load immediate int32 to stack var | operand = stack var slot, imm1 = value (int) |
| 0x14 | LOAD_I_U32 | Small | Load immediate uint32 to stack var | operand = stack var slot, imm1 = value (uint) |
| 0x15 | LOAD_I_F32 | Small | Load immediate float to stack var | operand = stack var slot, imm1 = value (float) |
| 0x16 | LOAD_RET | Small | Load return value from frame | operand = dest stack var slot, imm1 = frame idx |

##### 5.3.2 Store Operations

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x20 | STORE_G | Small | Store stack var to global variable | operand = stack var slot, imm1 = global idx |
| 0x21 | STORE_L | Small | Store stack var to local variable | operand = stack var slot, imm1 = local idx |
| 0x22 | STORE_S | Small | Store to another frame's stack var | operand = src slot, imm1 = stack_var_ref |
| 0x23 | STORE_RET | Small | Store to return value of frame | operand = src stack var slot, imm1 = frame idx |

#### 5.4 Arithmetic Operations

All arithmetic operations work on stack variables and store results in stack variables. They are **medium instructions** (12 bytes) with two source operands, except unary operations which are **small instructions** (8 bytes).

##### 5.4.1 Integer Arithmetic

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x30 | ADD_I32 | Medium | Add signed integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x31 | SUB_I32 | Medium | Subtract signed integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x32 | MUL_I32 | Medium | Multiply signed integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x33 | DIV_I32 | Medium | Divide signed integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x34 | MOD_I32 | Medium | Modulo signed integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x35 | NEG_I32 | Small | Negate signed integer | operand = dest slot, imm1 = src slot |
| 0x36 | ADD_U32 | Medium | Add unsigned integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x37 | SUB_U32 | Medium | Subtract unsigned integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x38 | MUL_U32 | Medium | Multiply unsigned integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x39 | DIV_U32 | Medium | Divide unsigned integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x3A | MOD_U32 | Medium | Modulo unsigned integers | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |

##### 5.4.2 Float Arithmetic

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x40 | ADD_F32 | Medium | Add floats | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x41 | SUB_F32 | Medium | Subtract floats | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x42 | MUL_F32 | Medium | Multiply floats | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x43 | DIV_F32 | Medium | Divide floats | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x44 | NEG_F32 | Small | Negate float | operand = dest slot, imm1 = src slot |
| 0x45 | ABS_F32 | Small | Absolute value of float | operand = dest slot, imm1 = src slot |
| 0x46 | SQRT_F32 | Small | Square root of float | operand = dest slot, imm1 = src slot |

#### 5.5 Bitwise Operations (Unsigned Only - MISRA-C Compliant)

Per MISRA-C Rule 10.1, bitwise operations are only performed on unsigned types. All are **medium instructions** (12 bytes) except NOT which is **small** (8 bytes).

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x50 | AND_U32 | Medium | Bitwise AND | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x51 | OR_U32 | Medium | Bitwise OR | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x52 | XOR_U32 | Medium | Bitwise XOR | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x53 | NOT_U32 | Small | Bitwise NOT | operand = dest slot, imm1 = src slot |
| 0x54 | SHL_U32 | Medium | Shift left | operand = dest slot, imm1 = value slot, imm2 = shift count slot |
| 0x55 | SHR_U32 | Medium | Logical shift right | operand = dest slot, imm1 = value slot, imm2 = shift count slot |

#### 5.6 Comparison Operations

Comparison operations set condition flags and are **medium instructions** (12 bytes).

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x60 | CMP_I32 | Medium | Compare signed integers | operand unused, imm1 = src1 slot, imm2 = src2 slot |
| 0x61 | CMP_U32 | Medium | Compare unsigned integers | operand unused, imm1 = src1 slot, imm2 = src2 slot |
| 0x62 | CMP_F32 | Medium | Compare floats | operand unused, imm1 = src1 slot, imm2 = src2 slot |

#### 5.7 Type Conversion Operations

Type conversion operations are **small instructions** (8 bytes).

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x70 | I32_TO_U32 | Small | Convert signed to unsigned int | operand = dest slot, imm1 = src slot |
| 0x71 | U32_TO_I32 | Small | Convert unsigned to signed int | operand = dest slot, imm1 = src slot |
| 0x72 | I32_TO_F32 | Small | Convert signed int to float | operand = dest slot, imm1 = src slot |
| 0x73 | U32_TO_F32 | Small | Convert unsigned int to float | operand = dest slot, imm1 = src slot |
| 0x74 | F32_TO_I32 | Small | Convert float to signed int (truncate) | operand = dest slot, imm1 = src slot |
| 0x75 | F32_TO_U32 | Small | Convert float to unsigned int (truncate) | operand = dest slot, imm1 = src slot |

#### 5.8 Memory Buffer Operations

Operations on memory buffers for array and string manipulation.

##### 5.8.1 Buffer Access

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x80 | BUF_READ | Medium | Read from buffer to stack var | operand = dest slot, imm1 = buf idx, imm2 = position |
| 0x81 | BUF_WRITE | Medium | Write stack var to buffer | operand = src slot, imm1 = buf idx, imm2 = position |
| 0x82 | BUF_LEN | Small | Get buffer element count | operand = dest slot, imm1 = buf idx |
| 0x83 | BUF_CLEAR | Small | Clear buffer | operand unused, imm1 = buf idx |

##### 5.8.2 String Operations

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x90 | STR_CAT | Medium | Concatenate strings | operand = dest buf, imm1 = src1 buf, imm2 = src2 buf |
| 0x91 | STR_COPY | Medium | Copy string | operand = dest buf, imm1 = src buf, imm2 unused |
| 0x92 | STR_LEN | Small | Get string length | operand = dest slot, imm1 = buf idx |
| 0x93 | STR_CMP | Medium | Compare strings | operand unused, imm1 = buf1 idx, imm2 = buf2 idx |
| 0x94 | STR_CHR | Medium | Get character at index | operand = dest slot, imm1 = buf idx, imm2 = position |
| 0x95 | STR_SET_CHR | Large | Set character at index | operand unused, imm1 = buf idx, imm2 = position, imm3 = char value |

#### 5.9 I/O Operations

I/O operations are **small instructions** (8 bytes) except PRINTLN which is **tiny** (4 bytes).

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0xA0 | PRINT_I32 | Small | Print signed integer | operand unused, imm1 = stack var slot |
| 0xA1 | PRINT_U32 | Small | Print unsigned integer | operand unused, imm1 = stack var slot |
| 0xA2 | PRINT_F32 | Small | Print float | operand unused, imm1 = stack var slot |
| 0xA3 | PRINT_STR | Small | Print string from buffer | operand unused, imm1 = buf idx |
| 0xA4 | PRINTLN | Tiny | Print newline | - |
| 0xA5 | READ_I32 | Small | Read signed integer | operand = dest stack var slot, imm1 unused |
| 0xA6 | READ_U32 | Small | Read unsigned integer | operand = dest stack var slot, imm1 unused |
| 0xA7 | READ_F32 | Small | Read float | operand = dest stack var slot, imm1 unused |
| 0xA8 | READ_STR | Small | Read string to buffer | operand unused, imm1 = dest buf idx |

### 6. VM Execution Model

#### 6.1 Initialization

The `vm_init()` function initializes a VM instance:

1. All global variables (g_vars[]) are set to V_VOID type with zero values
2. All memory buffers (g_membuf[]) are set to MB_VOID type with cleared contents
3. Stack pointer (SP) is set to 0 (frame level 0)
4. Program counter (PC) is set to 0
5. Condition flags are cleared
6. All stack frames are zeroed
7. Error state is set to VM_OK
8. Program memory is cleared

#### 6.2 Program Loading

The `vm_load_program()` function loads bytecode into instruction memory:

1. Validates program length (must not exceed PROGRAM_MAX_SIZE)
2. Copies bytecode into vm.program[]
3. Sets vm.program_len
4. Resets PC to 0
5. Returns VM_OK on success or VM_ERR_PROGRAM_TOO_LARGE on failure

#### 6.3 Instruction Fetch-Decode-Execute Cycle

The `vm_step()` function executes one instruction:

```
1. Validate PC is within bounds
2. Fetch: Read instruction header at PC
3. Decode: Extract opcode, operand, payload_len, and immediate types
4. Validate: Check opcode validity
5. Fetch payload: Read payload_len 4-byte words
6. Execute: Perform operation based on opcode
7. Increment PC by instruction size (4 + payload_len * 4 bytes)
8. Return status code (VM_OK or error)
```

The `vm_run()` function executes until HALT or error:

```
while (status == VM_OK) {
    status = vm_step(vm);
    if (status == VM_ERR_HALT) {
        return VM_OK;  /* Normal termination */
    }
}
return status;  /* Error occurred */
```

The variable-length instruction format allows the VM to be space-efficient while maintaining 4-byte alignment.

#### 6.4 Stack Variable Management

Stack variables in the current frame are the primary working storage for the VM. Unlike register-based VMs, there are no dedicated registers - all operations work directly on typed stack variables.

**Accessing Stack Variables:**
- Stack variables are accessed via the `operand` byte in the instruction header (0-15 for stack_vars[])
- Each stack variable is a full var_value_t with a type tag
- The VM validates type compatibility before operations
- Stack variables can hold values or references

**Stack Variable Types:**
- Direct values: V_I32, V_U32, V_FLOAT, V_U8, V_U16, V_UC
- References: V_GLOBAL_VAR_IDX, V_STACK_VAR_IDX, V_BUF_IDX, V_BUF_POS

**Example:**
```c
// Load immediate value 42 into stack_vars[0]
LOAD_I_I32 operand=0, imm1=42
// Now stack_frames[SP].stack_vars[0].type == V_I32
// and stack_frames[SP].stack_vars[0].val.i32 == 42

// Add two stack variables
ADD_I32 operand=2, imm1=0, imm2=1
// Result stored in stack_vars[2]
```

#### 6.5 Local Variable Management

Local variables provide additional storage within a frame for variables that need to persist across operations but aren't passed as parameters.

**Local Variables:**
- 64 local variables per frame (locals[0] through locals[63])
- Each is a full var_value_t with type tag
- Zeroed when entering a function
- Accessed via LOAD_L and STORE_L instructions

**Example:**
```c
// Store stack_vars[0] to locals[5]
STORE_L operand=0, imm1=5

// Later, load locals[5] back to stack_vars[1]
LOAD_L operand=1, imm1=5
```

#### 6.6 Global Variable Management

Global variables provide shared storage across all function calls.

**Global Variables:**
- 256 global variables (g_vars[0] through g_vars[255])
- Each is a full var_value_t with type tag
- Persistent across function calls
- Accessed via LOAD_G and STORE_G instructions

**Example:**
```c
// Store stack_vars[0] to global variable 100
STORE_G operand=0, imm1=100

// Load global variable 100 to stack_vars[1]
LOAD_G operand=1, imm1=100
```

#### 6.7 Function Calls

Function calls use stack frames to maintain separate execution contexts. The mechanism relies on stack variables for parameter passing and return values.

**CALL Operation:**
1. Validate stack depth: `if (SP >= STACK_DEPTH - 1) return VM_ERR_STACK_OVERFLOW`
2. Save return address: `stack_frames[SP + 1].return_addr = PC + 8` (next instruction)
3. Increment SP: `SP++`
4. Zero the new frame's locals (stack_vars are preserved from parameter setup):
   ```c
   for (i = 0; i < STACK_LOCALS_COUNT; i++) {
       stack_frames[SP].locals[i].type = V_VOID;
       stack_frames[SP].locals[i].val.u32 = 0;
   }
   ```
5. Set PC to target address: `PC = imm1.u32`
6. Return VM_OK

**Parameter Passing:**
Before calling a function, the caller sets up parameters in the next frame's stack_vars[]:
```c
// Caller (at frame level 0):
LOAD_I_I32 operand=0, imm1=10                // Load 10 into stack_vars[0]
STORE_S operand=0, imm1={frame=1, var=0}     // Store to next frame's stack_vars[0]
LOAD_I_I32 operand=1, imm1=20                // Load 20 into stack_vars[1]
STORE_S operand=1, imm1={frame=1, var=1}     // Store to next frame's stack_vars[1]
CALL imm1=<function_addr>                    // Call function

// Callee (now at frame level 1):
// Parameters are already in stack_vars[0] and stack_vars[1]
ADD_I32 operand=2, imm1=0, imm2=1            // Add params, result in stack_vars[2]
```

**RET Operation:**
1. Validate stack depth: `if (SP == 0) return VM_ERR_STACK_UNDERFLOW`
2. Restore PC from return address: `PC = stack_frames[SP].return_addr`
3. Decrement SP: `SP--`
4. Return VM_OK

**Return Values:**
Before returning, the callee stores the return value in ret_val:
```c
// Callee stores return value
STORE_RET operand=2, imm1=SP                 // Store stack_vars[2] to current frame's ret_val
RET

// Caller retrieves return value (after RET, SP has decremented)
LOAD_RET operand=0, imm1=(SP+1)              // Load return value from frame SP+1 to stack_vars[0]
```

**Complete Function Call Example:**
```c
# High-level: result = add(5, 3)
# where add(a, b) { return a + b; }

# Caller (at frame level 0):
LOAD_I_I32 operand=0, imm1=5                 # Load param 1
STORE_S operand=0, imm1={frame=1, var=0}     # Store to next frame's stack_vars[0]
LOAD_I_I32 operand=1, imm1=3                 # Load param 2
STORE_S operand=1, imm1={frame=1, var=1}     # Store to next frame's stack_vars[1]
CALL imm1=<add_addr>                         # Call add(), SP becomes 1
LOAD_RET operand=0, imm1=1                   # Load return value from frame 1
PRINT_I32 imm1=0                             # Print result
HALT

# Function 'add' (at frame level 1):
<add_addr>:
ADD_I32 operand=2, imm1=0, imm2=1            # stack_vars[2] = stack_vars[0] + stack_vars[1]
STORE_RET operand=2, imm1=SP                 # Store result to ret_val (imm1=SP means current frame)
RET                                          # Return, SP becomes 0
```

#### 6.8 Memory Buffer Operations

Memory buffers provide array and string storage. Each buffer has a type that determines how it's interpreted.

**Buffer Indexing:**
- Buffers are 256 bytes total
- Element size depends on type: U8 (256 elements), U16 (128 elements), I32/U32/FLOAT (64 elements)
- Position is always in element units, not bytes
- Bounds checking is performed based on buffer type

**String Storage:**
- Strings are stored in MB_U8 buffers
- Null-terminated C-style strings
- Maximum length is 255 characters (256th byte for null terminator)

**Example:**
```c
// Write value to integer array
LOAD_I_I32 operand=0, imm1=42          # Load value
LOAD_I_U32 operand=1, imm1=5           # Load index
BUF_WRITE operand=0, imm1=10, imm2=1   # Write to buffer 10 at position from stack_vars[1]

// Read from array
LOAD_I_U32 operand=1, imm1=5           # Load index
BUF_READ operand=0, imm1=10, imm2=1    # Read from buffer 10 at position 5
```

#### 6.9 Condition Flags

The VM maintains condition flags set by comparison operations:
- **FLAG_ZERO (0x01)**: Values are equal
- **FLAG_LESS (0x02)**: First operand is less than second
- **FLAG_GREATER (0x04)**: First operand is greater than second

Flags are set by CMP_* operations and tested by conditional jump instructions (JZ, JNZ, JLT, JGT, JLE, JGE).

#### 6.10 Error Handling

The VM validates all operations and returns appropriate error codes. All validation is performed before modifying VM state to maintain consistency.

**Error Detection:**
1. **Stack Overflow**: SP >= STACK_DEPTH - 1 on CALL
2. **Stack Underflow**: SP == 0 on RET
3. **Division by Zero**: Integer or float division/modulo by zero
4. **Invalid Indices**: Global, local, stack var, or buffer index out of range
5. **Buffer Bounds**: Position exceeds buffer capacity for its type
6. **Type Mismatch**: Operation type doesn't match variable type
7. **Invalid Opcode**: Opcode >= OP_MAX
8. **Invalid PC**: PC out of program bounds
9. **Malformed Instruction**: Invalid payload_len or immediate types

When an error occurs:
1. The operation is aborted without modifying state
2. The error code is returned from the operation
3. The error is stored in vm.last_error
4. Execution halts (vm_run returns the error code)

### 7. MISRA-C Compliance Details

#### 7.1 Memory Management

- **Static Allocation**: All arrays are fixed-size and statically allocated
- **Pre-allocated Frames**: Stack frames are pre-allocated (no dynamic allocation)
- **Bounds Checking**: All array accesses are validated against array bounds
- **No Pointer Arithmetic**: Array indexing uses subscript notation only
- **Fixed Limits**: Maximum stack depth (32), global variables (256), buffers (256)

#### 7.2 Type Safety

- **Tagged Union Types**: var_value_t and membuf_t use type tags to prevent misuse
- **Explicit Type Conversions**: Type conversions use dedicated opcodes
- **Fixed-Width Types**: Uses int32_t, uint32_t, uint16_t, uint8_t for integer types
- **Float Requirement**: Implementation requires IEEE 754 single-precision (32-bit) floating point support
- **Enum for Opcodes and Types**: Opcodes and type tags defined as enumeration constants
- **Runtime Type Checking**: VM validates type tags before operations

#### 7.3 Unions and Variant Types

While MISRA-C advises caution with unions, Stipple uses them in a controlled manner:

- **Tagged Unions Only**: All unions (var_value_t, membuf_t, instruction_payload_t) have associated type tags
- **Type Tag Validation**: Type tag is always checked before accessing union members
- **No Type Punning**: Union members are only accessed according to their type tag
- **Deterministic**: The active union member is always known from the type tag

This usage follows MISRA-C guidelines when proper discipline is maintained.

#### 7.4 Bitwise Operations (MISRA-C Compliant)

Per MISRA-C Rule 10.1: "Shift and bitwise operations shall only be performed on operands of essentially unsigned type"

- **Unsigned-Only Bitwise Ops**: All bitwise operations (AND, OR, XOR, NOT, SHL, SHR) only operate on V_U32 typed values
- **Type Checking**: VM verifies operands are V_U32 type before bitwise operations
- **No Signed Bitwise**: V_I32 values cannot be used for bitwise operations

#### 7.5 Portability

- **No Bitfields**: Instruction header uses byte fields with macros instead of bitfields
- **Explicit Endianness**: All multi-byte values use platform-native endianness
- **Static Assertions**: Compile-time size checks ensure structure layout
- **Alignment**: All structures are designed for 4-byte alignment

#### 7.6 Deterministic Behavior

- **Defined Overflow**: Integer operations use well-defined wraparound semantics for unsigned, and implementation-defined for signed (typically two's complement)
- **No Undefined Behavior**: All edge cases explicitly handled
- **Predictable Execution**: No implementation-defined behavior in VM core beyond integer overflow
- **Pre-allocated Resources**: Fixed stack depth prevents unbounded recursion
- **Bounds Checking**: All array accesses validated

### 8. Performance Considerations

#### 8.1 Memory Footprint

- **Global Variables**: 256 × sizeof(var_value_t) ≈ 256 × 8 bytes = 2 KB
- **Global Memory Buffers**: 256 × 256 bytes = 64 KB
- **Stack Frames**: 32 × sizeof(stack_frame_t)
  - 16 stack_vars × 8 bytes = 128 bytes
  - 64 locals × 8 bytes = 512 bytes
  - 1 ret_val × 8 bytes = 8 bytes
  - return_addr = 4 bytes
  - Total per frame: ~652 bytes
  - Total for 32 frames: ~21 KB
- **Instruction Memory**: 64 KB
- **Total VM State**: Approximately 151 KB

#### 8.2 Execution Speed

- **Stack-based Design**: More memory accesses than register-based, but simpler decoder
- **Variable-length Instructions**: Compact encoding reduces instruction memory and cache pressure
- **Typed Operations**: Type checking overhead on each operation
- **Cache-friendly**: All data in fixed arrays with predictable access patterns
- **Predictable Timing**: No dynamic allocation or unbounded loops

#### 8.3 Instruction Encoding Efficiency

The variable-length instruction format provides good space efficiency:
- **Tiny (4 bytes)**: NOP, HALT, RET, PRINTLN - minimal operations (0 operands)
- **Small (8 bytes)**: Most load/store operations, unary operations (1 operand)
- **Medium (12 bytes)**: Binary operations - arithmetic, bitwise, comparison (2 operands)
- **Large (16 bytes)**: Ternary operations - rare, only STR_SET_CHR (3 operands)

Typical programs use mostly small and medium instructions, providing good code density.

### 9. API Usage Examples

#### 9.1 VM Initialization and Program Loading

```c
vm_state_t vm;
vm_init(&vm);

uint8_t program[] = {
    /* Bytecode here */
};

vm_status_t status = vm_load_program(&vm, program, sizeof(program));
if (status != VM_OK) {
    printf("Error loading program: %s\n", vm_get_error_string(status));
    return -1;
}
```

#### 9.2 Execution

```c
/* Execute entire program */
status = vm_run(&vm);
if (status != VM_OK) {
    printf("Runtime error: %s\n", vm_get_error_string(status));
    printf("PC: %u, SP: %u\n", vm.pc, vm.sp);
}

/* Or execute step-by-step for debugging */
while ((status = vm_step(&vm)) == VM_OK) {
    vm_dump_state(&vm);  /* Debug output */
}
```

#### 9.3 Validation

```c
/* Validate indices before use */
if (!validate_global_idx(idx)) {
    /* Handle error */
}

if (!validate_buffer_pos(buffer_type, position)) {
    /* Handle error */
}
```

### 10. Example Programs

#### 10.1 Simple Arithmetic

```
# High-level: x = 10 + 20
# Store result in global variable 0

LOAD_I_I32 operand=0, imm1=10          # stack_vars[0] = 10
LOAD_I_I32 operand=1, imm1=20          # stack_vars[1] = 20
ADD_I32 operand=2, imm1=0, imm2=1      # stack_vars[2] = stack_vars[0] + stack_vars[1]
STORE_G operand=2, imm1=0              # g_vars[0] = stack_vars[2]
PRINT_I32 imm1=2                       # Print stack_vars[2]
HALT
```

#### 10.2 Function Call with Parameters

```
# High-level: result = add(5, 3)
# where add(a, b) { return a + b; }

main:
    LOAD_I_I32 operand=0, imm1=5                 # Prepare param 1
    STORE_S operand=0, imm1={frame=1, var=0}     # Store to next frame's stack_vars[0]
    LOAD_I_I32 operand=1, imm1=3                 # Prepare param 2
    STORE_S operand=1, imm1={frame=1, var=1}     # Store to next frame's stack_vars[1]
    CALL imm1=add_func                           # Call add function, SP becomes 1
    LOAD_RET operand=0, imm1=1                   # Get return value from frame 1
    PRINT_I32 imm1=0                             # Print result
    HALT

add_func:
    # Parameters are in stack_vars[0] and stack_vars[1]
    ADD_I32 operand=2, imm1=0, imm2=1            # Add parameters
    STORE_RET operand=2, imm1=SP                 # Store return value to current frame's ret_val
    RET
```

#### 10.3 Array Operations

```
# High-level: arr[5] = 42; print(arr[5])
# Using buffer 0 as integer array

LOAD_I_I32 operand=0, imm1=42          # Value to store
LOAD_I_U32 operand=1, imm1=5           # Index
BUF_WRITE operand=0, imm1=0, imm2=1    # arr[5] = 42
BUF_READ operand=2, imm1=0, imm2=1     # Load arr[5]
PRINT_I32 imm1=2                       # Print value
HALT
```

#### 10.4 String Concatenation

```
# High-level: str3 = str1 + str2
# Buffers 0 and 1 contain strings, result in buffer 2

STR_CAT operand=2, imm1=0, imm2=1      # Concatenate buffers 0 and 1 into buffer 2
PRINT_STR imm1=2                       # Print result
HALT
```

### 11. Future Extensions

Potential enhancements while maintaining MISRA-C compliance:

1. **Extended Type System**: V_I64, V_U64 for 64-bit integers (if platform supports)
2. **More Memory Buffers**: Increase G_MEMBUF_COUNT for larger programs
3. **Debugging Support**: Breakpoint and trace opcodes for development
4. **Optimization**: Peephole optimization of instruction sequences
5. **Error Recovery**: Structured exception handling with try/catch mechanisms
6. **File I/O**: File operations for persistent storage
7. **Interoperability**: Controlled foreign function interface for safe C library calls
8. **Packed Arrays**: Support for bit-packed arrays to save memory

### 12. Conclusion

The Stipple VM provides a robust, MISRA-C compliant execution environment for a shell-like scripting language. Key features include:

- **MISRA-C Compliance**: Portable instruction format, tagged unions, no dynamic allocation, unsigned-only bitwise operations
- **Stack-based Design**: All operations work on typed stack variables
- **Variable-length Instructions**: Space-efficient encoding with four sizes (4, 8, 12, 16 bytes)
- **Encapsulated State**: Single vm_state_t structure for complete VM state
- **Typed Variables**: All variables (global, local, stack) have explicit type tags
- **Typed Buffers**: Memory buffers for arrays/strings with type tracking
- **Pre-allocated Frames**: Maximum stack depth of 32 with comprehensive bounds checking
- **Function Calls**: Stack variables for parameter passing, ret_val for return values
- **Error Handling**: Explicit status codes for all operations
- **Type Safety**: Runtime type checking prevents type confusion
- **Deterministic Behavior**: Fixed resource allocation and explicit error handling

The architecture supports efficient execution while maintaining strict MISRA-C compliance through careful design of the type system, instruction format, and memory management.
