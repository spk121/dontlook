/*
 * Stipple VM Core Implementation
 * MISRA-C Compliant Virtual Machine
 */

#include "stipple.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>  /* For INT32_MIN */

/* ============================================================================
 * Helper Functions - MISRA-C Compliant I/O (no printf/fprintf)
 * ============================================================================ */

static void print_i32(int32_t value) {
    char buf[12];  /* Enough for -2147483648 + null */
    int i = 0;
    bool negative = false;
    uint32_t uval;
    
    if (value < 0) {
        negative = true;
        /* Handle INT32_MIN specially to avoid undefined behavior */
        if (value == INT32_MIN) {
            uval = 2147483648u;
        } else {
            uval = (uint32_t)(-value);
        }
    } else {
        uval = (uint32_t)value;
    }
    
    if (uval == 0u) {
        (void)fputc('0', stdout);
        return;
    }
    
    while (uval > 0u) {
        buf[i] = (char)('0' + (uval % 10u));
        uval /= 10u;
        i++;
    }
    
    if (negative) {
        (void)fputc('-', stdout);
    }
    
    while (i > 0) {
        i--;
        (void)fputc(buf[i], stdout);
    }
}

static void print_u32(uint32_t value) {
    char buf[12];
    int i = 0;
    
    if (value == 0u) {
        (void)fputc('0', stdout);
        return;
    }
    
    while (value > 0u) {
        buf[i] = (char)('0' + (value % 10u));
        value /= 10u;
        i++;
    }
    
    while (i > 0) {
        i--;
        (void)fputc(buf[i], stdout);
    }
}

static void print_f32(float value) {
    /* Simple float printing - integer part, dot, 6 decimal places */
    int32_t int_part;
    float frac_part;
    uint32_t frac_val;
    int i;
    
    if (value < 0.0f) {
        (void)fputc('-', stdout);
        value = -value;
    }
    
    int_part = (int32_t)value;
    frac_part = value - (float)int_part;
    
    print_i32(int_part);
    (void)fputc('.', stdout);
    
    /* Print 6 decimal places */
    frac_val = (uint32_t)(frac_part * 1000000.0f);
    for (i = 0; i < 6; i++) {
        (void)fputc((char)('0' + (frac_val / 100000u)), stdout);
        frac_val = (frac_val % 100000u) * 10u;
    }
}

static void print_hex16(uint16_t value) {
    const char hex[] = "0123456789ABCDEF";
    (void)fputc('0', stdout);
    (void)fputc('x', stdout);
    (void)fputc(hex[(value >> 12) & 0xFu], stdout);
    (void)fputc(hex[(value >> 8) & 0xFu], stdout);
    (void)fputc(hex[(value >> 4) & 0xFu], stdout);
    (void)fputc(hex[value & 0xFu], stdout);
}

static void print_hex8(uint8_t value) {
    const char hex[] = "0123456789ABCDEF";
    (void)fputc('0', stdout);
    (void)fputc('x', stdout);
    (void)fputc(hex[(value >> 4) & 0xFu], stdout);
    (void)fputc(hex[value & 0xFu], stdout);
}

/* ============================================================================
 * Helper Functions - String Conversion
 * ============================================================================ */

const char* var_type_to_string(var_value_type_t type) {
    const char* types[] = {"void", "i32", "u32", "float", "u8x4", "u16x2", 
                           "unicode", "global_ref", "stack_ref", "buffer_ref", "buffer_pos"};
    return (type <= V_BUF_POS) ? types[type] : "unknown";
}

const char* buffer_type_to_string(membuf_type_t type) {
    const char* types[] = {"void", "u8[256]", "u16[128]", "i32[64]", "u32[64]", "float[64]"};
    return (type <= MB_FLOAT) ? types[type] : "unknown";
}

