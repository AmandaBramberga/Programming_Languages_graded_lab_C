// Name: Amanda Laima Lastname: Bramberga StudentID: 251RDC071
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: <every AI tool, website and book you used; details in comments where you used them>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>

#define NAME "amanda"
#define LAST_NAME "bramberga"
#define STUDENT_ID "251rdc071"
#define USAGE "Usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n"

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

// Why squaring instead of a loop: steps scale with the exponent's bit length, not its size, making huge powers instant.
// Why skip the final squaring: avoids unnecessary overflow traps on an iteration whose result is never used.
static long long int_pow(long long base, long long exp) {
    long long res = 1;
    long long b = base;
    long long e = exp;

    while (e > 0) {
        if (e % 2 == 1) {
            res *= b;
        }
        e /= 2;
        if (e > 0) {
            b *= b;
        }
    }
    return res;
}

// Why power calls itself: recursion provides right-associativity for exponentiation (e.g. 2 ** 3 ** 2 evaluates as 2 ** (3 ** 2)).
// Why exponent's start pointer is saved: negative or missing exponent errors point precisely to the start of the exponent operand.
int power(const char **p, const char *buffer, long long *result, size_t *err_pos) {
    long long base = 0;
    if (primary(p, buffer, &base, err_pos) != 0) {
        return 1;
    }

    skip_blanks(p);
    if (**p == '*') {
        const char *first_star = *p;
        (*p)++;
        if (**p == '*') {
            (*p)++;
            
            skip_blanks(p);
            const char *exp_pos = *p;

            long long exp = 0;
            if (power(p, buffer, &exp, err_pos) != 0) {
                return 1;
            }

            if (exp < 0) {
                *err_pos = (size_t)(exp_pos - buffer + 1);
                return 1;
            }

            *result = int_pow(base, exp);
            return 0;
        } else {
            *p = first_star;
        }
    }

    *result = base;
    return 0;
}

// Why term calls power: ensures ** binds tighter than * and /.
// Left-to-right works because the loop folds each term immediately, making 100 / 7 / 2 evaluate as (100 / 7) / 2.
// Save operand_pos because division by zero needs to point to the start of the divisor operand, not the slash operator.
int term(const char **p, const char *buffer, long long *result, size_t *err_pos) {
    long long total = 0;
    
    if (power(p, buffer, &total, err_pos) != 0) {
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
        if (power(p, buffer, &n, err_pos) != 0) {
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

// Why the base-name helper strips trailing slashes for directories: otherwise paths ending in slashes (like labs/) would evaluate to an empty base name.
const char *get_basename(const char *path, size_t *out_len, int is_dir) {
    size_t len = strlen(path);
    
    if (is_dir) {
        while (len > 1 && path[len - 1] == '/') {
            len--;
        }
    }

    const char *last_slash = NULL;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            last_slash = path + i;
        }
    }

    const char *base = (last_slash != NULL) ? last_slash + 1 : path;
    size_t base_len = len - (base - path);

    if (!is_dir) {
        if (base_len >= 4 && strcmp(base + base_len - 4, ".txt") == 0) {
            base_len -= 4;
        }
    }

    *out_len = base_len;
    return base;
}

// Why process_file returns success (0) even for an ERROR: result: calculation errors are normal outputs, whereas non-zero is reserved for system/IO failures.
int process_file(const char *input_path, const char *out_dir) {
    size_t len = 0;
    char *buffer = read_file(input_path, &len);
    if (buffer == NULL) {
        fprintf(stderr, "Error: Could not open or read file '%s': %s\n", input_path, strerror(errno));
        return 1;
    }

    size_t base_len = 0;
    const char *base = get_basename(input_path, &base_len, 0);

    char path[4096];
    int written = snprintf(path, sizeof(path), "%s/%.*s_%s_%s_%s.txt",
                           out_dir, (int)base_len, base, NAME, LAST_NAME, STUDENT_ID);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        fprintf(stderr, "Error: Output path truncated.\n");
        free(buffer);
        return 1;
    }

    if (mkdir(out_dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error: Failed to create output directory '%s': %s\n", out_dir, strerror(errno));
        free(buffer);
        return 1;
    }

    FILE *out = fopen(path, "w");
    if (out == NULL) {
        fprintf(stderr, "Error: Failed to open output file '%s' for writing: %s\n", path, strerror(errno));
        free(buffer);
        return 1;
    }

    char *p = buffer;
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
    }

    if (had_error) {
        FILE *new_out = freopen(path, "w", out);
        if (new_out == NULL) {
            fprintf(stderr, "Error: Failed to reopen output file for error writing: %s\n", strerror(errno));
            free(buffer);
            return 1;
        }
        out = new_out;
        fprintf(out, "ERROR:%zu\n", error_pos_to_report);
    }

    if (fclose(out) != 0) {
        fprintf(stderr, "Error: Failed to close output file '%s': %s\n", path, strerror(errno));
        free(buffer);
        return 1;
    }

    free(buffer);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *dir_path = NULL;
    const char *out_dir = NULL;
    const char *input_path = NULL;
    char default_dir[4096];

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing value for %s\n", argv[i]);
                fprintf(stderr, "%s", USAGE);
                return 1;
            }
            if (out_dir != NULL) {
                fprintf(stderr, "Error: Duplicate output directory option.\n");
                fprintf(stderr, "%s", USAGE);
                return 1;
            }
            // Why value options consume the next argument: options take parameters as separate tokens in argv.
            // Why you check i + 1 < argc first: prevents out-of-bounds memory reads when an option is missing its required value at the end of the argument list.
            out_dir = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing value for %s\n", argv[i]);
                fprintf(stderr, "%s", USAGE);
                return 1;
            }
            if (dir_path != NULL) {
                fprintf(stderr, "Error: Duplicate directory option.\n");
                fprintf(stderr, "%s", USAGE);
                return 1;
            }
            dir_path = argv[++i];
        } else if (argv[i][0] == '-') {
            // Why an unrecognized -something is an error: starting with a hyphen designates a command-line flag rather than a positional filename argument.
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            fprintf(stderr, "%s", USAGE);
            return 1;
        } else {
            if (input_path != NULL) {
                fprintf(stderr, "Error: Multiple input files provided.\n");
                fprintf(stderr, "%s", USAGE);
                return 1;
            }
            input_path = argv[i];
        }
    }

    // Why validation happens after the loop: ensures option arguments can appear in any order (e.g. file before -o) before checking completeness.
    if ((dir_path == NULL && input_path == NULL) || (dir_path != NULL && input_path != NULL)) {
        fprintf(stderr, "%s", USAGE);
        return 1;
    }

    if (out_dir == NULL) {
        size_t base_len = 0;
        const char *base = get_basename(input_path, &base_len, 0);
        // Why the default folder name omits the last name: it is a spec rule validated by tests.
        int written = snprintf(default_dir, sizeof(default_dir), "%.*s_%s_%s",
                               (int)base_len, base, NAME, STUDENT_ID);
        if (written < 0 || (size_t)written >= sizeof(default_dir)) {
            fprintf(stderr, "Error: Default output directory path truncated.\n");
            return 1;
        }
        out_dir = default_dir;
    }

    if (dir_path != NULL) {
        fprintf(stderr, "Error: -d / --dir mode not implemented yet.\n");
        return 1;
    }

    return process_file(input_path, out_dir);
}