# Stipple - MISRA-C Compliant Language Interpreter

## Software Design Document

### 1. Overview

Stipple is a MISRA-C compliant interpreted language with syntax roughly similar to POSIX shell. The interpreter consists of a parser/lexer frontend that converts source code into opcodes for a stack-based virtual machine (VM). This document focuses on the VM architecture and design.

### 2. Design Constraints

The VM is designed to comply with MISRA-C guidelines, which impose the following constraints:

- **No dynamic memory allocation**: All data structures use fixed-size arrays
- **Limited use of unions**: Unions are used sparingly and only where necessary for type-safe variant types (var_value_t, membuf_t, instruction_payload_t)
- **Type safety**: Explicit type conversions, bounds checking, and type tags for variant types
- **Deterministic behavior**: No undefined or implementation-defined behavior

### 3. VM Architecture

#### 3.1 Core Components

The Stipple VM is a stack-based virtual machine containing the following components:

1. **Instruction Memory**: Array of variable-sized instructions representing the program
2. **Global Variables**: 256 typed global variables (g_vars[])
3. **Global Memory Buffers**: 256 typed memory buffers for arrays/strings (g_membuf[])
4. **Stack Frames**: Pre-allocated stack frames (maximum depth 32) for function calls
5. **Program Counter (PC)**: Points to the current instruction
6. **Stack Pointer (SP)**: Points to the current stack frame

#### 3.2 Data Structures

##### 3.2.1 Global Variables

Global variables are stored in a fixed array with explicit type tracking:

```c
#define G_VARS_COUNT 256

typedef enum {
    V_VOID,             /* Variable is unused */
    V_U8,               /* 4 unsigned 8-bit characters */
    V_U16,              /* 2 unsigned 16-bit integers */
    V_I32,              /* Signed 32-bit integer value */
    V_U32,              /* Unsigned 32-bit integer values */
    V_UC,               /* A Unicode codepoint encoded as a 32-bit signed integer */
    V_FLOAT,            /* Float value */
    V_GLOBAL_VAR_IDX,   /* Reference to global variable, encoded as u32 */
    V_STACK_VAR_IDX,    /* Reference to a stack variable, encoded as stack_var_ref_t */
    V_BUF_IDX,          /* Reference to one of the memory buffers, encoded as u32 */
    V_BUF_POS           /* A location inside memory buffer, encoded as u32 */
} var_value_type_t;

typedef struct {
    var_value_type_t type;
    union {
        uint8_t u8x4[4];
        uint16_t u16x2[2];
        uint32_t u32;
        int32_t i32;
        float f32;
        stack_var_ref_t stack_var_ref;
        index_t global_var_idx;
        index_t membuf_idx;
        pos_t membuf_pos;
    } val;
} var_value_t;

var_value_t g_vars[G_VARS_COUNT];
```

**Type Usage:**
- **V_VOID**: Unused variable slot
- **V_U8**: Four 8-bit unsigned characters packed in one variable (e.g., for small strings or byte sequences)
- **V_U16**: Two 16-bit unsigned integers packed in one variable
- **V_I32**: Single signed 32-bit integer
- **V_U32**: Single unsigned 32-bit integer
- **V_UC**: Unicode codepoint (stored as signed 32-bit integer per Unicode standard)
- **V_FLOAT**: Single-precision floating-point value
- **V_GLOBAL_VAR_IDX**: Index reference to another global variable (for indirect access/pointers)
- **V_STACK_VAR_IDX**: Reference to a stack variable (contains frame_idx and var_idx)
- **V_BUF_IDX**: Index reference to a memory buffer
- **V_BUF_POS**: Position/offset within a memory buffer (for buffer indexing)

##### 3.2.2 Global Memory Buffers

Memory buffers provide array storage with explicit type tracking:

