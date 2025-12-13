#pragma once
#include <stdint.h>

/* Global Storage*/

/* Variable type identifiers */
typedef enum {
	V_VOID,             /* Variable is unused. Its contents have no meaning. */
	V_U8,               /* 4 unsigned 8-bit characters */
	V_U16,              /* 2 unsigned 16-bit integers */
	V_I32,              /* Signed 32-bit integer value */
	V_U32,              /* Unsigned 32-bit integer values */
	V_UC,               /* A Unicode codepoint encoded as a 32-bit signed integer */
	V_FLOAT,            /* Float value */
	V_GLOBAL_VAR_IDX,   /* Reference to global variable, encoded as u32 */
	V_STACK_VAR_IDX,    /* Reference to a stack variable, encoded as stack_var_ref_t */
	V_BUF_IDX,          /* Reference to one of the memory buffers, encoded as u32 */
	V_BUF_POS           /* A location inside memory buffer 0 = 1st element, 1 = 2nd element, etc. Encoded as u32 */
} var_value_type_t;

/* Global variables */
#define G_VARS_COUNT 256

typedef struct {
	uint16_t frame_idx; /* Stack frame index */
	uint16_t var_idx;   /* Variable index inside the stack frame */
} stack_var_ref_t;

typedef uint32_t index_t;
typedef uint32_t pos_t;

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

/* Global string / array storage */
#define G_MEMBUF_LEN 256
#define G_MEMBUF_COUNT 256

typedef enum {
	MB_VOID,
	MB_U8,
	MB_U16,
	MB_I32,
	MB_U32,
	MB_FLOAT
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

/* Instructions */
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

#define INSTRUCTION_HEADER_SIZE 4  /* Size of instruction header in bytes */
#define INSTRUCTION_TINY_PAYLOAD_SIZE 0
#define INSTRUCTION_SMALL_PAYLOAD_SIZE 4 /* Size of small instruction payload in bytes */
#define INSTRUCTION_MEDIUM_PAYLOAD_SIZE 8 /* Size of medium instruction payload in bytes */
#define INSTRUCTION_LARGE_PAYLOAD_SIZE 12 /* Size of large instruction payload in bytes */

typedef struct instruction_header {
	uint8_t opcode;   /* Operation code */
	uint8_t operand;  /* Operand: specializes the opcode */
	uint16_t payload_len : 4; /* Length of the payload in 4-byte words. */
	uint16_t imm_type1 : 4; /* Immediate type for field 1: interpret like imm_type_t */
	uint16_t imm_type2 : 4; /* Immediate type for field 2: interpret like imm_type_t */
	uint16_t imm_type3 : 4; /* Immediate type for field 3: interpret like imm_type_t */
} instruction_header_t;

typedef union instruction_payload {
		uint8_t u8x4[4];      /* Unsigned char immediate */
		uint16_t u16x2[2]; /* Unsigned short immediate as array */
		uint32_t u32;      /* Unsigned integer immediate */
		int32_t i32;        /* Signed integer immediate */
		float f32;          /* Float immediate */
		stack_var_ref_t stack_var_ref;
		index_t global_var_idx;
		index_t membuf_idx;
		pos_t membuf_pos;
} instruction_payload_t;

typedef struct tiny_instruction {
	instruction_header_t header;
	/* No payload for tiny instructions */
} tiny_instruction_t;

typedef struct small_instruction {
	instruction_header_t header;
	instruction_payload_t imm1; /* Immediate field 1 */
} small_instruction_t;

typedef struct medium_instruction {
	instruction_header_t header;
	instruction_payload_t imm1; /* Immediate field 1 */
	instruction_payload_t imm2; /* Immediate field 2 */
} medium_instruction_t;

typedef struct large_instruction {
	instruction_header_t header;
	instruction_payload_t imm1; /* Immediate field 1 */
	instruction_payload_t imm2; /* Immediate field 2 */
	instruction_payload_t imm3; /* Immediate field 3 */
} large_instruction_t;

/* Stacks */
#define STACK_DEPTH 32      /* Maximum nested function calls */
#define STACK_VAR_COUNT 16  /* Each frame has slots for passing variable / values in function calls */
#define STACK_LOCALS_COUNT 64 /* Maximum number of local variables at a given stack depth */

/* Complete stack frame structure */
typedef struct {
	var_value_t stack_vars[STACK_VAR_COUNT];  /* Variables for this frame */
	var_value_t locals[STACK_LOCALS_COUNT];
	var_value_t ret_val;                     /* Return value */
	uint32_t return_addr;                    /* Return address (PC) */
} stack_frame_t;

/* Stack frame array */
stack_frame_t stack_frames[STACK_DEPTH];
