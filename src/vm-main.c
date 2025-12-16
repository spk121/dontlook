/*
 * Stipple VM - Command Line Interface
 * Loads and executes bytecode files
 */

#include "stipple.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char* progname) {
    printf("Usage: %s <bytecode_file>\n", progname);
    printf("\nLoads and executes Stipple VM bytecode.\n");
}

static uint8_t* load_file(const char* filename, uint32_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (fsize < 0 || fsize > PROGRAM_MAX_SIZE) {
        fprintf(stderr, "Error: Invalid file size\n");
        fclose(f);
        return NULL;
    }
    
    /* Allocate and read */
    uint8_t* buffer = (uint8_t*)malloc((size_t)fsize);
    if (!buffer) {
        fprintf(stderr, "Error: Out of memory\n");
        fclose(f);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, (size_t)fsize, f);
    fclose(f);
    
    if (read_size != (size_t)fsize) {
        fprintf(stderr, "Error: Failed to read file\n");
        free(buffer);
        return NULL;
    }
    
    *size = (uint32_t)fsize;
    return buffer;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Load bytecode */
    uint32_t program_size;
    uint8_t* program = load_file(argv[1], &program_size);
    if (!program) {
        return 1;
    }
    
    printf("Loaded %u bytes from '%s'\n", program_size, argv[1]);
    
    /* Initialize VM */
    vm_state_t vm;
    vm_init(&vm);
    
    /* Load program */
    vm_status_t status = vm_load_program(&vm, program, program_size);
    if (status != VM_OK) {
        fprintf(stderr, "Error loading program: %s\n", vm_get_error_string(status));
        free(program);
        return 1;
    }
    
    /* Execute */
    printf("Executing...\n");
    status = vm_run(&vm);
    
    /* Report results */
    if (status == VM_OK) {
        printf("\nProgram completed successfully.\n");
    } else {
        fprintf(stderr, "\nProgram error at PC=0x%04X: %s\n", 
                vm.pc, vm_get_error_string(status));
        vm_dump_state(&vm);
    }
    
    free(program);
    return (status == VM_OK) ? 0 : 1;
}