```c
#define G_MEMBUF_LEN 256
#define G_MEMBUF_COUNT 256

typedef enum {
    MB_VOID,    /* Buffer is unused */
    MB_U8,      /* Array of 256 unsigned 8-bit values */
    MB_U16,     /* Array of 128 unsigned 16-bit values */
    MB_I32,     /* Array of 64 signed 32-bit values */
    MB_U32,     /* Array of 64 unsigned 32-bit values */
    MB_FLOAT    /* Array of 64 float values */
} membuf_type_t;

typedef struct {
    membuf_type_t type;
    union {
        uint8_t u8x256[G_MEMBUF_LEN];
        uint16_t u16x128[G_MEMBUF_LEN/2];
        uint32_t u32x64[G_MEMBUF_LEN/4];
        int32_t i32x64[G_MEMBUF_LEN/4];
        float f32x64[G_MEMBUF_LEN/4];
    } buf;
} membuf_t;

membuf_t g_membuf[G_MEMBUF_COUNT];
```

**Buffer Type Usage:**
- **MB_VOID**: Unused buffer slot
- **MB_U8**: 256 bytes (used for strings, byte arrays, or raw data)
- **MB_U16**: 128 16-bit unsigned integers (for larger numeric arrays)
- **MB_I32**: 64 32-bit signed integers (for signed integer arrays)
- **MB_U32**: 64 32-bit unsigned integers (for unsigned integer arrays)
- **MB_FLOAT**: 64 single-precision floats (for floating-point arrays)

All buffers are 256 bytes in size, but the interpretation changes based on type. Strings are stored in MB_U8 buffers with null termination.

##### 3.2.3 Instruction Format

Instructions have a variable-length format with a common header and optional payload:

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

typedef struct instruction_header {
    uint8_t opcode;           /* Operation code (0-255) */
    uint8_t operand;          /* Operand: specializes the opcode */
    uint16_t payload_len : 4; /* Length of the payload in 4-byte words (0-3) */
    uint16_t imm_type1 : 4;   /* Immediate type for field 1 */
    uint16_t imm_type2 : 4;   /* Immediate type for field 2 */
    uint16_t imm_type3 : 4;   /* Immediate type for field 3 */
} instruction_header_t;

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
- `IMM_TYPE_STACK_VAR_REF`: Reference to a stack variable (frame_idx + var_idx)
- `IMM_TYPE_GLOBAL_REF`: Index to a global variable
- `IMM_TYPE_MEMBUF_REF`: Index to a memory buffer
- `IMM_TYPE_MEMBUF_POS`: Position/offset within a memory buffer

##### 3.2.4 Stack Structure

The VM uses pre-allocated stack frames with a maximum depth of 32. Each frame contains stack variables, local variables, and a return value slot for function calls.

```c
#define STACK_DEPTH 32           /* Maximum 32 nested function calls */
#define STACK_VAR_COUNT 16       /* Each frame has 16 slots for parameters/temp values */
#define STACK_LOCALS_COUNT 64    /* Maximum 64 local variables per frame */

typedef struct {
    uint16_t frame_idx; /* Stack frame index */
    uint16_t var_idx;   /* Variable index inside the stack frame */
} stack_var_ref_t;

/* Complete stack frame structure */
typedef struct {
    var_value_t stack_vars[STACK_VAR_COUNT];  /* Variables for parameter passing */
    var_value_t locals[STACK_LOCALS_COUNT];   /* Local variables */
    var_value_t ret_val;                      /* Return value */
    uint32_t return_addr;                     /* Return address (PC) */
} stack_frame_t;

/* Stack frame array */
stack_frame_t stack_frames[STACK_DEPTH];
```

**Stack Frame Components:**

- **stack_vars[]**: Used for passing parameters to functions and receiving return values
  - When calling a function, the caller writes parameter values into the callee's stack_vars[]
  - Each stack_var is a full var_value_t with type tag, supporting all data types
  - Maximum 16 stack variables per frame
  - These variables are preserved across CALL operations

- **locals[]**: Frame-local variable storage
  - Used for function local variables that persist throughout the function execution
  - Each local is a full var_value_t with type tag
  - Maximum 64 local variables per frame
  - Locals are typically zeroed when entering a function

- **ret_val**: Return value from the function
  - Single var_value_t to hold the function's return value
  - The callee sets this before returning
  - The caller reads this after the function returns
  - Supports all data types through the type tag

- **return_addr**: Saved program counter
  - Stores the PC value to return to after RET instruction
  - Set automatically by CALL instruction

### 4. Instruction Set

The opcodes are organized into categories based on operation type. Instructions use the most appropriate size (tiny, small, medium, or large) based on their operand requirements.

