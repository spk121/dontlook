/*
 * vm.h - MISRA-C compliant Virtual Machine data structures
 */

#ifndef VM_H
#define VM_H

#include <stdint.h>

/* Capacity constants */
#define STR_POOL_CAPACITY   256U
#define STR_LEN_MAX         256U
#define IMM_CAPACITY        256U
#define INTVAR_CAPACITY     256U
#define VARNAME_CAPACITY    256U
#define VARNAME_LEN_MAX     32U
#define COND_CAPACITY       256U

/* Variable type enumeration */
enum var_type {
    VAR_STRING = 0,
    VAR_INT = 1
};

/* Condition type enumeration */
enum cond_type {
    COND_CMP_STR = 0,
    COND_CMP_INT = 1
};

/* Comparison operator enumeration */
enum cond_op {
    COND_OP_EQ = 0,    /* Equal */
    COND_OP_NE = 1,    /* Not equal */
    COND_OP_LT = 2,    /* Less than */
    COND_OP_LE = 3,    /* Less than or equal */
    COND_OP_GT = 4,    /* Greater than */
    COND_OP_GE = 5,    /* Greater than or equal */
    COND_OP_GLOB = 6   /* Glob pattern match */
};

/* String pool structure */
struct string_pool {
    uint8_t buf[STR_POOL_CAPACITY][STR_LEN_MAX];  /* 8-byte aligned buffer */
    uint8_t len[STR_POOL_CAPACITY];                /* 0 = unused, 1 to STR_LEN_MAX is string length */
};

/* Variable name structure */
struct varname {
    uint8_t name[VARNAME_LEN_MAX];  /* Variable name */
    enum var_type type;              /* Variable type */
    int16_t idx;                     /* Index into either integer-valued variables or string pool */
};

/* Condition structure */
struct cond {
    uint8_t type;      /* COND_CMP_STR or COND_CMP_INT */
    uint8_t op;        /* Comparison operator */
    uint16_t arg1;     /* Index into strings / vars / immediates */
    uint16_t arg2;     /* Second argument or immediate value */
    uint8_t negate;    /* 1 = apply logical NOT */
    uint8_t padding;   /* Padding for alignment */
};

/* External declarations for global VM data structures */
extern struct string_pool g_string_pool;
extern int32_t g_imm[IMM_CAPACITY];
extern int32_t g_intvar[INTVAR_CAPACITY];
extern struct varname g_varnames[VARNAME_CAPACITY];
extern struct cond g_cond_table[COND_CAPACITY];
extern uint16_t g_cond_count;

/* VM initialization function */
void vm_init(void);

#endif /* VM_H */
