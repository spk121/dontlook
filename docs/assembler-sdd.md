# Stipple Assembler/Disassembler Tool

## Software Design Document

### 1. Overview

This document describes the design of the assembler and disassembler tools for the Stipple VM. The assembler converts human-readable assembly language text into binary bytecode for the Stipple virtual machine. The disassembler performs the reverse operation, converting binary bytecode back into human-readable assembly language.

### 2. Design Goals

- **Readable Assembly Syntax**: Clear, consistent syntax that mirrors the VM instruction set
- **One-to-One Mapping**: Direct correspondence between assembly instructions and VM bytecode
- **Error Reporting**: Clear error messages with line numbers for syntax and semantic errors
- **MISRA-C Compliance**: Both tools maintain MISRA-C compliance guidelines
- **Bidirectional**: Assembly → Binary → Assembly should produce equivalent code

### 3. Assembly Language Grammar

#### 3.1 Lexical Structure

**Comments:**
```ebnf
comment = '#' {any character except newline} newline
```

**Identifiers:**
```ebnf
identifier = letter {letter | digit | '_'}
letter = 'a'..'z' | 'A'..'Z'
digit = '0'..'9'
```

**Labels:**
```ebnf
label = identifier ':'
```

**Numbers:**
```ebnf
decimal_int = ['-'] digit {digit}
hex_int = '0x' hex_digit {hex_digit}
hex_digit = digit | 'a'..'f' | 'A'..'F'
float = ['-'] digit {digit} '.' digit {digit} [exponent]
exponent = ('e' | 'E') ['+' | '-'] digit {digit}
```

**Strings:**
```ebnf
string = '"' {character | escape_sequence} '"'
escape_sequence = '\n' | '\t' | '\r' | '\\' | '\"' | '\x' hex_digit hex_digit
```

**Register References:**
```ebnf
stack_var = 's' digit {digit}      /* s0 to s15 */
local_var = 'l' digit {digit}      /* l0 to l63 */
global_var = 'g' digit {digit}     /* g0 to g255 */
buffer = 'b' digit {digit}         /* b0 to b255 */
```

**Stack Frame References:**
```ebnf
frame_ref = 'f' digit {digit}      /* f0 to f31 */
stack_var_ref = frame_ref '.' stack_var    /* e.g., f1.s0 */
```

#### 3.2 Instruction Format

```ebnf
program = {line}
line = [label] [instruction | directive] [comment] newline

instruction = opcode [operands]
operands = operand {',' operand}
operand = immediate | register | label_ref | stack_var_ref

immediate = decimal_int | hex_int | float | string
register = stack_var | local_var | global_var | buffer
label_ref = identifier
```

#### 3.3 Directives

Directives provide additional information to the assembler:

```ebnf
directive = '.' directive_name [arguments]

directive_name = 'data' | 'string' | 'align' | 'org'
```

**Supported Directives:**

- `.org <address>`: Set the current assembly address (PC)
- `.align <bytes>`: Align to the specified byte boundary (4, 8, 12, 16)
- `.data <buffer_idx>, <type>, <values>`: Initialize a data buffer
- `.string <buffer_idx>, <string>`: Initialize a string buffer

**Examples:**
```asm
.org 0x0000           # Start at address 0
.align 4              # Align to 4-byte boundary
.string 0, "Hello"    # Initialize buffer 0 with "Hello"
.data 1, i32, 10, 20, 30, 40    # Initialize buffer 1 with integers
```

### 4. Assembly Language Syntax

#### 4.1 Instruction Syntax by Category

**Control Flow:**
```asm
nop                   # No operation
halt                  # Stop execution
jmp label             # Unconditional jump
jz label              # Jump if zero
jnz label             # Jump if not zero
jlt label             # Jump if less than
jgt label             # Jump if greater than
jle label             # Jump if less or equal
jge label             # Jump if greater or equal
call label            # Call subroutine
ret                   # Return from subroutine
```