#### 4.1 Opcode Naming Convention

- **Prefix indicates operation type**: LOAD, STORE, ADD, SUB, MUL, DIV, CMP, JMP, etc.
- **Suffix `_G`**: Operates on global variable
- **Suffix `_L`**: Operates on local variable
- **Suffix `_S`**: Operates on stack variable
- **Suffix `_B`**: Operates on memory buffer
- **Suffix `_I`**: Uses immediate value

#### 4.2 Control Flow Operations

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

#### 4.3 Variable Load/Store Operations

##### 4.3.1 Load Operations

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x10 | LOAD_G | Small | Load global variable to stack var | operand = stack var slot, imm1 = global idx |
| 0x11 | LOAD_L | Small | Load local variable to stack var | operand = stack var slot, imm1 = local idx |
| 0x12 | LOAD_S | Small | Load stack variable to stack var | operand = dest slot, imm1 = src stack_var_ref |
| 0x13 | LOAD_I_I32 | Small | Load immediate int32 to stack var | operand = stack var slot, imm1 = value (int) |
| 0x14 | LOAD_I_U32 | Small | Load immediate uint32 to stack var | operand = stack var slot, imm1 = value (uint) |
| 0x15 | LOAD_I_F32 | Small | Load immediate float to stack var | operand = stack var slot, imm1 = value (float) |

##### 4.3.2 Store Operations

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x20 | STORE_G | Small | Store stack var to global variable | operand = stack var slot, imm1 = global idx |
| 0x21 | STORE_L | Small | Store stack var to local variable | operand = stack var slot, imm1 = local idx |
| 0x22 | STORE_S | Small | Store stack var to stack variable | operand = src slot, imm1 = dest stack_var_ref |

#### 4.4 Arithmetic Operations

All arithmetic operations work on stack variables and store results in stack variables. They are **medium instructions** (12 bytes) with two operands.

##### 4.4.1 Integer Arithmetic

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

##### 4.4.2 Float Arithmetic

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x40 | ADD_F32 | Medium | Add floats | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x41 | SUB_F32 | Medium | Subtract floats | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x42 | MUL_F32 | Medium | Multiply floats | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x43 | DIV_F32 | Medium | Divide floats | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x44 | NEG_F32 | Small | Negate float | operand = dest slot, imm1 = src slot |
| 0x45 | ABS_F32 | Small | Absolute value of float | operand = dest slot, imm1 = src slot |
| 0x46 | SQRT_F32 | Small | Square root of float | operand = dest slot, imm1 = src slot |

#### 4.5 Bitwise Operations (Unsigned Only - MISRA-C Compliant)

Per MISRA-C Rule 10.1, bitwise operations are only performed on unsigned types. All are **medium instructions** (12 bytes).

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x50 | AND_U32 | Medium | Bitwise AND | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x51 | OR_U32 | Medium | Bitwise OR | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x52 | XOR_U32 | Medium | Bitwise XOR | operand = dest slot, imm1 = src1 slot, imm2 = src2 slot |
| 0x53 | NOT_U32 | Small | Bitwise NOT | operand = dest slot, imm1 = src slot |
| 0x54 | SHL_U32 | Medium | Shift left | operand = dest slot, imm1 = value slot, imm2 = shift count slot |
| 0x55 | SHR_U32 | Medium | Logical shift right | operand = dest slot, imm1 = value slot, imm2 = shift count slot |

#### 4.6 Comparison Operations

Comparison operations set condition flags and can also store boolean results. They are **medium instructions** (12 bytes).

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x60 | CMP_I32 | Medium | Compare signed integers | operand = flags, imm1 = src1 slot, imm2 = src2 slot |
| 0x61 | CMP_U32 | Medium | Compare unsigned integers | operand = flags, imm1 = src1 slot, imm2 = src2 slot |
| 0x62 | CMP_F32 | Medium | Compare floats | operand = flags, imm1 = src1 slot, imm2 = src2 slot |

#### 4.7 Type Conversion Operations

