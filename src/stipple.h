#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
 * Stipple - MISRA-C Compliant Language Interpreter
 * VM Header File (Revised)
 */

/* ============================================================================
 * VM Configuration Constants
 * ============================================================================ */

/* Global storage limits */
#define G_VARS_COUNT 256
#define G_MEMBUF_COUNT 256
#define G_MEMBUF_LEN 256

/* Buffer element counts based on type */
#define MEMBUF_U8_COUNT  256  /* MB_U8: 256 elements */
#define MEMBUF_U16_COUNT 128  /* MB_U16: 128 elements */
#define MEMBUF_I32_COUNT 64   /* MB_I32: 64 elements */
#define MEMBUF_U32_COUNT 64   /* MB_U32: 64 elements */
#define MEMBUF_F32_COUNT 64   /* MB_FLOAT: 64 elements */

/* Stack configuration */
#define STACK_DEPTH 32           /* Maximum nested function calls */
#define STACK_VAR_COUNT 16       /* Stack variables per frame */
#define STACK_LOCALS_COUNT 64    /* Local variables per frame */

/* Instruction memory */
#define PROGRAM_MAX_SIZE 65536   /* 64KB instruction memory */

/* Instruction sizes in bytes */
#define INSTRUCTION_HEADER_SIZE 4
#define INSTRUCTION_TINY_SIZE 4
#define INSTRUCTION_SMALL_SIZE 8
#define INSTRUCTION_MEDIUM_SIZE 12
#define INSTRUCTION_LARGE_SIZE 16

/* Maximum payload length in 4-byte words */
#define INSTRUCTION_MAX_PAYLOAD_WORDS 3

/* ============================================================================
 * VM Status Codes
 * ============================================================================ */

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
	VM_ERR_OVERFLOW,              /* Arithmetic overflow or invalid float result */
	VM_ERR_HALT                   /* HALT instruction executed (not an error) */
} vm_status_t;

/* ============================================================================
 * Variable Types and Values
 * ============================================================================ */

/* Variable type identifiers */
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

/* Type aliases for clarity */
typedef uint32_t index_t;
typedef uint32_t pos_t;

/* Stack variable reference structure */
typedef struct {
	uint16_t frame_idx; /* Stack frame index (0-31) */
	uint16_t var_idx;   /* Variable index within frame */
} stack_var_ref_t;

/* Variant type for variable values */
typedef struct {
	var_value_type_t type;
	union {
		int32_t i32;                    /* V_I32 */
		uint32_t u32;                   /* V_U32, V_UC, V_GLOBAL_VAR_IDX, V_BUF_IDX, V_BUF_POS */
		float f32;                      /* V_FLOAT */
		uint8_t u8x4[4];                /* V_U8 */
		uint16_t u16x2[2];              /* V_U16 */
		stack_var_ref_t stack_var_ref;  /* V_STACK_VAR_IDX */
	} val;
} var_value_t;

/* Static assertion to verify var_value_t size */
_Static_assert(sizeof(var_value_t) <= 12, "var_value_t too large");

/* ============================================================================
 * Memory Buffers
 * ============================================================================ */

/* Memory buffer type identifiers */
typedef enum {
	MB_VOID = 0,    /* Buffer is unused */
	MB_U8,          /* Array of 256 unsigned 8-bit values */
	MB_U16,         /* Array of 128 unsigned 16-bit values */
	MB_I32,         /* Array of 64 signed 32-bit values */
	MB_U32,         /* Array of 64 unsigned 32-bit values */
	MB_FLOAT        /* Array of 64 float values */
} membuf_type_t;

/* Memory buffer with typed storage */
typedef struct {
	membuf_type_t type;
	union {
		uint8_t u8x256[G_MEMBUF_LEN];
		uint16_t u16x128[G_MEMBUF_LEN / 2];
		int32_t i32x64[G_MEMBUF_LEN / 4];
		uint32_t u32x64[G_MEMBUF_LEN / 4];
		float f32x64[G_MEMBUF_LEN / 4];
	} buf;
} membuf_t;

/* Static assertion to verify membuf_t size */
_Static_assert(sizeof(membuf_t) <= 260, "membuf_t too large");

/* ============================================================================
 * Stack Frame Structure
 * ============================================================================ */

/* Complete stack frame structure */
typedef struct {
	var_value_t stack_vars[STACK_VAR_COUNT];  /* Parameter passing variables */
	var_value_t locals[STACK_LOCALS_COUNT];   /* Local variables */
	var_value_t ret_val;                      /* Return value */
	uint32_t return_addr;                     /* Return address (PC) */
} stack_frame_t;

