#!/usr/bin/env python3
"""
Expand vm.c with complete instruction set implementation
This script reads the current vm.c and adds all remaining instructions
"""

import os

# Read the current vm.c
with open('src/vm.c', 'r') as f:
    lines = f.readlines()

# Find the switch statement in vm_step
switch_start_idx = None
switch_end_idx = None

for i, line in enumerate(lines):
    if 'switch (hdr.opcode)' in line:
        switch_start_idx = i
    if switch_start_idx is not None and 'default:' in line:
        switch_end_idx = i
        break

if switch_start_idx is None or switch_end_idx is None:
    print("ERROR: Could not find switch statement")
    exit(1)

# Generate additional instruction handlers
# This will be inserted before the 'default:' case

additional_cases = """
        /* Additional Load/Store Operations */
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
        
        /* Additional Arithmetic - Unsigned */
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
        
        /* Print Operations */
        case OP_PRINT_U32: {
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_U32) { status = VM_ERR_TYPE_MISMATCH; break; }
            printf("%u", src->val.u32);
            break;
        }
        case OP_PRINT_F32: {
            var_value_t* src = get_stack_var(vm, imm1.u32 & 0xFF);
            if (!src) { status = VM_ERR_INVALID_STACK_VAR_IDX; break; }
            if (src->type != V_FLOAT) { status = VM_ERR_TYPE_MISMATCH; break; }
            printf("%f", src->val.f32);
            break;
        }
"""

# Insert the additional cases before the default case
lines.insert(switch_end_idx, additional_cases)

# Write the updated file
with open('src/vm.c', 'w') as f:
    f.writelines(lines)

print("Expanded vm.c with additional instructions")
print(f"Added instructions at line {switch_end_idx}")