Type conversion operations are **small instructions** (8 bytes).

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x70 | I32_TO_U32 | Small | Convert signed to unsigned int | operand = dest slot, imm1 = src slot |
| 0x71 | U32_TO_I32 | Small | Convert unsigned to signed int | operand = dest slot, imm1 = src slot |
| 0x72 | I32_TO_F32 | Small | Convert signed int to float | operand = dest slot, imm1 = src slot |
| 0x73 | U32_TO_F32 | Small | Convert unsigned int to float | operand = dest slot, imm1 = src slot |
| 0x74 | F32_TO_I32 | Small | Convert float to signed int (truncate) | operand = dest slot, imm1 = src slot |
| 0x75 | F32_TO_U32 | Small | Convert float to unsigned int (truncate) | operand = dest slot, imm1 = src slot |

#### 4.8 Memory Buffer Operations

Operations on memory buffers for array and string manipulation.

##### 4.8.1 Buffer Access

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x80 | BUF_READ | Medium | Read from buffer to stack var | operand = dest slot, imm1 = buf idx, imm2 = position |
| 0x81 | BUF_WRITE | Medium | Write stack var to buffer | operand = src slot, imm1 = buf idx, imm2 = position |
| 0x82 | BUF_LEN | Small | Get buffer element count | operand = dest slot, imm1 = buf idx |
| 0x83 | BUF_CLEAR | Small | Clear buffer | operand unused, imm1 = buf idx |

##### 4.8.2 String Operations

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0x90 | STR_CAT | Medium | Concatenate strings | operand = dest buf, imm1 = src1 buf, imm2 = src2 buf |
| 0x91 | STR_COPY | Medium | Copy string | operand = dest buf, imm1 = src buf |
| 0x92 | STR_LEN | Small | Get string length | operand = dest slot, imm1 = buf idx |
| 0x93 | STR_CMP | Medium | Compare strings | operand = flags, imm1 = buf1 idx, imm2 = buf2 idx |
| 0x94 | STR_CHR | Medium | Get character at index | operand = dest slot, imm1 = buf idx, imm2 = position |
| 0x95 | STR_SET_CHR | Large | Set character at index | operand unused, imm1 = buf idx, imm2 = position, imm3 = char value |

#### 4.9 I/O Operations

I/O operations are **small instructions** (8 bytes).

| Opcode | Name | Size | Description | Operands |
|--------|------|------|-------------|----------|
| 0xA0 | PRINT_I32 | Small | Print signed integer | operand unused, imm1 = stack var slot |
| 0xA1 | PRINT_U32 | Small | Print unsigned integer | operand unused, imm1 = stack var slot |
| 0xA2 | PRINT_F32 | Small | Print float | operand unused, imm1 = stack var slot |
| 0xA3 | PRINT_STR | Small | Print string from buffer | operand unused, imm1 = buf idx |
| 0xA4 | PRINTLN | Tiny | Print newline | - |
| 0xA5 | READ_I32 | Small | Read signed integer | operand = dest stack var slot |
| 0xA6 | READ_U32 | Small | Read unsigned integer | operand = dest stack var slot |
| 0xA7 | READ_F32 | Small | Read float | operand = dest stack var slot |
| 0xA8 | READ_STR | Small | Read string to buffer | operand unused, imm1 = dest buf idx |

### 5. VM Execution Model

#### 5.1 Initialization

On VM initialization:
1. All global variables (g_vars[]) are set to V_VOID type with zero values
2. All memory buffers (g_membuf[]) are set to MB_VOID type with cleared contents
3. Stack pointer (SP) is set to 0 (frame level 0)
4. Program counter (PC) is set to 0
5. Condition flags are cleared
6. All stack frames are zeroed

#### 5.2 Instruction Fetch-Decode-Execute Cycle

```
while PC < program_length and opcode != HALT:
    1. Fetch: Read instruction header at PC
    2. Decode: Extract opcode, operand, payload_len, and immediate types
    3. Fetch payload: Read payload_len 4-byte words based on instruction size
    4. Execute: Perform operation based on opcode
    5. Increment PC by instruction size (4 + payload_len * 4 bytes)
    6. Check for errors (stack overflow/underflow, division by zero, etc.)
```

The variable-length instruction format allows the VM to be space-efficient while maintaining alignment.

#### 5.3 Stack Variable Management

Stack variables in the current frame are the primary working storage for the VM. Unlike register-based VMs, there are no dedicated registers - all operations work directly on typed stack variables.