**Load Operations:**
```asm
load.g s0, g10        # Load global 10 to stack var 0
load.l s1, l5         # Load local 5 to stack var 1
load.s s2, f1.s0      # Load from frame 1, stack var 0 to s2
load.i s0, 42         # Load immediate int32
load.u s1, 0x2A       # Load immediate uint32
load.f s2, 3.14       # Load immediate float
load.ret s0, f1       # Load return value from frame 1
```

**Store Operations:**
```asm
store.g s0, g10       # Store stack var 0 to global 10
store.l s1, l5        # Store stack var 1 to local 5
store.s s2, f1.s0     # Store s2 to frame 1, stack var 0
store.ret s0, f0      # Store s0 to current frame's return value
```

**Arithmetic Operations (Integer):**
```asm
add.i32 s2, s0, s1    # s2 = s0 + s1 (signed)
sub.i32 s2, s0, s1    # s2 = s0 - s1 (signed)
mul.i32 s2, s0, s1    # s2 = s0 * s1 (signed)
div.i32 s2, s0, s1    # s2 = s0 / s1 (signed)
mod.i32 s2, s0, s1    # s2 = s0 % s1 (signed)
neg.i32 s1, s0        # s1 = -s0 (signed)

add.u32 s2, s0, s1    # s2 = s0 + s1 (unsigned)
sub.u32 s2, s0, s1    # s2 = s0 - s1 (unsigned)
mul.u32 s2, s0, s1    # s2 = s0 * s1 (unsigned)
div.u32 s2, s0, s1    # s2 = s0 / s1 (unsigned)
mod.u32 s2, s0, s1    # s2 = s0 % s1 (unsigned)
```

**Arithmetic Operations (Float):**
```asm
add.f32 s2, s0, s1    # s2 = s0 + s1 (float)
sub.f32 s2, s0, s1    # s2 = s0 - s1 (float)
mul.f32 s2, s0, s1    # s2 = s0 * s1 (float)
div.f32 s2, s0, s1    # s2 = s0 / s1 (float)
neg.f32 s1, s0        # s1 = -s0 (float)
abs.f32 s1, s0        # s1 = |s0| (float)
sqrt.f32 s1, s0       # s1 = sqrt(s0) (float)
```

**Bitwise Operations:**
```asm
and.u32 s2, s0, s1    # s2 = s0 & s1
or.u32 s2, s0, s1     # s2 = s0 | s1
xor.u32 s2, s0, s1    # s2 = s0 ^ s1
not.u32 s1, s0        # s1 = ~s0
shl.u32 s2, s0, s1    # s2 = s0 << s1
shr.u32 s2, s0, s1    # s2 = s0 >> s1 (logical)
```

**Comparison Operations:**
```asm
cmp.i32 s0, s1        # Compare s0 with s1 (signed), set flags
cmp.u32 s0, s1        # Compare s0 with s1 (unsigned), set flags
cmp.f32 s0, s1        # Compare s0 with s1 (float), set flags
```

**Type Conversion:**
```asm
i32.to.u32 s1, s0     # Convert signed to unsigned
u32.to.i32 s1, s0     # Convert unsigned to signed
i32.to.f32 s1, s0     # Convert signed int to float
u32.to.f32 s1, s0     # Convert unsigned int to float
f32.to.i32 s1, s0     # Convert float to signed int (truncate)
f32.to.u32 s1, s0     # Convert float to unsigned int (truncate)
```

**Buffer Operations:**
```asm
buf.read s0, b10, s1     # Read from buffer 10 at position s1 to s0
buf.write s0, b10, s1    # Write s0 to buffer 10 at position s1
buf.len s0, b10          # Get buffer 10 element count
buf.clear b10            # Clear buffer 10
```

**String Operations:**
```asm
str.cat b2, b0, b1       # Concatenate buffers 0 and 1 into buffer 2
str.copy b1, b0          # Copy buffer 0 to buffer 1
str.len s0, b0           # Get string length of buffer 0
str.cmp b0, b1           # Compare strings, set flags
str.chr s0, b0, s1       # Get character at position s1 from buffer 0
str.set.chr b0, s1, s2   # Set character at position s1 in buffer 0
```

