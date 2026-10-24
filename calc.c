// Name: Amanda Laima Lastname: Bramberga StudentID: 251RDC071
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: <every AI tool, website and book you used; details in comments where you used them>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

char *read_file(const char *path, size_t *out_len) {
    FILE *in = fopen(path, "rb");
    if (in == NULL) {
        return NULL;
    }

    if (fseek(in, 0, SEEK_END) != 0) {
        fclose(in);
        return NULL;
    }
    
    long size = ftell(in);
    if (size < 0) {
        fclose(in);
        return NULL;
    }

    rewind(in);

    char *buffer = malloc(size + 1);
    if (buffer == NULL) {
        fclose(in);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, size, in);
    if (read_bytes != (size_t)size) {
        free(buffer);
        fclose(in);
        return NULL;
    }

    buffer[size] = '\0';
    *out_len = (size_t)size;
    fclose(in);
    
    return buffer;
}

int main(int argc, char *argv[]) {

    const char *out_dir = NULL;
    const char *input_path = NULL;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s -o <out_dir> <input_path>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-o") != 0) {
        fprintf(stderr, "Error: Expected '-o' flag as the first argument.\n");
        return 1;
    }

    out_dir = argv[2];
    input_path = argv[3];

    size_t len = 0;
    char *buffer = read_file(input_path, &len);
    if (buffer == NULL) {
        fprintf(stderr, "Error: Could not open or read file '%s'\n", input_path);
        return 1;
    }

    char *p = buffer;
    size_t line_num = 1;

    while (*p != '\0') {
        char *q = p;
        while (*q != '\n' && *q != '\0') {
            q++;
        }

        char *temp = p;
        while (temp < q && (*temp == ' ' || *temp == '\t')) {
            temp++;
        }
        
        if (temp < q) {
            size_t position = (size_t)(p - buffer + 1);
            printf("Line %zu starts at position %zu\n", line_num, position);
        }

        p = q;
        if (*p == '\n') {
            p++;
        }
        line_num++;
    }

    free(buffer);

    return 0;
}