const char* opcode_to_string(opcode_t opcode) {
    if (opcode >= OP_MAX) return "unknown";
    const char* ops[OP_MAX] = {
        [OP_NOP] = "nop", [OP_HALT] = "halt", [OP_JMP] = "jmp", [OP_JZ] = "jz",
        [OP_JNZ] = "jnz", [OP_JLT] = "jlt", [OP_JGT] = "jgt", [OP_JLE] = "jle",
        [OP_JGE] = "jge", [OP_CALL] = "call", [OP_RET] = "ret",
        [OP_LOAD_G] = "load.g", [OP_LOAD_L] = "load.l", [OP_LOAD_S] = "load.s",
        [OP_LOAD_I_I32] = "load.i32", [OP_LOAD_I_U32] = "load.u32",
        [OP_LOAD_I_F32] = "load.f32", [OP_LOAD_RET] = "load.ret",
        [OP_STORE_G] = "store.g", [OP_STORE_L] = "store.l",
        [OP_STORE_S] = "store.s", [OP_STORE_RET] = "store.ret",
        [OP_ADD_I32] = "add.i32", [OP_SUB_I32] = "sub.i32",
        [OP_MUL_I32] = "mul.i32", [OP_DIV_I32] = "div.i32",
        [OP_MOD_I32] = "mod.i32", [OP_NEG_I32] = "neg.i32",
        [OP_ADD_U32] = "add.u32", [OP_SUB_U32] = "sub.u32",
        [OP_MUL_U32] = "mul.u32", [OP_DIV_U32] = "div.u32", [OP_MOD_U32] = "mod.u32",
        [OP_ADD_F32] = "add.f32", [OP_SUB_F32] = "sub.f32",
        [OP_MUL_F32] = "mul.f32", [OP_DIV_F32] = "div.f32",
        [OP_NEG_F32] = "neg.f32", [OP_ABS_F32] = "abs.f32", [OP_SQRT_F32] = "sqrt.f32",
        [OP_AND_U32] = "and.u32", [OP_OR_U32] = "or.u32",
        [OP_XOR_U32] = "xor.u32", [OP_NOT_U32] = "not.u32",
        [OP_SHL_U32] = "shl.u32", [OP_SHR_U32] = "shr.u32",
        [OP_CMP_I32] = "cmp.i32", [OP_CMP_U32] = "cmp.u32", [OP_CMP_F32] = "cmp.f32",
        [OP_I32_TO_U32] = "i32.to.u32", [OP_U32_TO_I32] = "u32.to.i32",
        [OP_I32_TO_F32] = "i32.to.f32", [OP_U32_TO_F32] = "u32.to.f32",
        [OP_F32_TO_I32] = "f32.to.i32", [OP_F32_TO_U32] = "f32.to.u32",
        [OP_BUF_READ] = "buf.read", [OP_BUF_WRITE] = "buf.write",
        [OP_BUF_LEN] = "buf.len", [OP_BUF_CLEAR] = "buf.clear",
        [OP_STR_CAT] = "str.cat", [OP_STR_COPY] = "str.copy",
        [OP_STR_LEN] = "str.len", [OP_STR_CMP] = "str.cmp",
        [OP_STR_CHR] = "str.chr", [OP_STR_SET_CHR] = "str.set_chr",
        [OP_PRINT_I32] = "print.i32", [OP_PRINT_U32] = "print.u32",
        [OP_PRINT_F32] = "print.f32", [OP_PRINT_STR] = "print.str",
        [OP_PRINTLN] = "println",
        [OP_READ_I32] = "read.i32", [OP_READ_U32] = "read.u32",
        [OP_READ_F32] = "read.f32", [OP_READ_STR] = "read.str"
    };
    return ops[opcode] ? ops[opcode] : "unknown";
}

const char* vm_get_error_string(vm_status_t status) {
    const char* errors[] = {
        [VM_OK] = "Success", [VM_ERR_STACK_OVERFLOW] = "Stack overflow",
        [VM_ERR_STACK_UNDERFLOW] = "Stack underflow", [VM_ERR_DIV_BY_ZERO] = "Division by zero",
        [VM_ERR_INVALID_OPCODE] = "Invalid opcode", [VM_ERR_TYPE_MISMATCH] = "Type mismatch",
        [VM_ERR_BOUNDS] = "Array bounds exceeded", [VM_ERR_INVALID_GLOBAL_IDX] = "Invalid global index",
        [VM_ERR_INVALID_LOCAL_IDX] = "Invalid local index", [VM_ERR_INVALID_STACK_VAR_IDX] = "Invalid stack var index",
        [VM_ERR_INVALID_BUFFER_IDX] = "Invalid buffer index", [VM_ERR_INVALID_BUFFER_POS] = "Invalid buffer position",
        [VM_ERR_INVALID_PC] = "Invalid program counter", [VM_ERR_INVALID_INSTRUCTION] = "Invalid instruction",
        [VM_ERR_PROGRAM_TOO_LARGE] = "Program too large", [VM_ERR_HALT] = "Program halted"
    };
    return (status <= VM_ERR_HALT) ? errors[status] : "Unknown error";
}

bool validate_global_idx(index_t idx) { return idx < G_VARS_COUNT; }
bool validate_local_idx(index_t idx) { return idx < STACK_LOCALS_COUNT; }
bool validate_stack_var_idx(index_t idx) { return idx < STACK_VAR_COUNT; }
bool validate_buffer_idx(index_t idx) { return idx < G_MEMBUF_COUNT; }
bool validate_buffer_pos(membuf_type_t type, pos_t pos) {
    return pos < get_buffer_capacity(type);
}

void vm_init(vm_state_t* vm) {
    memset(vm, 0, sizeof(*vm));
    for (uint32_t i = 0; i < G_VARS_COUNT; i++) vm->g_vars[i].type = V_VOID;
    for (uint32_t i = 0; i < G_MEMBUF_COUNT; i++) vm->g_membuf[i].type = MB_VOID;
    for (uint32_t i = 0; i < STACK_DEPTH; i++) {
        for (uint32_t j = 0; j < STACK_VAR_COUNT; j++)
            vm->stack_frames[i].stack_vars[j].type = V_VOID;
        for (uint32_t j = 0; j < STACK_LOCALS_COUNT; j++)
            vm->stack_frames[i].locals[j].type = V_VOID;
        vm->stack_frames[i].ret_val.type = V_VOID;
    }
}

