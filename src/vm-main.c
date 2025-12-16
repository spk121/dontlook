/*
 * Stipple VM - Command Line Interface
 * Loads and executes bytecode files
 * MISRA-C Compliant - No dynamic allocation
 */

#include "stipple.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* Static buffer for loading programs - no dynamic allocation */
static uint8_t g_program_buffer[PROGRAM_MAX_SIZE];

static void print_usage(const char* progname) {
    (void)fputs("Usage: ", stdout);
    (void)fputs(progname, stdout);
    (void)fputs(" <bytecode_file>\n", stdout);
    (void)fputs("\nLoads and executes Stipple VM bytecode.\n", stdout);
}

static bool load_file(const char* filename, uint8_t* buffer, uint32_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        (void)fputs("Error: Cannot open file '", stderr);
        (void)fputs(filename, stderr);
        (void)fputs("'\n", stderr);
        return false;
    }
    
    /* Read file in chunks using static buffer */
    const size_t CHUNK_SIZE = 4096;
    size_t total_read = 0;

    while (total_read < PROGRAM_MAX_SIZE) {
        size_t to_read = PROGRAM_MAX_SIZE - total_read;
        if (to_read > CHUNK_SIZE) {
            to_read = CHUNK_SIZE;
        }
        
        errno = 0;
        size_t n = fread(buffer + total_read, 1, to_read, f);
        
        if (n == 0) {
            if (feof(f) != 0) {
                break;
            } else {
                (void)fputs("Error: Failed to read file\n", stderr);
                (void)fclose(f);
                return false;
            }
        }
        
        total_read += n;
        
        /* Check if there's more data beyond max size */
        if (total_read == PROGRAM_MAX_SIZE) {
            int c = fgetc(f);
            if (c != EOF) {
                (void)fputs("Error: File too large\n", stderr);
                (void)fclose(f);
                return false;
            }
            break;
        }
    }
    
    errno = 0;
    if (fclose(f) != 0) {
        (void)fputs("Error: Failed to close file\n", stderr);
        return false;
    }

    if (total_read == 0u) {
        (void)fputs("Error: File is empty\n", stderr);
        return false;
    }

    *size = (uint32_t)total_read;
    return true;
}

static void print_uint32(uint32_t value) {
    char buf[12];  /* Enough for 4294967295 + null */
    int i = 0;
    
    if (value == 0u) {
        (void)fputc('0', stdout);
        return;
    }
    
    /* Convert to string in reverse */
    while (value > 0u) {
        buf[i] = (char)('0' + (value % 10u));
        value /= 10u;
        i++;
    }
    
    /* Print in correct order */
    while (i > 0) {
        i--;
        (void)fputc(buf[i], stdout);
    }
}

static void print_hex16_err(uint16_t value) {
    const char hex[] = "0123456789ABCDEF";
    (void)fputc('0', stderr);
    (void)fputc('x', stderr);
    (void)fputc(hex[(value >> 12) & 0xFu], stderr);
    (void)fputc(hex[(value >> 8) & 0xFu], stderr);
    (void)fputc(hex[(value >> 4) & 0xFu], stderr);
    (void)fputc(hex[value & 0xFu], stderr);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Load bytecode into static buffer */
    uint32_t program_size;
    if (!load_file(argv[1], g_program_buffer, &program_size)) {
        return 1;
    }
    
    (void)fputs("Loaded ", stdout);
    print_uint32(program_size);
    (void)fputs(" bytes from '", stdout);
    (void)fputs(argv[1], stdout);
    (void)fputs("'\n", stdout);
    
    /* Initialize VM */
    vm_state_t vm;
    vm_init(&vm);
    
    /* Load program */
    vm_status_t status = vm_load_program(&vm, g_program_buffer, program_size);
    if (status != VM_OK) {
        (void)fputs("Error loading program: ", stderr);
        (void)fputs(vm_get_error_string(status), stderr);
        (void)fputs("\n", stderr);
        return 1;
    }
    
    /* Execute */
    (void)fputs("Executing...\n", stdout);
    status = vm_run(&vm);
    
    /* Report results */
    if (status == VM_OK) {
        (void)fputs("\nProgram completed successfully.\n", stdout);
    } else {
        (void)fputs("\nProgram error at PC=", stderr);
        print_hex16_err((uint16_t)vm.pc);
        (void)fputs(": ", stderr);
        (void)fputs(vm_get_error_string(status), stderr);
        (void)fputs("\n", stderr);
        vm_dump_state(&vm);
    }
    
    return (status == VM_OK) ? 0 : 1;
}
