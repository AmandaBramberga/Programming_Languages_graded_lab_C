// Name: Amanda Laima Lastname: Bramberga StudentID: 251RDC071
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: <every AI tool, website and book you used; details in comments where you used them>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#define NAME "amanda"
#define LAST_NAME "bramberga"
#define STUDENT_ID "251rdc071"

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

// Forward declaration needed because primary and expr call each other recursively.
int expr(const char **p, const char *buffer, long long *result, size_t *err_pos);

void skip_blanks(const char **p) {
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
}

long long read_number(const char **p) {
    long long value = 0;
    while (isdigit((unsigned char)**p)) {
        value = value * 10 + (**p - '0');
        (*p)++;
    }
    return value;
}

// primary is the only place that decides "an operand must start here", so every missing-operand error is detected in exactly one spot.
// Why in primary: parens bind tightest, making a bracketed block a single operand.
// Why expr/term stop quietly: they only look for their operators and ignore ')' naturally.
// Why missing ) hits newline: expr reads to the end, leaving primary stranded at the line break.
// Why extra ) is caught by leftover check: inner parsers ignore it, leaving eval_line to flag trailing junk.
int primary(const char **p, const char *buffer, long long *value, size_t *err_pos) {
    skip_blanks(p);

    if (**p == '(') {
        (*p)++;
        
        if (expr(p, buffer, value, err_pos) != 0) {
            return 1;
        }
        
        skip_blanks(p);
        
        if (**p == ')') {
            (*p)++;
        } else {
            *err_pos = (size_t)(*p - buffer + 1);
            return 1;
        }
        
        return 0;
    }

    if (!isdigit((unsigned char)**p)) {
        *err_pos = (size_t)(*p - buffer + 1);
        return 1;
    }
    *value = read_number(p);
    return 0;
}

// Term comes first because it binds * and / tighter than + and - by resolving them before expr.
// Left-to-right works because the loop folds each term immediately, making 100 / 7 / 2 evaluate as (100 / 7) / 2.
// Save operand_pos because division by zero needs to point to the start of the divisor operand, not the slash operator.
int term(const char **p, const char *buffer, long long *result, size_t *err_pos) {
    long long total = 0;
    
    if (primary(p, buffer, &total, err_pos) != 0) {
        return 1;
    }

    while (1) {
        skip_blanks(p);
        
        if (**p == '\n' || **p == '\r' || **p == '\0') {
            break;
        }

        char op = **p;
        if (op != '*' && op != '/') {
            break;
        }
        (*p)++;

        skip_blanks(p);
        const char *op_ptr = *p;

        long long n = 0;
        if (primary(p, buffer, &n, err_pos) != 0) {
            return 1;
        }

        if (op == '*') {
            total *= n;
        } else {
            if (n == 0) {
                *err_pos = (size_t)(op_ptr - buffer + 1);
                return 1;
            }
            total /= n;
        }
    }

    *result = total;
    return 0;
}

// expr evaluates operator chains, delegating operand parsing down to primary.
int expr(const char **p, const char *buffer, long long *result, size_t *err_pos) {
    long long total = 0;
    
    if (term(p, buffer, &total, err_pos) != 0) {
        return 1;
    }

    while (1) {
        skip_blanks(p);
        
        if (**p == '\n' || **p == '\r' || **p == '\0') {
            break;
        }

        char op = **p;
        if (op != '+' && op != '-') {
            break;
        }
        (*p)++;

        long long n = 0;
        if (term(p, buffer, &n, err_pos) != 0) {
            return 1;
        }

        if (op == '+') {
            total += n;
        } else {
            total -= n;
        }
    }

    *result = total;
    return 0;
}

// eval_line is the only place that checks for leftover input, which is what catches unexpected tokens.
int eval_line(const char *p, const char *buffer, long long *result, size_t *err_pos) {
    long long total = 0;
    
    if (expr(&p, buffer, &total, err_pos) != 0) {
        return 1;
    }

    skip_blanks(&p);
    if (*p != '\n' && *p != '\r' && *p != '\0') {
        *err_pos = (size_t)(p - buffer + 1);
        return 1;
    }

    *result = total;
    return 0;
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

    const char *base = strrchr(input_path, '/');
    if (base != NULL) {
        base++;
    } else {
        base = input_path;
    }

    size_t base_len = strlen(base);
    if (base_len >= 4 && strcmp(base + base_len - 4, ".txt") == 0) {
        base_len -= 4;
    }

    char path[4096];
    int written = snprintf(path, sizeof(path), "%s/%.*s_%s_%s_%s.txt",
                           out_dir, (int)base_len, base, NAME, LAST_NAME, STUDENT_ID);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        fprintf(stderr, "Error: Output path truncated.\n");
        free(buffer);
        return 1;
    }

    if (mkdir(out_dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error: Failed to create output directory.\n");
        free(buffer);
        return 1;
    }

    FILE *out = fopen(path, "w");
    if (out == NULL) {
        fprintf(stderr, "Error: Failed to open output file for writing.\n");
        free(buffer);
        return 1;
    }

    char *p = buffer;
    size_t line_num = 1;

    int had_error = 0;
    size_t error_pos_to_report = 0;

    while (*p != '\0') {
        char *q = p;
        while (*q != '\n' && *q != '\0') {
            q++;
        }

        char *temp = p;
        while (temp < q && (*temp == ' ' || *temp == '\t')) {
            temp++;
        }

        // Why check the first non-blank character: allows indented comments (e.g. "  # ...") to be correctly recognized as comments rather than syntax errors.
        // Why comment characters count toward positions with no special code: the buffer pointer indexes the entire file stream continuously, so character offsets naturally advance past comments.
        // Why comment lines take the same path as blank lines: ensures pointers and line counters always advance properly, preventing infinite loops or hangs.
        if (temp < q && *temp != '#') {
            long long line_result = 0;
            size_t err_pos = 0;

            if (eval_line(p, buffer, &line_result, &err_pos) == 0) {
                fprintf(out, "%lld\n", line_result);
            } else {
                had_error = 1;
                error_pos_to_report = err_pos;
                break;
            }
        }

        p = q;
        if (*p == '\n') {
            p++;
        }
        line_num++;
    }

    if (had_error) {
        out = freopen(path, "w", out);
        if (out != NULL) {
            fprintf(out, "ERROR:%zu\n", error_pos_to_report);
        }
    }

    fclose(out);
    free(buffer);
    
    return 0;
}