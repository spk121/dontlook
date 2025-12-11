/*
 * test_vm.c - Simple test program for VM initialization
 */

#include <stdio.h>
#include <assert.h>
#include "vm.h"

int main(void)
{
    uint16_t i;
    
    printf("Testing VM initialization...\n");
    
    /* Initialize the VM */
    vm_init();
    
    /* Verify string pool is zeroed */
    for (i = 0U; i < STR_POOL_CAPACITY; i++) {
        assert(g_string_pool.len[i] == 0U);
    }
    printf("✓ String pool initialized correctly\n");
    
    /* Verify immediate values are zeroed */
    for (i = 0U; i < IMM_CAPACITY; i++) {
        assert(g_imm[i] == 0);
    }
    printf("✓ Immediate values initialized correctly\n");
    
    /* Verify integer variables are zeroed */
    for (i = 0U; i < INTVAR_CAPACITY; i++) {
        assert(g_intvar[i] == 0);
    }
    printf("✓ Integer variables initialized correctly\n");
    
    /* Verify variable names are initialized */
    for (i = 0U; i < VARNAME_CAPACITY; i++) {
        assert(g_varnames[i].idx == -1);
    }
    printf("✓ Variable names initialized correctly\n");
    
    /* Verify condition table is zeroed */
    for (i = 0U; i < COND_CAPACITY; i++) {
        assert(g_cond_table[i].type == 0U);
        assert(g_cond_table[i].op == 0U);
    }
    assert(g_cond_count == 0U);
    printf("✓ Condition table initialized correctly\n");
    
    printf("\nAll tests passed!\n");
    
    return 0;
}