/* ============================================================================
 * Instruction Format
 * ============================================================================ */

/* Immediate value type identifiers */
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

/*
 * Instruction header - 4 bytes, portable format
 * Avoids bitfields for portability across compilers/platforms
 */
typedef struct {
	uint8_t opcode;   /* Operation code */
	uint8_t operand;  /* Operand: specializes the opcode */
	uint8_t flags;    /* Bits 0-3: payload_len, Bits 4-7: imm_type1 */
	uint8_t types;    /* Bits 0-3: imm_type2, Bits 4-7: imm_type3 */
} instruction_header_t;

/* Instruction header accessor macros */
#define INSTR_PAYLOAD_LEN(hdr)  ((hdr).flags & 0x0Fu)
#define INSTR_IMM_TYPE1(hdr)    (((hdr).flags >> 4) & 0x0Fu)
#define INSTR_IMM_TYPE2(hdr)    ((hdr).types & 0x0Fu)
#define INSTR_IMM_TYPE3(hdr)    (((hdr).types >> 4) & 0x0Fu)

/* Instruction header setter macros */
#define SET_INSTR_PAYLOAD_LEN(hdr, len)  ((hdr).flags = ((hdr).flags & 0xF0u) | ((len) & 0x0Fu))
#define SET_INSTR_IMM_TYPE1(hdr, type)   ((hdr).flags = ((hdr).flags & 0x0Fu) | (((type) & 0x0Fu) << 4))
#define SET_INSTR_IMM_TYPE2(hdr, type)   ((hdr).types = ((hdr).types & 0xF0u) | ((type) & 0x0Fu))
#define SET_INSTR_IMM_TYPE3(hdr, type)   ((hdr).types = ((hdr).types & 0x0Fu) | (((type) & 0x0Fu) << 4))

/* Instruction payload - 4 bytes, can hold various immediate types */
typedef union {
	uint8_t u8x4[4];           /* 4 unsigned chars */
	uint16_t u16x2[2];         /* 2 unsigned shorts */
	uint32_t u32;              /* Unsigned int */
	int32_t i32;               /* Signed int */
	float f32;                 /* Float */
	stack_var_ref_t stack_var_ref; /* Stack variable reference */
	index_t global_var_idx;    /* Global variable index */
	index_t membuf_idx;        /* Memory buffer index */
	pos_t membuf_pos;          /* Memory buffer position */
} instruction_payload_t;

/* Instruction structures for different sizes */
typedef struct {
	instruction_header_t header;
} tiny_instruction_t;

typedef struct {
	instruction_header_t header;
	instruction_payload_t imm1;
} small_instruction_t;

typedef struct {
	instruction_header_t header;
	instruction_payload_t imm1;
	instruction_payload_t imm2;
} medium_instruction_t;

typedef struct {
	instruction_header_t header;
	instruction_payload_t imm1;
	instruction_payload_t imm2;
	instruction_payload_t imm3;
} large_instruction_t;

/* Static assertions for instruction sizes */
_Static_assert(sizeof(instruction_header_t) == INSTRUCTION_HEADER_SIZE, 
               "instruction_header_t must be 4 bytes");
_Static_assert(sizeof(tiny_instruction_t) == INSTRUCTION_TINY_SIZE, 
               "tiny_instruction_t must be 4 bytes");
_Static_assert(sizeof(small_instruction_t) == INSTRUCTION_SMALL_SIZE, 
               "small_instruction_t must be 8 bytes");
_Static_assert(sizeof(medium_instruction_t) == INSTRUCTION_MEDIUM_SIZE, 
               "medium_instruction_t must be 12 bytes");
_Static_assert(sizeof(large_instruction_t) == INSTRUCTION_LARGE_SIZE, 
               "large_instruction_t must be 16 bytes");

/* ============================================================================
 * Opcode Definitions
 * ============================================================================ */