**Accessing Stack Variables:**
- Stack variables are accessed via the `operand` byte in the instruction header (0-15 for stack_vars[])
- Each stack variable is a full var_value_t with a type tag
- The VM checks type compatibility before operations
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

#### 5.4 Local Variable Management

Local variables provide additional storage within a frame for variables that need to persist across operations but aren't passed as parameters.

**Local Variables:**
- 64 local variables per frame (locals[0] through locals[63])
- Each is a full var_value_t with type tag
- Typically zeroed when entering a function
- Accessed via LOAD_L and STORE_L instructions

**Example:**
```c
// Store stack_vars[0] to locals[5]
STORE_L operand=0, imm1=5

// Later, load locals[5] back to stack_vars[1]
LOAD_L operand=1, imm1=5
```

#### 5.5 Global Variable Management

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

#### 5.6 Function Calls

Function calls use stack frames to maintain separate execution contexts. The mechanism relies on stack variables for parameter passing and return values.

**CALL Operation:**
1. Check stack overflow: `if (SP >= STACK_DEPTH - 1) error("stack overflow")`
2. Save return address: `stack_frames[SP + 1].return_addr = PC + 4` (PC + instruction size)
3. Increment SP: `SP++`
4. Zero the new frame's locals (stack_vars are preserved from parameter setup):
   ```c
   for (i = 0; i < STACK_LOCALS_COUNT; i++) {
       stack_frames[SP].locals[i].type = V_VOID;
       stack_frames[SP].locals[i].val.u32 = 0;
   }
   ```
5. Set PC to target address: `PC = imm1.u32`

**Parameter Passing:**
Before calling a function, the caller sets up parameters in the next frame's stack_vars[]:
```c
// Caller (at frame level 0):
LOAD_I_I32 operand=0, imm1=10          // Load 10 into stack_vars[0]
STORE_S operand=0, imm1=(frame=1,var=0) // Store to next frame's stack_vars[0]
LOAD_I_I32 operand=1, imm1=20          // Load 20 into stack_vars[1]
STORE_S operand=1, imm1=(frame=1,var=1) // Store to next frame's stack_vars[1]
CALL imm1=<function_addr>              // Call function

// Callee (now at frame level 1):
// Parameters are already in stack_vars[0] and stack_vars[1]
ADD_I32 operand=2, imm1=0, imm2=1      // Add params, result in stack_vars[2]
```

**RET Operation:**
1. Check stack underflow: `if (SP == 0) error("stack underflow")`
2. Restore PC from return address: `PC = stack_frames[SP].return_addr`
3. Decrement SP: `SP--`

**Return Values:**
Before returning, the callee stores the return value in ret_val:
```c
// Callee stores return value
STORE_S operand=2, imm1=(ret_val)      // Store result to ret_val
RET

// Caller retrieves return value
LOAD_S operand=0, imm1=(ret_val of SP+1) // Load return value to stack_vars[0]
```

**Complete Function Call Example:**
```c
# High-level: result = add(5, 3)
# where add(a, b) { return a + b; }

# Caller (at frame level 0):
LOAD_I_I32 operand=0, imm1=5           # Load param 1
STORE_S operand=0, imm1=(1,0)          # Store to next frame's stack_vars[0]
LOAD_I_I32 operand=1, imm1=3           # Load param 2
STORE_S operand=1, imm1=(1,1)          # Store to next frame's stack_vars[1]
CALL imm1=<add_addr>                   # Call add()
LOAD_S operand=0, imm1=(1,ret_val)     # Load return value
PRINT_I32 imm1=0                       # Print result
HALT

# Function 'add' (at frame level 1):
<add_addr>:
ADD_I32 operand=2, imm1=0, imm2=1      # stack_vars[2] = stack_vars[0] + stack_vars[1]
STORE_S operand=2, imm1=(ret_val)      # Store to ret_val
RET                                     # Return
```

#### 5.7 Memory Buffer Operations

Memory buffers provide array and string storage. Each buffer has a type that determines how it's interpreted.

**Buffer Indexing:**
- Buffers are 256 bytes total
- Element size depends on type: U8 (256 elements), U16 (128 elements), I32/U32/FLOAT (64 elements)
- Position is always in element units, not bytes
- Bounds checking is required based on buffer type

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