**I/O Operations:**
```asm
print.i32 s0          # Print signed integer from s0
print.u32 s0          # Print unsigned integer from s0
print.f32 s0          # Print float from s0
print.str b0          # Print string from buffer 0
println               # Print newline
read.i32 s0           # Read signed integer to s0
read.u32 s0           # Read unsigned integer to s0
read.f32 s0           # Read float to s0
read.str b0           # Read string to buffer 0
```

#### 4.2 Complete Assembly Example

```asm
# Simple program: result = add(5, 3)

.org 0x0000

main:
    # Prepare parameters for add function
    load.i s0, 5              # Load first parameter
    store.s s0, f1.s0         # Store to next frame's s0
    load.i s1, 3              # Load second parameter
    store.s s1, f1.s1         # Store to next frame's s1
    
    # Call the add function
    call add_func
    
    # Get return value and print
    load.ret s0, f1           # Get return value
    print.i32 s0              # Print result
    println
    halt

add_func:
    # Function: add(a, b) returns a + b
    # Parameters in s0 and s1
    add.i32 s2, s0, s1        # s2 = s0 + s1
    store.ret s2, f0          # Store result in return value
    ret

# Output: 8
```

### 5. Assembler Design

#### 5.1 Architecture

The assembler follows a two-pass design:

**Pass 1: Symbol Resolution**
1. Read all lines
2. Parse labels and track addresses
3. Build symbol table mapping labels to addresses
4. Track program counter (PC) accounting for instruction sizes
5. Validate directives

**Pass 2: Code Generation**
1. Parse each instruction
2. Resolve label references using symbol table
3. Generate binary bytecode
4. Apply directives (data initialization, alignment)
5. Write output binary file

#### 5.2 Data Structures

**Symbol Table:**
```c
#define MAX_SYMBOLS 256

typedef struct {
    char name[64];        /* Symbol/label name */
    uint32_t address;     /* Address in bytecode */
    bool defined;         /* Whether symbol is defined */
} symbol_t;

typedef struct {
    symbol_t symbols[MAX_SYMBOLS];
    uint32_t count;       /* Number of symbols currently stored */
} symbol_table_t;
```

**Token:**
```c
typedef enum {
    TOK_EOF,
    TOK_NEWLINE,
    TOK_IDENTIFIER,
    TOK_LABEL,
    TOK_NUMBER,
    TOK_STRING,
    TOK_REGISTER,
    TOK_COMMA,
    TOK_DOT,
    TOK_DIRECTIVE,
    TOK_COMMENT
} token_type_t;

typedef struct {
    token_type_t type;
    char text[256];
    union {
        int32_t int_val;
        uint32_t uint_val;
        float float_val;
    } value;
    uint32_t line_number;
    uint32_t column;
} token_t;
```

**Assembler State:**
```c
#define PROGRAM_MAX_SIZE 65536  /* 64KB - see docs/sdd.md section 3.1 */

typedef struct {
    symbol_table_t symbols;
    uint8_t output[PROGRAM_MAX_SIZE];
    uint32_t pc;                /* Current program counter */
    uint32_t current_line;
    char error_msg[256];
    bool has_error;
} assembler_state_t;
```

#### 5.3 Instruction Encoding

Each instruction is encoded according to the VM instruction format:

```c
typedef struct {
    char mnemonic[32];      /* e.g., "add.i32" */
    uint8_t opcode;         /* Opcode value */
    uint8_t size;           /* Instruction size: 4, 8, 12, or 16 */
    uint8_t operand_count;  /* Number of operands */
    operand_type_t operand_types[3];  /* Expected operand types */
} instruction_spec_t;

typedef enum {
    OP_NONE,
    OP_STACK_VAR,       /* s0-s15 */
    OP_LOCAL_VAR,       /* l0-l63 */
    OP_GLOBAL_VAR,      /* g0-g255 */
    OP_BUFFER,          /* b0-b255 */
    OP_IMMEDIATE_INT,   /* Immediate integer */
    OP_IMMEDIATE_UINT,  /* Immediate unsigned */
    OP_IMMEDIATE_FLOAT, /* Immediate float */
    OP_LABEL,           /* Label reference */
    OP_STACK_VAR_REF,   /* f1.s0 */
    OP_FRAME_REF        /* f0-f31 */
} operand_type_t;
```

