// Name: Amanda Laima Lastname: Bramberga StudentID: 251RDC071

// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: Claude (Anthropic), chat history link: https://claude.ai/share/1d7b117b-eb66-4587-aef5-b26c97065c32 ; details in comments where used.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include <dirent.h>

#define NAME "amanda"
#define LAST_NAME "bramberga"
#define STUDENT_ID "251rdc071"
#define USAGE "Usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n"

// Whole file in one buffer, so error positions are just offsets into it.
// AI (Claude): prompt "start with grade 4 on how to calculate integers with + and -".
//   I used its idea of reading the whole file at once: fopen "rb", fseek/ftell/rewind, malloc(size + 1), fread, '\0'.
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

// Grammar (from the lab handout hint):
//   expr    := term  { ('+' | '-') term }
//   term    := power { ('*' | '/') power }
//   power   := primary [ '**' power ]
//   primary := NUMBER | '(' expr ')'
// Each level calls the tighter one, so tighter operators are solved first.
// Positions: every function gets `buffer`, so position = pointer - buffer + 1
// (1-based). Newlines and comments count because the buffer is never cut up.
 
// primary and expr call each other, so C needs this declaration first.
// AI (Claude): prompt "can you guide me through the steps for garde 7".
//   I used its idea of a prototype for expr above primary.
int expr(const char **p, const char *buffer, long long *result, size_t *err_pos);

// AI (Claude): prompt "start with grade 4 on how to calculate integers with + and -".
//   I used its idea of skipping spaces and tabs before reading a token.
// AI (Claude): prompt "yes so i am ready to do those things for grade 5, guide me through how you already did it before".
//   I used its point that skip_blanks must skip only spaces and tabs, never '\n', so a missing operand at the end of a line
//   is not pushed onto the next line.
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

// Only primary decides "an operand must start here", so all missing-operand
// errors are caught in this one spot. Brackets bind tightest, so (...) is one operand.
// A missing ) is found here at the line break; an extra ) is left for eval_line.
// AI (Claude): prompt "yes so i am ready to do those things for grade 5, guide me through how you already did it before".
//   I used its idea of checking for a digit after skipping blanks and reporting the error at the current pointer.
// AI (Claude): prompt "how do i do it now for grade 6  which are * and / with precedence; division by zero".
//   I used its idea of a primary function that skips blanks, reports the error at the current position if there is no digit,
//   and then calls read_number.
// AI (Claude): prompt "can you guide me through the steps for garde 7".
//   I used its idea of handling '(' in primary: call expr inside the brackets, skip blanks before ')',
//   and check the inner result first so an error inside the brackets is reported where it happens.
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

// Squaring instead of a loop, so huge exponents take few steps.
// The last squaring is skipped because it's never used and could overflow.
// AI (Claude): prompt "guide me through and give the idea for ** (right-associative, negative-exponent error); # comment lines and blank lines".
//   I used its idea of exponentiation by squaring, and of not squaring the base after the last bit.
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