#### 5.8 Condition Flags

The VM maintains condition flags set by comparison operations:
- **Zero (Z)**: Values are equal
- **Less (L)**: First operand is less than second
- **Greater (G)**: First operand is greater than second

Flags are set by CMP_* operations and tested by conditional jump instructions (JZ, JNZ, JLT, JGT, JLE, JGE).

#### 5.9 Error Handling

The VM detects and reports the following error conditions:

1. **Stack Overflow**: SP >= STACK_DEPTH - 1 on CALL
2. **Stack Underflow**: SP == 0 on RET
3. **Division by Zero**: Integer or float division/modulo by zero
4. **Invalid Global Index**: Global variable index >= G_VARS_COUNT
5. **Invalid Local Index**: Local variable index >= STACK_LOCALS_COUNT
6. **Invalid Stack Var Index**: Stack variable index >= STACK_VAR_COUNT
7. **Invalid Buffer Index**: Buffer index >= G_MEMBUF_COUNT
8. **Invalid Buffer Position**: Position out of bounds for buffer type
9. **Type Mismatch**: Operation type doesn't match variable type
10. **Invalid Opcode**: Unrecognized opcode value
11. **Invalid PC**: PC out of program bounds

### 6. MISRA-C Compliance Details

#### 6.1 Memory Management

- **Static Allocation**: All arrays are fixed-size and statically allocated
- **Pre-allocated Frames**: Stack frames are pre-allocated (no dynamic allocation)
- **Bounds Checking**: All array accesses are validated against array bounds
- **No Pointer Arithmetic**: Array indexing uses subscript notation only
- **Fixed Limits**: Maximum stack depth (32), global variables (256), buffers (256)

#### 6.2 Type Safety

- **Tagged Union Types**: var_value_t, membuf_t use type tags to prevent misuse
- **Explicit Type Conversions**: Type conversions use dedicated opcodes
- **Fixed-Width Types**: Use `int32_t`, `uint32_t`, `uint16_t`, `uint8_t` for integer types
- **Float Requirement**: Implementation requires IEEE 754 single-precision (32-bit) floating point support
- **Enum for Opcodes and Types**: Opcodes and type tags defined as enumeration constants
- **Runtime Type Checking**: VM validates type tags before operations

#### 6.3 Unions and Variant Types

While MISRA-C generally advises against unions, Stipple uses them in a controlled manner:

- **Tagged Unions Only**: All unions (var_value_t, membuf_t, instruction_payload_t) have associated type tags
- **Type Tag Validation**: Type tag is always checked before accessing union members
- **No Type Punning**: Union members are only accessed according to their type tag
- **Deterministic**: The active union member is always known from the type tag

This usage is acceptable under MISRA-C guidelines when proper discipline is maintained.

#### 6.4 Bitwise Operations (MISRA-C Compliant)

Per MISRA-C Rule 10.1: "Shift and bitwise operations shall only be performed on operands of essentially unsigned type"

- **Unsigned-Only Bitwise Ops**: All bitwise operations (AND, OR, XOR, NOT, SHL, SHR) only operate on V_U32 typed values
- **Type Checking**: VM verifies operands are V_U32 type before bitwise operations
- **No Signed Bitwise**: V_I32 values cannot be used for bitwise operations

#### 6.5 Deterministic Behavior