**Instruction Encoding Algorithm:**
```c
vm_status_t encode_instruction(
    assembler_state_t* asm_state,
    const instruction_spec_t* spec,
    const operand_t operands[],
    uint8_t* output
) {
    instruction_header_t header = {0};
    header.opcode = spec->opcode;
    
    /* Encode operands based on instruction spec */
    switch (spec->size) {
        case 4:  /* Tiny instruction */
            encode_tiny_instruction(&header, output);
            break;
        case 8:  /* Small instruction */
            encode_small_instruction(&header, operands, output);
            break;
        case 12: /* Medium instruction */
            encode_medium_instruction(&header, operands, output);
            break;
        case 16: /* Large instruction */
            encode_large_instruction(&header, operands, output);
            break;
    }
    
    return VM_OK;
}
```

#### 5.4 Error Handling

The assembler provides detailed error messages:

```c
typedef enum {
    ASM_OK = 0,
    ASM_ERR_SYNTAX,              /* Syntax error */
    ASM_ERR_UNDEFINED_LABEL,     /* Undefined label reference */
    ASM_ERR_DUPLICATE_LABEL,     /* Label defined multiple times */
    ASM_ERR_INVALID_OPCODE,      /* Unknown instruction */
    ASM_ERR_INVALID_OPERAND,     /* Wrong operand type */
    ASM_ERR_OPERAND_COUNT,       /* Wrong number of operands */
    ASM_ERR_OUT_OF_RANGE,        /* Value out of range */
    ASM_ERR_PROGRAM_TOO_LARGE,   /* Program exceeds max size */
    ASM_ERR_INVALID_REGISTER,    /* Invalid register reference */
    ASM_ERR_IO_ERROR             /* File I/O error */
} asm_status_t;
```

**Error Message Format:**
```
<filename>:<line>:<column>: error: <message>
  <source line>
  <caret pointing to error position>
```

**Example:**
```
program.asm:15:10: error: undefined label 'loop_end'
    jmp loop_end
        ^~~~~~~~
```

#### 5.5 Assembler API

```c
/* Initialize assembler state */
void asm_init(assembler_state_t* asm_state);

/* Assemble source file to binary */
asm_status_t asm_assemble_file(
    const char* input_file,
    const char* output_file,
    assembler_state_t* asm_state
);

/* Assemble source string to buffer */
asm_status_t asm_assemble_string(
    const char* source,
    uint8_t* output,
    uint32_t* output_len,
    assembler_state_t* asm_state
);

/* Get error message */
const char* asm_get_error(const assembler_state_t* asm_state);
```

### 6. Disassembler Design

#### 6.1 Architecture

The disassembler is a single-pass tool that reads binary bytecode and produces assembly language output.

**Algorithm:**
1. Initialize program counter to 0
2. While PC < program length:
   - Read instruction header at PC
   - Decode opcode and operands
   - Look up instruction mnemonic
   - Format instruction as assembly text
   - Increment PC by instruction size
   - Write assembly line to output

#### 6.2 Data Structures

**Disassembler State:**
```c
typedef struct {
    const uint8_t* program;     /* Input bytecode */
    uint32_t program_len;       /* Length of bytecode */
    uint32_t pc;                /* Current program counter */
    FILE* output;               /* Output file */
    char line_buffer[512];      /* Current line being built */
    bool show_addresses;        /* Show addresses in output */
    bool show_hex;              /* Show hex bytes in output */
} disasm_state_t;
```

