/*
 * vm.c - MISRA-C compliant Virtual Machine implementation
 */

#include "vm.h"
#include <string.h>

/* Global VM data structures */
struct string_pool g_string_pool;
int32_t g_imm[IMM_CAPACITY];
int32_t g_intvar[INTVAR_CAPACITY];
struct varname g_varnames[VARNAME_CAPACITY];
struct cond g_cond_table[COND_CAPACITY];
uint16_t g_cond_count = 0U;

/*
 * vm_init - Initialize all VM data structures to zero
 */
void vm_init(void)
{
    uint16_t i;
    
    /* Initialize string pool */
    (void)memset(&g_string_pool, 0, sizeof(g_string_pool));
    
    /* Initialize immediate values */
    (void)memset(g_imm, 0, sizeof(g_imm));
    
    /* Initialize integer variables */
    (void)memset(g_intvar, 0, sizeof(g_intvar));
    
    /* Initialize variable names */
    for (i = 0U; i < VARNAME_CAPACITY; i++) {
        (void)memset(&g_varnames[i], 0, sizeof(struct varname));
        g_varnames[i].type = VAR_INT;
        g_varnames[i].idx = -1;
    }
    
    /* Initialize condition table */
    for (i = 0U; i < COND_CAPACITY; i++) {
        (void)memset(&g_cond_table[i], 0, sizeof(struct cond));
    }
    
    /* Reset condition count */
    g_cond_count = 0U;
}
