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
    
    /* Read file in chunks for portability */
    const size_t CHUNK_SIZE = 4096;
    size_t capacity = CHUNK_SIZE;
    size_t total_read = 0;
    uint8_t* buffer = (uint8_t*)malloc(capacity);
    if (!buffer) {
        fprintf(stderr, "Error: Out of memory\n");
        fclose(f);
        return NULL;
    }

    while (1) {
        if (total_read == capacity) {
            if (capacity >= PROGRAM_MAX_SIZE) {
                fprintf(stderr, "Error: File too large\n");
                free(buffer);
                fclose(f);
                return NULL;
            }
            size_t new_capacity = capacity * 2;
            if (new_capacity > PROGRAM_MAX_SIZE) {
                new_capacity = PROGRAM_MAX_SIZE;
            }
            uint8_t* new_buffer = (uint8_t*)realloc(buffer, new_capacity);
            if (!new_buffer) {
                fprintf(stderr, "Error: Out of memory\n");
                free(buffer);
                fclose(f);
                return NULL;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        size_t to_read = capacity - total_read;
        if (to_read > CHUNK_SIZE) {
            to_read = CHUNK_SIZE;
        }
        size_t n = fread(buffer + total_read, 1, to_read, f);
        if (n == 0) {
            if (feof(f)) {
                break;
            } else {
                fprintf(stderr, "Error: Failed to read file\n");
                free(buffer);
                fclose(f);
                return NULL;
            }
        }
        if (total_read + n > PROGRAM_MAX_SIZE) {
            fprintf(stderr, "Error: File too large\n");
            free(buffer);
            fclose(f);
            return NULL;
        }
        total_read += n;
    }
    fclose(f);

    if (total_read == 0) {
        fprintf(stderr, "Error: File is empty\n");
        free(buffer);
        return NULL;
    }

    *size = (uint32_t)total_read;
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