**Instruction Decoder:**
```c
typedef struct {
    uint8_t opcode;
    char mnemonic[32];
    uint8_t size;
    operand_info_t operands[3];
} decoded_instruction_t;

typedef struct {
    operand_type_t type;
    union {
        int32_t int_val;
        uint32_t uint_val;
        float float_val;
        struct {
            uint16_t frame_idx;
            uint16_t var_idx;
        } stack_var_ref;
    } value;
} operand_info_t;
```

#### 6.3 Instruction Decoding

```c
vm_status_t decode_instruction(
    const uint8_t* program,
    uint32_t pc,
    uint32_t program_len,
    decoded_instruction_t* instr
) {
    /* Validate PC */
    if (pc >= program_len) {
        return VM_ERR_INVALID_PC;
    }
    
    /* Read header */
    instruction_header_t header;
    memcpy(&header, &program[pc], sizeof(header));
    
    /* Look up opcode */
    instr->opcode = header.opcode;
    instr->size = 4 + (INSTR_PAYLOAD_LEN(header) * 4);
    
    /* Decode operands based on instruction type */
    decode_operands(&header, &program[pc + 4], instr->operands);
    
    /* Look up mnemonic */
    get_mnemonic(header.opcode, instr->mnemonic);
    
    return VM_OK;
}
```

#### 6.4 Output Formatting

**Default Format:**
```asm
    load.i s0, 42
    load.i s1, 3
    add.i32 s2, s0, s1
    print.i32 s2
    halt
```

**With Addresses:**
```asm
0x0000:     load.i s0, 42
0x0008:     load.i s1, 3
0x0010:     add.i32 s2, s0, s1
0x001c:     print.i32 s2
0x0024:     halt
```

**With Hex Bytes:**
```asm
0x0000: 13 00 34 00 | 2a 00 00 00     load.i s0, 42
0x0008: 13 01 34 00 | 03 00 00 00     load.i s1, 3
0x0010: 30 02 22 00 | 00 00 | 01 00   add.i32 s2, s0, s1
0x001c: a0 00 10 00 | 02 00 00 00     print.i32 s2
0x0024: 01 00 00 00                   halt
```

#### 6.5 Disassembler API

```c
/* Initialize disassembler state */
void disasm_init(disasm_state_t* disasm_state);

/* Disassemble binary file to assembly */
vm_status_t disasm_disassemble_file(
    const char* input_file,
    const char* output_file,
    disasm_state_t* disasm_state
);

/* Disassemble binary buffer to string */
vm_status_t disasm_disassemble_buffer(
    const uint8_t* input,
    uint32_t input_len,
    char* output,
    uint32_t output_size,
    disasm_state_t* disasm_state
);

/* Set output options */
void disasm_set_show_addresses(disasm_state_t* disasm_state, bool show);
void disasm_set_show_hex(disasm_state_t* disasm_state, bool show);
```

### 7. Command-Line Interface

#### 7.1 Assembler Usage

```bash
stipple-asm [options] <input.asm> -o <output.bin>

Options:
  -o <file>       Output binary file
  -l <file>       Generate listing file with addresses
  -s              Generate symbol table
  -h, --help      Show help message
  -v, --version   Show version information
```

**Example:**
```bash
$ stipple-asm program.asm -o program.bin
Assembled successfully: 256 bytes written to program.bin

$ stipple-asm program.asm -o program.bin -l program.lst -s
Assembled successfully: 256 bytes written to program.bin
Listing written to program.lst
Symbol table: 8 symbols
```

#### 7.2 Disassembler Usage

```bash
stipple-disasm [options] <input.bin> -o <output.asm>

Options:
  -o <file>       Output assembly file (default: stdout)
  -a              Show addresses
  -x              Show hex bytes
  -h, --help      Show help message
  -v, --version   Show version information
```

**Example:**
```bash
$ stipple-disasm program.bin
    load.i s0, 42
    load.i s1, 3
    add.i32 s2, s0, s1
    print.i32 s2
    halt

$ stipple-disasm -a -x program.bin -o program.asm
Disassembled successfully: 28 bytes, 5 instructions
Output written to program.asm
```