void vm_reset(vm_state_t* vm) { vm_init(vm); }

vm_status_t vm_load_program(vm_state_t* vm, const uint8_t* program, uint32_t len) {
    if (len > PROGRAM_MAX_SIZE) {
        vm->last_error = VM_ERR_PROGRAM_TOO_LARGE;
        return VM_ERR_PROGRAM_TOO_LARGE;
    }
    memcpy(vm->program, program, len);
    vm->program_len = len;
    vm->pc = 0;
    vm->last_error = VM_OK;
    return VM_OK;
}

static inline var_value_t* get_stack_var(vm_state_t* vm, uint8_t idx) {
    return (idx < STACK_VAR_COUNT) ? &vm->stack_frames[vm->sp].stack_vars[idx] : NULL;
}

static inline var_value_t* get_local_var(vm_state_t* vm, uint32_t idx) {
    return (idx < STACK_LOCALS_COUNT) ? &vm->stack_frames[vm->sp].locals[idx] : NULL;
}

static inline var_value_t* get_global_var(vm_state_t* vm, uint32_t idx) {
    return (idx < G_VARS_COUNT) ? &vm->g_vars[idx] : NULL;
}

/* Minimal instruction execution - implements only key instructions */
vm_status_t vm_step(vm_state_t* vm) {
    if (vm->pc >= vm->program_len || vm->program_len - vm->pc < 4) {
        vm->last_error = VM_ERR_INVALID_PC;
        return VM_ERR_INVALID_PC;
    }
    
    instruction_header_t hdr;
    memcpy(&hdr, &vm->program[vm->pc], 4);
    
    uint8_t payload_len = INSTR_PAYLOAD_LEN(hdr);
    uint32_t instr_size = 4 + (payload_len * 4);
    
    if (vm->pc + instr_size > vm->program_len || payload_len > 3) {
        vm->last_error = VM_ERR_INVALID_INSTRUCTION;
        return VM_ERR_INVALID_INSTRUCTION;
    }
    
    instruction_payload_t imm1 = {0}, imm2 = {0}, imm3 = {0};
    if (payload_len >= 1) memcpy(&imm1, &vm->program[vm->pc + 4], 4);
    if (payload_len >= 2) memcpy(&imm2, &vm->program[vm->pc + 8], 4);
    if (payload_len >= 3) memcpy(&imm3, &vm->program[vm->pc + 12], 4);
    
    uint32_t next_pc = vm->pc + instr_size;
    vm_status_t status = VM_OK;
    
    switch (hdr.opcode) {
        case OP_NOP:
            break;
        case OP_HALT:
            status = VM_ERR_HALT;
            break;
            
        /* Control Flow */
        case OP_JMP:
            if (imm1.u32 >= vm->program_len) { status = VM_ERR_INVALID_PC; break; }
            next_pc = imm1.u32;
            break;
        case OP_JZ:
            if ((vm->flags & FLAG_ZERO) != 0) {
                if (imm1.u32 >= vm->program_len) { status = VM_ERR_INVALID_PC; break; }
                next_pc = imm1.u32;
            }
            break;
        case OP_JNZ:
            if ((vm->flags & FLAG_ZERO) == 0) {
                if (imm1.u32 >= vm->program_len) { status = VM_ERR_INVALID_PC; break; }
                next_pc = imm1.u32;
            }
            break;
        case OP_JLT:
            if ((vm->flags & FLAG_LESS) != 0) {
                if (imm1.u32 >= vm->program_len) { status = VM_ERR_INVALID_PC; break; }
                next_pc = imm1.u32;
            }
            break;
        case OP_JGT:
            if ((vm->flags & FLAG_GREATER) != 0) {
                if (imm1.u32 >= vm->program_len) { status = VM_ERR_INVALID_PC; break; }
                next_pc = imm1.u32;
            }
            break;
        case OP_JLE:
            if (((vm->flags & FLAG_LESS) != 0) || ((vm->flags & FLAG_ZERO) != 0)) {
                if (imm1.u32 >= vm->program_len) { status = VM_ERR_INVALID_PC; break; }
                next_pc = imm1.u32;
            }
            break;
        case OP_JGE:
            if (((vm->flags & FLAG_GREATER) != 0) || ((vm->flags & FLAG_ZERO) != 0)) {
                if (imm1.u32 >= vm->program_len) { status = VM_ERR_INVALID_PC; break; }
                next_pc = imm1.u32;
            }
            break;
        case OP_CALL:
            if (vm->sp >= STACK_DEPTH - 1) { status = VM_ERR_STACK_OVERFLOW; break; }
            if (imm1.u32 >= vm->program_len) { status = VM_ERR_INVALID_PC; break; }
            vm->stack_frames[vm->sp + 1].return_addr = next_pc;
            vm->sp++;
            for (uint32_t i = 0; i < STACK_LOCALS_COUNT; i++) {
                vm->stack_frames[vm->sp].locals[i].type = V_VOID;
                vm->stack_frames[vm->sp].locals[i].val.u32 = 0;
            }
            next_pc = imm1.u32;
            break;
        case OP_RET:
            if (vm->sp == 0) { status = VM_ERR_STACK_UNDERFLOW; break; }
            next_pc = vm->stack_frames[vm->sp].return_addr;
            vm->sp--;
            break;
            
        /* Load Operations */
        case OP_LOAD_G: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_global_var(vm, imm1.u32);
            if (!dest || !src) { status = VM_ERR_INVALID_GLOBAL_IDX; break; }
            *dest = *src;
            break;
        }
        case OP_LOAD_L: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_local_var(vm, imm1.u32);
            if (!dest || !src) { status = VM_ERR_INVALID_LOCAL_IDX; break; }
            *dest = *src;
            break;
        }
        case OP_LOAD_S: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (imm1.stack_var_ref.frame_idx >= STACK_DEPTH || imm1.stack_var_ref.var_idx >= STACK_VAR_COUNT) {
                status = VM_ERR_INVALID_STACK_VAR_IDX; break;
            }
            *dest = vm->stack_frames[imm1.stack_var_ref.frame_idx].stack_vars[imm1.stack_var_ref.var_idx];
            break;
        }
        case OP_LOAD_I_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            dest->type = V_I32;
            dest->val.i32 = imm1.i32;
            break;
        }
        case OP_LOAD_I_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            dest->type = V_U32;
            dest->val.u32 = imm1.u32;
            break;
        }
        case OP_LOAD_I_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = imm1.f32;
            break;
        }
        case OP_LOAD_RET: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (imm1.u32 >= STACK_DEPTH) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            *dest = vm->stack_frames[imm1.u32].ret_val;
            break;
        }
        
        /* Store Operations */
        case OP_STORE_G: {
            var_value_t* src = get_stack_var(vm, hdr.operand);
            var_value_t* dest = get_global_var(vm, imm1.u32);
            if (!src || !dest) { status = VM_ERR_INVALID_GLOBAL_IDX; break; }
            *dest = *src;
            break;
        }
        case OP_STORE_L: {
            var_value_t* src = get_stack_var(vm, hdr.operand);
            var_value_t* dest = get_local_var(vm, imm1.u32);
            if (!src || !dest) { status = VM_ERR_INVALID_LOCAL_IDX; break; }
            *dest = *src;
            break;
        }
        case OP_STORE_S: {
            var_value_t* src = get_stack_var(vm, hdr.operand);
            if (!src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (imm1.stack_var_ref.frame_idx >= STACK_DEPTH || imm1.stack_var_ref.var_idx >= STACK_VAR_COUNT) {
                status = VM_ERR_INVALID_STACK_VAR_IDX; break;
            }
            vm->stack_frames[imm1.stack_var_ref.frame_idx].stack_vars[imm1.stack_var_ref.var_idx] = *src;
            break;
        }
        case OP_STORE_RET: {
            var_value_t* src = get_stack_var(vm, hdr.operand);
            if (!src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (imm1.u32 >= STACK_DEPTH) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            vm->stack_frames[imm1.u32].ret_val = *src;
            break;
        }
        
        /* Signed Integer Arithmetic */
        case OP_ADD_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_I32 || src2->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_I32;
            dest->val.i32 = src1->val.i32 + src2->val.i32;
            break;
        }
        case OP_SUB_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_I32 || src2->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_I32;
            dest->val.i32 = src1->val.i32 - src2->val.i32;
            break;
        }
        case OP_MUL_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_I32 || src2->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_I32;
            dest->val.i32 = src1->val.i32 * src2->val.i32;
            break;
        }
        case OP_DIV_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_I32 || src2->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (src2->val.i32 == 0) { status = VM_ERR_DIV_BY_ZERO; break; }
            dest->type = V_I32;
            dest->val.i32 = src1->val.i32 / src2->val.i32;
            break;
        }
        case OP_MOD_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_I32 || src2->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (src2->val.i32 == 0) { status = VM_ERR_DIV_BY_ZERO; break; }
            dest->type = V_I32;
            dest->val.i32 = src1->val.i32 % src2->val.i32;
            break;
        }
        case OP_NEG_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_I32;
            dest->val.i32 = -src->val.i32;
            break;
        }
        
        /* Unsigned Integer Arithmetic */
        case OP_ADD_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 + src2->val.u32;
            break;
        }
        case OP_SUB_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 - src2->val.u32;
            break;
        }
        case OP_MUL_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 * src2->val.u32;
            break;
        }
        case OP_DIV_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (src2->val.u32 == 0) { status = VM_ERR_DIV_BY_ZERO; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 / src2->val.u32;
            break;
        }
        case OP_MOD_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (src2->val.u32 == 0) { status = VM_ERR_DIV_BY_ZERO; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 % src2->val.u32;
            break;
        }
        
        /* Float Arithmetic */
        case OP_ADD_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_FLOAT || src2->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = src1->val.f32 + src2->val.f32;
            break;
        }
        case OP_SUB_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_FLOAT || src2->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = src1->val.f32 - src2->val.f32;
            break;
        }
        case OP_MUL_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_FLOAT || src2->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = src1->val.f32 * src2->val.f32;
            break;
        }
        case OP_DIV_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_FLOAT || src2->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (src2->val.f32 == 0.0f) { status = VM_ERR_DIV_BY_ZERO; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = src1->val.f32 / src2->val.f32;
            break;
        }
        case OP_NEG_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = -src->val.f32;
            break;
        }
        case OP_ABS_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = fabsf(src->val.f32);
            break;
        }
        case OP_SQRT_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = sqrtf(src->val.f32);
            break;
        }
        
        /* Bitwise Operations (Unsigned Only - MISRA-C) */
        case OP_AND_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 & src2->val.u32;
            break;
        }
        case OP_OR_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 | src2->val.u32;
            break;
        }
        case OP_XOR_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 ^ src2->val.u32;
            break;
        }
        case OP_NOT_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_U32;
            dest->val.u32 = ~src->val.u32;
            break;
        }
        case OP_SHL_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (src2->val.u32 >= 32) { status = VM_ERR_BOUNDS; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 << src2->val.u32;
            break;
        }
        case OP_SHR_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!dest || !src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (src2->val.u32 >= 32) { status = VM_ERR_BOUNDS; break; }
            dest->type = V_U32;
            dest->val.u32 = src1->val.u32 >> src2->val.u32;
            break;
        }
        
        /* Comparison Operations */
        case OP_CMP_I32: {
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_I32 || src2->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            vm->flags = 0;
            if (src1->val.i32 == src2->val.i32) vm->flags |= FLAG_ZERO;
            if (src1->val.i32 < src2->val.i32) vm->flags |= FLAG_LESS;
            if (src1->val.i32 > src2->val.i32) vm->flags |= FLAG_GREATER;
            break;
        }
        case OP_CMP_U32: {
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_U32 || src2->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            vm->flags = 0;
            if (src1->val.u32 == src2->val.u32) vm->flags |= FLAG_ZERO;
            if (src1->val.u32 < src2->val.u32) vm->flags |= FLAG_LESS;
            if (src1->val.u32 > src2->val.u32) vm->flags |= FLAG_GREATER;
            break;
        }
        case OP_CMP_F32: {
            var_value_t* src1 = get_stack_var(vm, imm1.u32 & 0xFF);
            var_value_t* src2 = get_stack_var(vm, imm2.u32 & 0xFF);
            if (!src1 || !src2) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src1->type != V_FLOAT || src2->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            vm->flags = 0;
            /* Use epsilon comparison for float equality to handle precision issues.
             * Tolerance of 1e-6f provides reasonable precision for 32-bit floats
             * while avoiding false inequalities from rounding errors. */
            if (fabsf(src1->val.f32 - src2->val.f32) < 1e-6f) vm->flags |= FLAG_ZERO;
            if (src1->val.f32 < src2->val.f32) vm->flags |= FLAG_LESS;
            if (src1->val.f32 > src2->val.f32) vm->flags |= FLAG_GREATER;
            break;
        }
        
        /* Type Conversions */
        case OP_I32_TO_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_U32;
            dest->val.u32 = (uint32_t)src->val.i32;
            break;
        }
        case OP_U32_TO_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_I32;
            dest->val.i32 = (int32_t)src->val.u32;
            break;
        }
        case OP_I32_TO_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = (float)src->val.i32;
            break;
        }
        case OP_U32_TO_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_FLOAT;
            dest->val.f32 = (float)src->val.u32;
            break;
        }
        case OP_F32_TO_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_I32;
            dest->val.i32 = (int32_t)src->val.f32;
            break;
        }
        case OP_F32_TO_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!dest || !src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            dest->type = V_U32;
            dest->val.u32 = (uint32_t)src->val.f32;
            break;
        }
        
        /* I/O Operations */
        case OP_PRINT_I32: {
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
            print_i32(src->val.i32);
            break;
        }
        case OP_PRINT_U32: {
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            print_u32(src->val.u32);
            break;
        }
        case OP_PRINT_F32: {
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            print_f32(src->val.f32);
            break;
        }
        case OP_PRINTLN:
            (void)fputc('\n', stdout);
            break;
        
        /* Buffer Operations */
        case OP_BUF_READ: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            
            uint32_t buf_idx = imm1.u32;
            uint32_t pos = imm2.u32;
            
            if (!validate_buffer_idx(buf_idx)) { status = VM_ERR_INVALID_BUFFER_IDX; break; }
            
            membuf_t* buf = &vm->g_membuf[buf_idx];
            if (buf->type == MB_VOID) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (!validate_buffer_pos(buf->type, pos)) { status = VM_ERR_INVALID_BUFFER_POS; break; }
            
            switch (buf->type) {
                case MB_U8:
                    dest->type = V_U32;
                    dest->val.u32 = (uint32_t)buf->buf.u8x256[pos];
                    break;
                case MB_U16:
                    dest->type = V_U32;
                    dest->val.u32 = (uint32_t)buf->buf.u16x128[pos];
                    break;
                case MB_I32:
                    dest->type = V_I32;
                    dest->val.i32 = buf->buf.i32x64[pos];
                    break;
                case MB_U32:
                    dest->type = V_U32;
                    dest->val.u32 = buf->buf.u32x64[pos];
                    break;
                case MB_FLOAT:
                    dest->type = V_FLOAT;
                    dest->val.f32 = buf->buf.f32x64[pos];
                    break;
                default:
                    status = VM_ERR_TYPE_MISMATCH;
                    break;
            }
            break;
        }
        
        case OP_BUF_WRITE: {
            var_value_t* src = get_stack_var(vm, hdr.operand);
            if (!src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            
            uint32_t buf_idx = imm1.u32;
            uint32_t pos = imm2.u32;
            
            if (!validate_buffer_idx(buf_idx)) { status = VM_ERR_INVALID_BUFFER_IDX; break; }
            
            membuf_t* buf = &vm->g_membuf[buf_idx];
            if (buf->type == MB_VOID) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (!validate_buffer_pos(buf->type, pos)) { status = VM_ERR_INVALID_BUFFER_POS; break; }
            
            switch (buf->type) {
                case MB_U8:
                    if (src->type != V_U32 && src->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
                    if (src->type == V_U32) {
                        buf->buf.u8x256[pos] = (uint8_t)src->val.u32;
                    } else {
                        buf->buf.u8x256[pos] = (uint8_t)src->val.i32;
                    }
                    break;
                case MB_U16:
                    if (src->type != V_U32 && src->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
                    if (src->type == V_U32) {
                        buf->buf.u16x128[pos] = (uint16_t)src->val.u32;
                    } else {
                        buf->buf.u16x128[pos] = (uint16_t)src->val.i32;
                    }
                    break;
                case MB_I32:
                    if (src->type != V_I32) { status = VM_ERR_TYPE_MISMATCH; break; }
                    buf->buf.i32x64[pos] = src->val.i32;
                    break;
                case MB_U32:
                    if (src->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
                    buf->buf.u32x64[pos] = src->val.u32;
                    break;
                case MB_FLOAT:
                    if (src->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
                    buf->buf.f32x64[pos] = src->val.f32;
                    break;
                default:
                    status = VM_ERR_TYPE_MISMATCH;
                    break;
            }
            break;
        }
        
        case OP_BUF_LEN: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            
            uint32_t buf_idx = imm1.u32;
            if (!validate_buffer_idx(buf_idx)) { status = VM_ERR_INVALID_BUFFER_IDX; break; }
            
            membuf_t* buf = &vm->g_membuf[buf_idx];
            dest->type = V_U32;
            dest->val.u32 = get_buffer_capacity(buf->type);
            break;
        }
        
        case OP_BUF_CLEAR: {
            uint32_t buf_idx = imm1.u32;
            if (!validate_buffer_idx(buf_idx)) { status = VM_ERR_INVALID_BUFFER_IDX; break; }
            
            membuf_t* buf = &vm->g_membuf[buf_idx];
            memset(&buf->buf, 0, sizeof(buf->buf));
            break;
        }
        
        /* String Operations */
        case OP_STR_CAT: {
            uint32_t dest_idx = hdr.operand;
            uint32_t src1_idx = imm1.u32;
            uint32_t src2_idx = imm2.u32;
            
            if (!validate_buffer_idx(dest_idx) || !validate_buffer_idx(src1_idx) || !validate_buffer_idx(src2_idx)) {
                status = VM_ERR_INVALID_BUFFER_IDX; break;
            }
            
            membuf_t* dest_buf = &vm->g_membuf[dest_idx];
            membuf_t* src1_buf = &vm->g_membuf[src1_idx];
            membuf_t* src2_buf = &vm->g_membuf[src2_idx];
            
            if (src1_buf->type != MB_U8 || src2_buf->type != MB_U8) {
                status = VM_ERR_TYPE_MISMATCH; break;
            }
            
            dest_buf->type = MB_U8;
            
            /* Find lengths of source strings */
            uint32_t len1 = 0;
            while (len1 < MEMBUF_U8_COUNT && src1_buf->buf.u8x256[len1] != 0) {
                len1++;
            }
            
            uint32_t len2 = 0;
            while (len2 < MEMBUF_U8_COUNT && src2_buf->buf.u8x256[len2] != 0) {
                len2++;
            }
            
            /* Copy first string */
            uint32_t i;
            for (i = 0; i < len1 && i < MEMBUF_U8_COUNT - 1; i++) {
                dest_buf->buf.u8x256[i] = src1_buf->buf.u8x256[i];
            }
            
            /* Append second string */
            for (uint32_t j = 0; j < len2 && i < MEMBUF_U8_COUNT - 1; j++, i++) {
                dest_buf->buf.u8x256[i] = src2_buf->buf.u8x256[j];
            }
            
            /* Null terminate */
            dest_buf->buf.u8x256[i] = 0;
            break;
        }
        
        case OP_STR_COPY: {
            uint32_t dest_idx = hdr.operand;
            uint32_t src_idx = imm1.u32;
            
            if (!validate_buffer_idx(dest_idx) || !validate_buffer_idx(src_idx)) {
                status = VM_ERR_INVALID_BUFFER_IDX; break;
            }
            
            membuf_t* dest_buf = &vm->g_membuf[dest_idx];
            membuf_t* src_buf = &vm->g_membuf[src_idx];
            
            if (src_buf->type != MB_U8) {
                status = VM_ERR_TYPE_MISMATCH; break;
            }
            
            dest_buf->type = MB_U8;
            
            /* Copy string with null terminator */
            uint32_t i;
            for (i = 0; i < MEMBUF_U8_COUNT; i++) {
                dest_buf->buf.u8x256[i] = src_buf->buf.u8x256[i];
                if (src_buf->buf.u8x256[i] == 0) {
                    break;
                }
            }
            
            /* Ensure null termination */
            if (i == MEMBUF_U8_COUNT) {
                dest_buf->buf.u8x256[MEMBUF_U8_COUNT - 1] = 0;
            }
            break;
        }
        
        case OP_STR_LEN: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            
            uint32_t buf_idx = imm1.u32;
            if (!validate_buffer_idx(buf_idx)) { status = VM_ERR_INVALID_BUFFER_IDX; break; }
            
            membuf_t* buf = &vm->g_membuf[buf_idx];
            if (buf->type != MB_U8) { status = VM_ERR_TYPE_MISMATCH; break; }
            
            /* Find string length */
            uint32_t len = 0;
            while (len < MEMBUF_U8_COUNT && buf->buf.u8x256[len] != 0) {
                len++;
            }
            
            dest->type = V_U32;
            dest->val.u32 = len;
            break;
        }
        
        case OP_STR_CMP: {
            uint32_t buf1_idx = imm1.u32;
            uint32_t buf2_idx = imm2.u32;
            
            if (!validate_buffer_idx(buf1_idx) || !validate_buffer_idx(buf2_idx)) {
                status = VM_ERR_INVALID_BUFFER_IDX; break;
            }
            
            membuf_t* buf1 = &vm->g_membuf[buf1_idx];
            membuf_t* buf2 = &vm->g_membuf[buf2_idx];
            
            if (buf1->type != MB_U8 || buf2->type != MB_U8) {
                status = VM_ERR_TYPE_MISMATCH; break;
            }
            
            /* Compare strings byte by byte */
            vm->flags = 0;
            int32_t cmp_result = 0;
            
            for (uint32_t i = 0; i < MEMBUF_U8_COUNT; i++) {
                uint8_t c1 = buf1->buf.u8x256[i];
                uint8_t c2 = buf2->buf.u8x256[i];
                
                if (c1 < c2) {
                    cmp_result = -1;
                    break;
                } else if (c1 > c2) {
                    cmp_result = 1;
                    break;
                } else if (c1 == 0) {
                    /* Both strings ended at same position */
                    break;
                }
            }
            
            if (cmp_result == 0) vm->flags |= FLAG_ZERO;
            if (cmp_result < 0) vm->flags |= FLAG_LESS;
            if (cmp_result > 0) vm->flags |= FLAG_GREATER;
            break;
        }
        
        case OP_STR_CHR: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            
            uint32_t buf_idx = imm1.u32;
            uint32_t pos = imm2.u32;
            
            if (!validate_buffer_idx(buf_idx)) { status = VM_ERR_INVALID_BUFFER_IDX; break; }
            
            membuf_t* buf = &vm->g_membuf[buf_idx];
            if (buf->type != MB_U8) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (pos >= MEMBUF_U8_COUNT) { status = VM_ERR_INVALID_BUFFER_POS; break; }
            
            dest->type = V_U32;
            dest->val.u32 = (uint32_t)buf->buf.u8x256[pos];
            break;
        }
        
        case OP_STR_SET_CHR: {
            uint32_t buf_idx = imm1.u32;
            uint32_t pos = imm2.u32;
            uint32_t chr_val = imm3.u32;
            
            if (!validate_buffer_idx(buf_idx)) { status = VM_ERR_INVALID_BUFFER_IDX; break; }
            
            membuf_t* buf = &vm->g_membuf[buf_idx];
            if (buf->type != MB_U8) { status = VM_ERR_TYPE_MISMATCH; break; }
            if (pos >= MEMBUF_U8_COUNT) { status = VM_ERR_INVALID_BUFFER_POS; break; }
            
            buf->buf.u8x256[pos] = (uint8_t)(chr_val & 0xFFu);
            break;
        }
        
        /* I/O Operations */
        case OP_PRINT_STR: {
            uint32_t buf_idx = imm1.u32;
            if (!validate_buffer_idx(buf_idx)) { status = VM_ERR_INVALID_BUFFER_IDX; break; }
            
            membuf_t* buf = &vm->g_membuf[buf_idx];
            if (buf->type != MB_U8) { status = VM_ERR_TYPE_MISMATCH; break; }
            
            /* Print string up to null terminator */
            for (uint32_t i = 0; i < MEMBUF_U8_COUNT; i++) {
                if (buf->buf.u8x256[i] == 0) {
                    break;
                }
                (void)fputc((char)buf->buf.u8x256[i], stdout);
            }
            break;
        }
        
        case OP_READ_I32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            
            /* Safe: scanf with %d reads into fixed-size int32_t variable, no buffer overflow risk */
            int32_t value;
            if (scanf("%d", &value) == 1) {
                dest->type = V_I32;
                dest->val.i32 = value;
            } else {
                /* On read failure, set to 0 */
                dest->type = V_I32;
                dest->val.i32 = 0;
                /* Clear input buffer */
                int c;
                while ((c = getchar()) != '\n' && c != EOF) {}
            }
            break;
        }
        
        case OP_READ_U32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            
            /* Safe: scanf with %u reads into fixed-size uint32_t variable, no buffer overflow risk */
            uint32_t value;
            if (scanf("%u", &value) == 1) {
                dest->type = V_U32;
                dest->val.u32 = value;
            } else {
                /* On read failure, set to 0 */
                dest->type = V_U32;
                dest->val.u32 = 0;
                /* Clear input buffer */
                int c;
                while ((c = getchar()) != '\n' && c != EOF) {}
            }
            break;
        }
        
        case OP_READ_F32: {
            var_value_t* dest = get_stack_var(vm, hdr.operand);
            if (!dest) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            
            /* Safe: scanf with %f reads into fixed-size float variable, no buffer overflow risk */
            float value;
            if (scanf("%f", &value) == 1) {
                dest->type = V_FLOAT;
                dest->val.f32 = value;
            } else {
                /* On read failure, set to 0.0 */
                dest->type = V_FLOAT;
                dest->val.f32 = 0.0f;
                /* Clear input buffer */
                int c;
                while ((c = getchar()) != '\n' && c != EOF) {}
            }
            break;
        }
        
        case OP_READ_STR: {
            uint32_t buf_idx = imm1.u32;
            if (!validate_buffer_idx(buf_idx)) { status = VM_ERR_INVALID_BUFFER_IDX; break; }
            
            membuf_t* buf = &vm->g_membuf[buf_idx];
            buf->type = MB_U8;
            
            /* Read string from stdin up to newline or max length */
            uint32_t i = 0;
            int c;
            while (i < MEMBUF_U8_COUNT - 1) {
                c = getchar();
                if (c == EOF || c == '\n') {
                    break;
                }
                buf->buf.u8x256[i] = (uint8_t)c;
                i++;
            }
            
            /* Null terminate */
            buf->buf.u8x256[i] = 0;
            break;
        }
        
        default:
            status = VM_ERR_INVALID_OPCODE;
            break;
    }
    
    if (status == VM_OK) {
        vm->pc = next_pc;
    }
    
    vm->last_error = status;
    return status;
}