- **Defined Overflow**: Integer operations use well-defined wraparound semantics for unsigned, and implementation-defined for signed (typically two's complement)
- **No Undefined Behavior**: All edge cases explicitly handled
- **Predictable Execution**: No implementation-defined behavior in VM core beyond integer overflow
- **Pre-allocated Resources**: Fixed stack depth prevents unbounded recursion
- **Bounds Checking**: All array accesses validated

### 7. Performance Considerations

#### 7.1 Memory Footprint

- **Global Variables**: 256 × sizeof(var_value_t) ≈ 256 × 12 bytes = 3 KB
- **Global Memory Buffers**: 256 × 256 bytes = 64 KB
- **Stack Frames**: 32 × sizeof(stack_frame_t)
  - 16 stack_vars × 12 bytes = 192 bytes
  - 64 locals × 12 bytes = 768 bytes
  - 1 ret_val × 12 bytes = 12 bytes
  - return_addr = 4 bytes
  - Total per frame: ~976 bytes
  - Total for 32 frames: ~31 KB
- **Total**: Approximately 98 KB base footprint (plus instruction memory)

#### 7.2 Execution Speed

- **Stack-based Design**: More memory accesses than register-based, but simpler decoder
- **Variable-length Instructions**: Compact encoding reduces instruction memory
- **Typed Operations**: Type checking overhead on each operation
- **Cache-friendly**: All data in fixed arrays with predictable access patterns
- **Predictable Timing**: No dynamic allocation or unbounded loops

#### 7.3 Instruction Encoding Efficiency

The variable-length instruction format provides good space efficiency:
- **Tiny (4 bytes)**: NOP, HALT, RET, PRINTLN - minimal operations
- **Small (8 bytes)**: Most common operations with 1 operand
- **Medium (12 bytes)**: Binary operations (arithmetic, bitwise, comparison)
- **Large (16 bytes)**: Ternary operations (rare, e.g., STR_SET_CHR)

### 8. Example Programs

#### 8.1 Simple Arithmetic

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

#### 8.2 Function Call with Parameters

```
# High-level: result = add(5, 3)
# where add(a, b) { return a + b; }

main:
    LOAD_I_I32 operand=0, imm1=5           # Prepare param 1
    STORE_S operand=0, imm1=(1,0)          # Store to next frame's stack_vars[0]
    LOAD_I_I32 operand=1, imm1=3           # Prepare param 2
    STORE_S operand=1, imm1=(1,1)          # Store to next frame's stack_vars[1]
    CALL imm1=add_func                     # Call add function
    LOAD_S operand=0, imm1=(1,ret_val)     # Get return value
    PRINT_I32 imm1=0                       # Print result
    HALT

add_func:
    # Parameters are in stack_vars[0] and stack_vars[1]
    ADD_I32 operand=2, imm1=0, imm2=1      # Add parameters
    STORE_S operand=2, imm1=(ret_val)      # Store return value
    RET
```

#### 8.3 Array Operations

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

#### 8.4 String Concatenation

```
# High-level: str3 = str1 + str2
# Buffers 0 and 1 contain strings, result in buffer 2

STR_CAT operand=2, imm1=0, imm2=1      # Concatenate buffers 0 and 1 into buffer 2
PRINT_STR imm1=2                       # Print result
HALT
```

### 9. Future Extensions

Potential enhancements while maintaining MISRA-C compliance:

1. **Extended Type System**: Additional types like V_I64, V_U64 for 64-bit integers
2. **More Memory Buffers**: Increase G_MEMBUF_COUNT for larger programs
3. **Debugging Support**: Breakpoint and trace opcodes for development
4. **Optimization**: Peephole optimization of instruction sequences
5. **Error Recovery**: Structured exception handling with try/catch mechanisms
6. **File I/O**: File operations for persistent storage
7. **Interoperability**: Controlled foreign function interface for safe C library calls
8. **Packed Arrays**: Support for bit-packed arrays to save memory

### 10. Conclusion

The Stipple VM provides a robust, MISRA-C compliant execution environment for a shell-like scripting language. The stack-based architecture with typed variables ensures safety and predictability. Key features include:

- **MISRA-C Compliance**: Limited union usage with type tags, no dynamic allocation, unsigned-only bitwise operations
- **Stack-based Design**: All operations work on typed stack variables instead of registers
- **Variable-length Instructions**: Space-efficient encoding with tiny (4B), small (8B), medium (12B), and large (16B) instructions
- **Typed Variables**: All variables (global, local, stack) have explicit type tags (var_value_t)
- **Typed Buffers**: Memory buffers for arrays/strings with type tracking (membuf_t)
- **Pre-allocated Frames**: Maximum stack depth of 32 with comprehensive bounds checking
- **Function Call Mechanism**: Stack variables for parameter passing, ret_val for return values
- **Type Safety**: Runtime type checking prevents type confusion
- **Deterministic Behavior**: Fixed resource allocation and explicit error handling

The architecture supports efficient execution while maintaining strict MISRA-C compliance through careful design of the type system, instruction format, and memory management.