// power calls itself for the exponent, so 2 ** 3 ** 2 is 2 ** (3 ** 2): right-associative.
// AI (Claude): prompt "guide me through and give the idea for ** (right-associative, negative-exponent error); # comment lines and blank lines".
//   I used its idea of power calling itself for the exponent (right-associative), checking for two adjacent stars,
//   and saving the exponent's start pointer for the negative-exponent error.
int power(const char **p, const char *buffer, long long *result, size_t *err_pos) {
    long long base = 0;
    if (primary(p, buffer, &base, err_pos) != 0) {
        return 1;
    }

    skip_blanks(p);
    if (**p == '*') {
        // A single * is multiplication (term's job), so put it back.
        const char *first_star = *p;
        (*p)++;
        if (**p == '*') {
            (*p)++;
            
            skip_blanks(p);
            // Handout: a negative exponent is reported at the exponent's start, so save it before parsing.
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

// term calls power so ** binds tighter than * and /.
// Folding each operand right away gives left-to-right: 100 / 7 / 2 = (100 / 7) / 2.
// AI (Claude): prompt "how do i do it now for grade 6  which are * and / with precedence; division by zero".
//   I used its idea of a term function between expr and primary: a loop for * and /, with the pointer saved before the
//   right operand so division by zero is reported at its start.
// AI (Claude): prompt "guide me through and give the idea for ** (right-associative, negative-exponent error); # comment lines and blank lines".
//   I used its idea that term calls power instead of primary.
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

        // Handout: division by zero is reported at the divisor's start, so save it first.
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

// expr calls term so * and / are finished before + and -; same loop keeps them left-to-right.
// AI (Claude): prompt "start with grade 4 on how to calculate integers with + and -".
//   I used its idea of a left-to-right loop, so 2 + 5 - 3 is (2 + 5) - 3.
// AI (Claude): prompt "how do i do it now for grade 6  which are * and / with precedence; division by zero".
//   I used its idea of one function per precedence level, so expr calls term for its operands.
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

// The only leftover check: anything after expr (like an extra ')') is an error at that character.
// AI (Claude): prompt "yes so i am ready to do those things for grade 5, guide me through how you already did it before".
//   I used its idea of returning a status and passing the result and the error position back through pointers.
// AI (Claude): prompt "how do i do it now for grade 6  which are * and / with precedence; division by zero".
//   I used its idea of eval_line calling expr and then checking that the line really ended (the leftover check).
// AI (Claude): prompt "can you guide me through the steps for garde 7".
//   I used its explanation of why an extra ')' is caught by this leftover check.
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

// Strip trailing slashes for directories, or "labs/" would give an empty name.
// AI (Claude): prompt "is this correct?" (sent with a screenshot).
//   I used its idea of cutting off the folder part and ".txt" by length instead of copying the name.
//   I used its point about stripping trailing slashes in directory mode.
// AI (Claude): prompt "how do i do the grade 9, guide me through give an idea".
//   I used its idea of a helper that returns a pointer and a length into the original string, so nothing is copied.
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

// Returns 0 even for an ERROR result: that's a normal output. Non-zero is for I/O problems only.
// AI (Claude): prompt "start with grade 4 on how to calculate integers with + and -".
//   I used its ideas of walking one buffer with a pointer (no line copies), building the output name with snprintf,
//   and treating mkdir's EEXIST as fine.
// AI (Claude): prompt "yes so i am ready to do those things for grade 5, guide me through how you already did it before".
//   I used its ideas of a had_error flag that breaks out of the loop (so cleanup still runs) and, on an error, reopening
//   the output file with "w" to empty it and writing only ERROR:<pos>.
// AI (Claude): prompt "guide me through and give the idea for ** (right-associative, negative-exponent error); # comment lines and blank lines".
//   I used its idea of skipping a line whose first non-blank character is '#', so comment characters still count for positions.
// AI (Claude): prompt "how do i do the grade 9, guide me through give an idea".
//   I used its idea of moving the work for one input file into process_file, which returns 0 on success
//   (even for an ERROR: result) and non-zero for real failures.
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

        // Check the first non-blank char so an indented "  # ..." is still a comment.
        // Comment and blank lines skip evaluation but p still advances, so no infinite loop.
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

    // On error the file must hold only ERROR:<pos>, so reopen with "w" to wipe earlier results.
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

// S_ISREG: only regular files, so a folder named x.txt is ignored.
// One bad file sets rc = 1 but the rest still run; the exit code shows the failure.
// readdir order doesn't matter: each input has its own output file.
// AI (Claude): prompt "how do i do the grade 9, guide me through give an idea".
//   I used its idea for the directory loop: opendir/readdir/closedir, only names ending in .txt, the full path built with
//   snprintf (checked for truncation), stat + S_ISREG, and continuing after a failing file and returning non-zero at the end.
int process_dir(const char *dir_path, const char *out_dir) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        fprintf(stderr, "Error: Failed to open directory '%s': %s\n", dir_path, strerror(errno));
        return 1;
    }

    if (mkdir(out_dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error: Failed to create output directory '%s': %s\n", out_dir, strerror(errno));
        closedir(dir);
        return 1;
    }

    int rc = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t name_len = strlen(name);

        if (name_len <= 4 || strcmp(name + name_len - 4, ".txt") != 0) {
            continue;
        }

        char full_path[4096];
        int written = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, name);
        if (written < 0 || (size_t)written >= sizeof(full_path)) {
            fprintf(stderr, "Error: Path truncated for entry '%s'\n", name);
            rc = 1;
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        if (process_file(full_path, out_dir) != 0) {
            rc = 1;
        }
    }

    closedir(dir);
    return rc;
}

// AI (Claude): prompt "how do i do the grade 9, guide me through give an idea".
//   I used its idea of one loop over argv with three variables starting as NULL (-d folder, output folder, input file),
//   an unknown '-' option or a second input file being an error, validating after the loop (-d plus a file is a usage error),
//   and building the default folder name with snprintf.
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
            
            // The value is the next argument; check i + 1 < argc to avoid reading past argv.
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
            // A leading '-' means an option, not a file name, so unknown ones are errors.
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

    // Validate after the loop so options can come in any order.
    if ((dir_path == NULL && input_path == NULL) || (dir_path != NULL && input_path != NULL)) {
        fprintf(stderr, "%s", USAGE);
        return 1;
    }

    // No -o: the handout says the default folder is named after the input (the directory's name with -d).
    if (out_dir == NULL) {
        size_t base_len = 0;
        const char *base;
        if (dir_path != NULL) {
            base = get_basename(dir_path, &base_len, 1);
        } else {
            base = get_basename(input_path, &base_len, 0);
        }
        int written = snprintf(default_dir, sizeof(default_dir), "%.*s_%s_%s",
                               (int)base_len, base, NAME, STUDENT_ID);
        if (written < 0 || (size_t)written >= sizeof(default_dir)) {
            fprintf(stderr, "Error: Default output directory path truncated.\n");
            return 1;
        }
        out_dir = default_dir;
    }

    if (dir_path != NULL) {
        return process_dir(dir_path, out_dir);
    }

    return process_file(input_path, out_dir);
}