### 8. Testing Strategy

#### 8.1 Assembler Tests

**Unit Tests:**
- Lexer: Token recognition for all token types
- Parser: Instruction parsing with valid/invalid operands
- Symbol table: Label definition and resolution
- Encoder: Correct binary encoding for each instruction type
- Error handling: Proper error messages for syntax errors

**Integration Tests:**
- Complete programs: Assemble and verify bytecode
- Round-trip: Assemble → Disassemble → Assemble should be idempotent
- Edge cases: Empty files, maximum-size programs, all instruction types

**Example Test:**
```c
void test_assemble_simple_program(void) {
    const char* source = 
        "main:\n"
        "    load.i s0, 42\n"
        "    print.i32 s0\n"
        "    halt\n";
    
    uint8_t output[256];
    uint32_t output_len;
    assembler_state_t asm_state;
    
    asm_init(&asm_state);
    asm_status_t status = asm_assemble_string(
        source, output, &output_len, &asm_state
    );
    
    assert(status == ASM_OK);
    assert(output_len == 20);  /* 3 instructions */
    
    /* Verify bytecode */
    assert(output[0] == 0x13);  /* LOAD_I_I32 opcode */
    /* ... verify remaining bytes ... */
}
```

#### 8.2 Disassembler Tests

**Unit Tests:**
- Decoder: Correct decoding of all instruction types
- Formatter: Proper formatting of operands
- Error handling: Handle invalid/truncated bytecode

**Integration Tests:**
- Known bytecode: Disassemble and verify output
- Round-trip: Binary → Assembly → Binary should preserve bytecode
- All instructions: Test disassembly of all opcode types

**Example Test:**
```c
void test_disassemble_simple_program(void) {
    uint8_t bytecode[] = {
        0x13, 0x00, 0x34, 0x00, 0x2A, 0x00, 0x00, 0x00,  /* load.i s0, 42 */
        0xA0, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,  /* print.i32 s0 */
        0x01, 0x00, 0x00, 0x00                           /* halt */
    };
    
    char output[1024];
    disasm_state_t disasm_state;
    
    disasm_init(&disasm_state);
    vm_status_t status = disasm_disassemble_buffer(
        bytecode, sizeof(bytecode), output, sizeof(output), &disasm_state
    );
    
    assert(status == VM_OK);
    assert(strstr(output, "load.i s0, 42") != NULL);
    assert(strstr(output, "print.i32 s0") != NULL);
    assert(strstr(output, "halt") != NULL);
}
```

#### 8.3 Round-Trip Tests

Verify that assembly → binary → assembly produces equivalent code:

```c
void test_round_trip(void) {
    const char* original = 
        "main:\n"
        "    load.i s0, 5\n"
        "    store.s s0, f1.s0\n"
        "    call add_func\n"
        "    halt\n"
        "add_func:\n"
        "    add.i32 s0, s0, s0\n"
        "    ret\n";
    
    /* Assemble */
    uint8_t binary[256];
    uint32_t binary_len;
    assembler_state_t asm_state;
    asm_init(&asm_state);
    asm_assemble_string(original, binary, &binary_len, &asm_state);
    
    /* Disassemble */
    char reassembled[1024];
    disasm_state_t disasm_state;
    disasm_init(&disasm_state);
    disasm_disassemble_buffer(binary, binary_len, reassembled, 
                               sizeof(reassembled), &disasm_state);
    
    /* Assemble again */
    uint8_t binary2[256];
    uint32_t binary2_len;
    asm_init(&asm_state);
    asm_assemble_string(reassembled, binary2, &binary2_len, &asm_state);
    
    /* Binaries should be identical */
    assert(binary_len == binary2_len);
    assert(memcmp(binary, binary2, binary_len) == 0);
}
```

### 9. MISRA-C Compliance

Both assembler and disassembler maintain MISRA-C compliance:

**Memory Management:**
- Fixed-size arrays for symbol tables, token buffers, output buffers
- No dynamic memory allocation
- All buffers have defined maximum sizes

**Type Safety:**
- Explicit type conversions
- Fixed-width integer types (int32_t, uint32_t, etc.)
- Enumerations for token types, opcodes, error codes

**Error Handling:**
- All functions return explicit status codes
- Error messages stored in fixed buffers
- No undefined behavior on invalid input

**Input Validation:**
- Bounds checking on all array accesses
- Validation of register indices, buffer indices
- File size limits enforced

### 10. Example Assembly Programs

#### 10.1 Fibonacci Sequence

```asm
# Calculate Fibonacci numbers
# Prints first 10 Fibonacci numbers

.org 0x0000

main:
    load.i s0, 0           # fib(0) = 0
    load.i s1, 1           # fib(1) = 1
    load.i s2, 10          # counter = 10
    
    print.i32 s0           # Print first number
    println
    print.i32 s1           # Print second number
    println

fib_loop:
    # Check if done
    load.i s3, 2
    cmp.i32 s2, s3
    jle done
    
    # Calculate next Fibonacci number
    add.i32 s4, s0, s1     # s4 = s0 + s1
    print.i32 s4
    println
    
    # Shift variables: s0 = s1, s1 = s4
    store.l s0, l0         # Temp save s0
    store.l s1, l1         # Temp save s1
    load.l s0, l1          # s0 = s1
    store.l s4, l1         # Update l1 with s4
    load.l s1, l1          # s1 = s4
    
    # Decrement counter
    load.i s5, 1
    sub.i32 s2, s2, s5
    jmp fib_loop

done:
    halt
```

#### 10.2 String Manipulation

```asm
# String concatenation example

.org 0x0000
.string 0, "Hello, "
.string 1, "World!"

main:
    # Concatenate strings
    str.cat b2, b0, b1     # b2 = "Hello, " + "World!"
    
    # Print result
    print.str b2           # Print "Hello, World!"
    println
    
    # Get string length
    str.len s0, b2
    print.i32 s0           # Print length
    println
    
    halt
```

#### 10.3 Array Sum

```asm
# Sum elements of an array

.org 0x0000
.data 0, i32, 10, 20, 30, 40, 50

main:
    load.i s0, 0           # sum = 0
    load.i s1, 0           # index = 0
    load.i s2, 5           # count = 5

sum_loop:
    # Check if done
    cmp.i32 s1, s2
    jge done
    
    # Read array element
    buf.read s3, b0, s1
    
    # Add to sum
    add.i32 s0, s0, s3
    
    # Increment index
    load.i s4, 1
    add.i32 s1, s1, s4
    
    jmp sum_loop

done:
    print.i32 s0           # Print sum (150)
    println
    halt
```

### 11. Future Enhancements

**Macro Support:**
- Define reusable code patterns
- Parameter substitution
- Conditional assembly

**Optimization:**
- Peephole optimization of instruction sequences
- Dead code elimination
- Constant folding

**Debugging Support:**
- Source-level debugging information
- Breakpoint insertion
- Watch expressions

**IDE Integration:**
- Syntax highlighting definitions
- Language server protocol support
- Code completion

### 12. Conclusion

The Stipple assembler and disassembler provide essential tooling for working with the Stipple VM. The assembly language offers a human-readable, one-to-one mapping to VM bytecode, while maintaining the same MISRA-C compliance standards as the VM itself.

**Key Features:**
- Clear, consistent assembly syntax mirroring VM instruction set
- Two-pass assembler with symbol resolution and error reporting
- Single-pass disassembler with multiple output formats
- Comprehensive error messages with line/column information
- MISRA-C compliant implementation
- Bidirectional translation (assembly ↔ binary)
- Support for all VM instruction types
- Directives for data initialization and program control

The tools enable developers to write, debug, and analyze Stipple VM programs efficiently while maintaining the safety and portability guarantees of MISRA-C.