typedef enum {
	/* Control Flow Operations (0x00-0x0F) */
	OP_NOP = 0x00,      /* No operation */
	OP_HALT = 0x01,     /* Stop execution */
	OP_JMP = 0x02,      /* Unconditional jump */
	OP_JZ = 0x03,       /* Jump if zero */
	OP_JNZ = 0x04,      /* Jump if not zero */
	OP_JLT = 0x05,      /* Jump if less than */
	OP_JGT = 0x06,      /* Jump if greater than */
	OP_JLE = 0x07,      /* Jump if less or equal */
	OP_JGE = 0x08,      /* Jump if greater or equal */
	OP_CALL = 0x09,     /* Call subroutine */
	OP_RET = 0x0A,      /* Return from subroutine */

	/* Variable Load Operations (0x10-0x1F) */
	OP_LOAD_G = 0x10,       /* Load global variable to stack var */
	OP_LOAD_L = 0x11,       /* Load local variable to stack var */
	OP_LOAD_S = 0x12,       /* Load from another frame's stack var */
	OP_LOAD_I_I32 = 0x13,   /* Load immediate int32 to stack var */
	OP_LOAD_I_U32 = 0x14,   /* Load immediate uint32 to stack var */
	OP_LOAD_I_F32 = 0x15,   /* Load immediate float to stack var */
	OP_LOAD_RET = 0x16,     /* Load return value from frame */

	/* Variable Store Operations (0x20-0x2F) */
	OP_STORE_G = 0x20,      /* Store stack var to global variable */
	OP_STORE_L = 0x21,      /* Store stack var to local variable */
	OP_STORE_S = 0x22,      /* Store to another frame's stack var */
	OP_STORE_RET = 0x23,    /* Store to return value of frame */

	/* Signed Integer Arithmetic (0x30-0x3F) */
	OP_ADD_I32 = 0x30,      /* Add signed integers */
	OP_SUB_I32 = 0x31,      /* Subtract signed integers */
	OP_MUL_I32 = 0x32,      /* Multiply signed integers */
	OP_DIV_I32 = 0x33,      /* Divide signed integers */
	OP_MOD_I32 = 0x34,      /* Modulo signed integers */
	OP_NEG_I32 = 0x35,      /* Negate signed integer */
	OP_ADD_U32 = 0x36,      /* Add unsigned integers */
	OP_SUB_U32 = 0x37,      /* Subtract unsigned integers */
	OP_MUL_U32 = 0x38,      /* Multiply unsigned integers */
	OP_DIV_U32 = 0x39,      /* Divide unsigned integers */
	OP_MOD_U32 = 0x3A,      /* Modulo unsigned integers */

	/* Float Arithmetic (0x40-0x4F) */
	OP_ADD_F32 = 0x40,      /* Add floats */
	OP_SUB_F32 = 0x41,      /* Subtract floats */
	OP_MUL_F32 = 0x42,      /* Multiply floats */
	OP_DIV_F32 = 0x43,      /* Divide floats */
	OP_NEG_F32 = 0x44,      /* Negate float */
	OP_ABS_F32 = 0x45,      /* Absolute value of float */
	OP_SQRT_F32 = 0x46,     /* Square root of float */

	/* Bitwise Operations - Unsigned Only (0x50-0x5F) */
	OP_AND_U32 = 0x50,      /* Bitwise AND */
	OP_OR_U32 = 0x51,       /* Bitwise OR */
	OP_XOR_U32 = 0x52,      /* Bitwise XOR */
	OP_NOT_U32 = 0x53,      /* Bitwise NOT */
	OP_SHL_U32 = 0x54,      /* Shift left */
	OP_SHR_U32 = 0x55,      /* Logical shift right */

	/* Comparison Operations (0x60-0x6F) */
	OP_CMP_I32 = 0x60,      /* Compare signed integers */
	OP_CMP_U32 = 0x61,      /* Compare unsigned integers */
	OP_CMP_F32 = 0x62,      /* Compare floats */

	/* Type Conversion Operations (0x70-0x7F) */
	OP_I32_TO_U32 = 0x70,   /* Convert signed to unsigned int */
	OP_U32_TO_I32 = 0x71,   /* Convert unsigned to signed int */
	OP_I32_TO_F32 = 0x72,   /* Convert signed int to float */
	OP_U32_TO_F32 = 0x73,   /* Convert unsigned int to float */
	OP_F32_TO_I32 = 0x74,   /* Convert float to signed int (truncate) */
	OP_F32_TO_U32 = 0x75,   /* Convert float to unsigned int (truncate) */

	/* Memory Buffer Access (0x80-0x8F) */
	OP_BUF_READ = 0x80,     /* Read from buffer to stack var */
	OP_BUF_WRITE = 0x81,    /* Write stack var to buffer */
	OP_BUF_LEN = 0x82,      /* Get buffer element count */
	OP_BUF_CLEAR = 0x83,    /* Clear buffer */

	/* String Operations (0x90-0x9F) */
	OP_STR_CAT = 0x90,      /* Concatenate strings */
	OP_STR_COPY = 0x91,     /* Copy string */
	OP_STR_LEN = 0x92,      /* Get string length */
	OP_STR_CMP = 0x93,      /* Compare strings */
	OP_STR_CHR = 0x94,      /* Get character at index */
	OP_STR_SET_CHR = 0x95,  /* Set character at index */

	/* I/O Operations (0xA0-0xAF) */
	OP_PRINT_I32 = 0xA0,    /* Print signed integer */
	OP_PRINT_U32 = 0xA1,    /* Print unsigned integer */
	OP_PRINT_F32 = 0xA2,    /* Print float */
	OP_PRINT_STR = 0xA3,    /* Print string from buffer */
	OP_PRINTLN = 0xA4,      /* Print newline */
	OP_READ_I32 = 0xA5,     /* Read signed integer */
	OP_READ_U32 = 0xA6,     /* Read unsigned integer */
	OP_READ_F32 = 0xA7,     /* Read float */
	OP_READ_STR = 0xA8,     /* Read string to buffer */

	/* Reserved ranges for future expansion */
	/* 0x0B-0x0F: Control flow extensions */
	/* 0x17-0x1F: Load operation extensions */
	/* 0x24-0x2F: Store operation extensions */
	/* 0x3B-0x3F: Integer arithmetic extensions */
	/* 0x47-0x4F: Float arithmetic extensions */
	/* 0x56-0x5F: Bitwise operation extensions */
	/* 0x63-0x6F: Comparison extensions */
	/* 0x76-0x7F: Type conversion extensions */
	/* 0x84-0x8F: Buffer operation extensions */
	/* 0x96-0x9F: String operation extensions */
	/* 0xA9-0xAF: I/O operation extensions */
	/* 0xB0-0xFF: Reserved for future use */

	OP_MAX = 0xA9  /* One past last valid opcode */
} opcode_t;