vm_status_t vm_run(vm_state_t* vm) {
    vm_status_t status;
    while ((status = vm_step(vm)) == VM_OK) {}
    return (status == VM_ERR_HALT) ? VM_OK : status;
}

void vm_disassemble_instruction(const vm_state_t* vm, uint32_t pc) {
    if (pc >= vm->program_len || vm->program_len - pc < 4) {
        print_hex16((uint16_t)pc);
        (void)fputs(": <invalid>\n", stdout);
        return;
    }
    
    instruction_header_t hdr;
    memcpy(&hdr, &vm->program[pc], 4);
    print_hex16((uint16_t)pc);
    (void)fputs(": ", stdout);
    (void)fputs(opcode_to_string(hdr.opcode), stdout);
    (void)fputc('\n', stdout);
}

void vm_dump_state(const vm_state_t* vm) {
    (void)fputs("=== VM State ===\n", stdout);
    (void)fputs("PC: ", stdout);
    print_hex16((uint16_t)vm->pc);
    (void)fputs("  SP: ", stdout);
    print_u32(vm->sp);
    (void)fputs("  Flags: ", stdout);
    print_hex8(vm->flags);
    (void)fputc('\n', stdout);
    
    (void)fputs("Last Error: ", stdout);
    (void)fputs(vm_get_error_string(vm->last_error), stdout);
    (void)fputc('\n', stdout);
    
    (void)fputs("\nStack Frame ", stdout);
    print_u32(vm->sp);
    (void)fputs(":\n", stdout);
    for (uint32_t i = 0; i < STACK_VAR_COUNT; i++) {
        var_value_t* v = (var_value_t*)&vm->stack_frames[vm->sp].stack_vars[i];
        if (v->type != V_VOID) {
            (void)fputs("  s", stdout);
            print_u32(i);
            (void)fputs(": ", stdout);
            (void)fputs(var_type_to_string(v->type), stdout);
            (void)fputs(" = ", stdout);
            if (v->type == V_I32) {
                print_i32(v->val.i32);
            } else if (v->type == V_U32) {
                print_u32(v->val.u32);
            } else if (v->type == V_FLOAT) {
                print_f32(v->val.f32);
            }
            (void)fputc('\n', stdout);
        }
    }
}