/* ============================================================================
 * VM State Structure
 * ============================================================================ */

/* Condition flags */
#define FLAG_ZERO    0x01u  /* Zero flag (Z) - values are equal */
#define FLAG_LESS    0x02u  /* Less flag (L) - first < second */
#define FLAG_GREATER 0x04u  /* Greater flag (G) - first > second */

/* Complete VM state */
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

/* ============================================================================
 * Helper Functions and Macros
 * ============================================================================ */

/* Stack variable reference construction */
static inline stack_var_ref_t make_stack_var_ref(uint16_t frame, uint16_t var)
{
	stack_var_ref_t ref;
	ref.frame_idx = frame;
	ref.var_idx = var;
	return ref;
}

/* Get buffer element count based on type */
static inline uint32_t get_buffer_capacity(membuf_type_t type)
{
	switch (type) {
		case MB_U8:    return MEMBUF_U8_COUNT;
		case MB_U16:   return MEMBUF_U16_COUNT;
		case MB_I32:   return MEMBUF_I32_COUNT;
		case MB_U32:   return MEMBUF_U32_COUNT;
		case MB_FLOAT: return MEMBUF_F32_COUNT;
		case MB_VOID:
		default:       return 0u;
	}
}

/* Calculate instruction size in bytes based on payload length */
static inline uint32_t get_instruction_size(uint8_t payload_len)
{
	return INSTRUCTION_HEADER_SIZE + (payload_len * 4u);
}

/* Validate opcode */
static inline bool is_valid_opcode(uint8_t opcode)
{
	return opcode < OP_MAX;
}

/* ============================================================================
 * VM API Functions
 * ============================================================================ */

/* Initialize VM to default state */
void vm_init(vm_state_t* vm);

/* Reset VM state (clear all variables, reset PC and SP) */
void vm_reset(vm_state_t* vm);

/* Load program into instruction memory */
vm_status_t vm_load_program(vm_state_t* vm, const uint8_t* program, uint32_t len);

/* Execute one instruction */
vm_status_t vm_step(vm_state_t* vm);

/* Execute until HALT or error */
vm_status_t vm_run(vm_state_t* vm);

/* Get human-readable error message */
const char* vm_get_error_string(vm_status_t status);

/* Validation helpers */
bool validate_global_idx(index_t idx);
bool validate_local_idx(index_t idx);
bool validate_stack_var_idx(index_t idx);
bool validate_buffer_idx(index_t idx);
bool validate_buffer_pos(membuf_type_t type, pos_t pos);

/* ============================================================================
 * Debug/Inspection Functions
 * ============================================================================ */

/* Get string representation of variable type */
const char* var_type_to_string(var_value_type_t type);

/* Get string representation of buffer type */
const char* buffer_type_to_string(membuf_type_t type);

/* Get string representation of opcode */
const char* opcode_to_string(opcode_t opcode);

/* Disassemble instruction at current PC */
void vm_disassemble_instruction(const vm_state_t* vm, uint32_t pc);

/* Dump VM state for debugging */
void vm_dump_state(const vm_state_t* vm);
