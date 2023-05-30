// ctail.c
// Author: Michal Krulich

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <assert.h>

#define LINE_LENGTH_LIMIT 16383        // Maximum length of a single line
#define DEFAULT_LINES_BUFFER_SIZE 10  // Default number of last lines to be displayed
#define BUFFER_SIZE_LIMIT 4200000000u // Limit for total number of lines in a file

// Cyclic buffer that contains pointers to strings.
// The size of the cyclic buffer is always bigger by one than the maximum count of held objects.
// The buffer is full, when the end index stands right in front of the start index.
// The buffer is empty, when the start and end index are equal.
struct circ_bfr // Circular buffer
{
    unsigned size;  // Usable size of the buffer, how many objects can this buffer hold.
    unsigned start; // Number indexing the first object.
    unsigned end;   // Number indexing place where will be placed next object.
    char *lines[];  // Array containing the lines.
};

typedef struct circ_bfr cbfr_t;

// The real size of the cyclic buffer
#define cb_true_size(cbfr_pointer) ((cbfr_pointer)->size + 1)

// Increments the count of objects inside the cyclic buffer
// Moves the start index
#define cb_inc_start(cbfr)                                        \
    do                                                            \
    {                                                             \
        (cbfr)->start = ((cbfr)->start + 1) % cb_true_size(cbfr); \
    } while (0)

// Decrements the count of objects inside the cyclic buffer
// Moves the end index
#define cb_inc_end(cbfr)                                      \
    do                                                        \
    {                                                         \
        (cbfr)->end = ((cbfr)->end + 1) % cb_true_size(cbfr); \
    } while (0)

/*
 * Allocates the memory space for the cyclic buffer and initializes it.
 * Returns the pointer to the buffer or NULL in case of an error.
 */
cbfr_t *cb_create(unsigned n);

/*
 * Inserts the the dynamically allocated string into the buffer.
 * If the buffer is already full, the first line in the buffer is removed and deallocated.
 */
void cb_put(cbfr_t *cb, char *line);

/*
 * Returns pointer to the first line in the buffer and removes it from the buffer.
 * Returns NULL, if the buffer is empty.
 */
char *cb_get(cbfr_t *cb);

/*
 * Frees the cyclic buffer and its content from the memory.
 */
void cb_free(cbfr_t *cb);

/*
 * Prints out a string ended with '\0' or '\n' to stdout.
 */
void print_line(char *line);

/*
 * Safely closes a file.
 */
void safe_file_close(const char *name, FILE *file)
{
    if (name != NULL)
    {
        fclose(file);
    }
}

int main(int argc, char *argv[])
{
    unsigned buffer_size = DEFAULT_LINES_BUFFER_SIZE;
    char *file_to_be_openned = NULL; // Name of the file to be openned.
    bool process_switches = true;

    // Argument processing, similiar to shell command "shift" logic
    for (int next_argument = 1; next_argument < argc; next_argument++)
    {
        // help text
        if (strcmp(argv[next_argument], "--help") == 0 && process_switches)
        {
            printf("Usage: ctail [OPTION]... [FILE]...\n"
                   "Print the last 10 lines of each FILE to standard output.\n"
                   "With no FILE, or when FILE is -, read standard input.\n"
                   "Options:\n"
                   "    -n [X]      Prints the last X lines.\n"
                   "    --help      Prints out this help.\n"
                   "    -i          Ignores switches/options after this one.\n"
                   "                ctail -i --help   ...   reads file with name \"--help\"\n"
                   "This program tries to mimic the behaviour of the classic UNIX tail utillity");
            return EXIT_SUCCESS;
        }
        else if(strcmp(argv[next_argument], "-i") == 0 && process_switches)
        {
            process_switches = false;
        }
        // switch -n, controls the number of lines to be displayed
        else if (strcmp(argv[next_argument], "-n") == 0 && process_switches)
        {
            next_argument++;
            if (next_argument >= argc)
            {
                fprintf(stderr, "ERROR: Expected a number after -n\n");
                return EXIT_FAILURE;
            }
            if (sscanf(argv[next_argument], "%u", &buffer_size) != 1)
            {
                fprintf(stderr, "ERROR: Expected a number after -n\n");
                return EXIT_FAILURE;
            }
            if (buffer_size > BUFFER_SIZE_LIMIT)
            {
                fprintf(stderr, "ERROR: Number is too large or smaller than 0\n");
                return EXIT_FAILURE;
            }
            if (buffer_size == 0) // No line should be printed
            {
                return EXIT_SUCCESS;
            }
        }
        else
        {
            file_to_be_openned = argv[next_argument];
        }
    }

    FILE *input = stdin;
    if (file_to_be_openned != NULL)
    {
        input = fopen(file_to_be_openned, "r");
        if (input == NULL)
        {
            fprintf(stderr, "ERROR: Could not open file \"%s\"\n", file_to_be_openned);
            return EXIT_FAILURE;
        }
    }

    cbfr_t *buffer = cb_create(buffer_size);

    if (buffer == NULL)
    {
        fprintf(stderr, "cb_create: Memory error\n");
        safe_file_close(file_to_be_openned, input);
        return EXIT_FAILURE;
    }

    int c;                 // read character
    int idx = 0;           // position of the next character
    bool overflow = false; // states wheter the LINE_LENGTH_LIMIT was reached
    char *line = malloc(sizeof(char) * (LINE_LENGTH_LIMIT + 1));
    if (line == NULL)
    {
        fprintf(stderr, "main: Memory error\n");
        cb_free(buffer);
        safe_file_close(file_to_be_openned, input);
        return EXIT_FAILURE;
    }
    int overflow_happened = 0;
    while (true)
    {
        c = fgetc(input);
        if (c == '\n' || c == EOF) // line processing
        {
            if (c == EOF && idx == 0) // classic ending of an UNIX file (...,LF,EOF)
            {
                free(line);
                break;
            }
            line[idx] = '\0';
            line[idx] = (c == '\n') ? '\n' : '\0';
            cb_put(buffer, line);
            idx = 0;
            overflow = false;
            if (c == EOF) // Last line does not end with LF but with EOF
            {
                break;
            }
            line = malloc(sizeof(char) * (LINE_LENGTH_LIMIT + 1));
            if (line == NULL)
            {
                fprintf(stderr, "main: Memory error\n");
                cb_free(buffer);
                safe_file_close(file_to_be_openned, input);
                return EXIT_FAILURE;
            }
        }
        else if (!overflow) // save the read character
        {
            if (idx == LINE_LENGTH_LIMIT) // overflow detected
            {
                if (overflow_happened == 0)
                {
                    fprintf(stderr, "WARNING: One or more lines are longer than %d, so their whole content could not be displayed.\n", LINE_LENGTH_LIMIT);
                }
                overflow = true;
                overflow_happened++;
                continue;
            }
            line[idx] = (char)c;
            idx++;
        }
    }

    // Print last N lines
    while ((line = cb_get(buffer)) != NULL)
    {
        print_line(line);
        free(line);
    }

    cb_free(buffer);
    safe_file_close(file_to_be_openned, input);
    return 0;
}

cbfr_t *cb_create(unsigned n)
{
    assert(n != 0);

    cbfr_t *cb = malloc(sizeof(cbfr_t) + sizeof(char *) * (n + 1));
    if (cb == NULL)
    {
        return NULL;
    }
    cb->size = n;
    cb->start = 0;
    cb->end = 0;
    return cb;
}

void cb_put(cbfr_t *cb, char *line)
{
    assert(cb != NULL);
    assert(line != NULL);

    // Buffer is full. First line must be removed and deallocated.
    if ((cb->end + 1) % cb_true_size(cb) == cb->start)
    {
        free(cb->lines[cb->start]);
        cb_inc_start(cb);
    }
    cb->lines[cb->end] = line;
    cb_inc_end(cb);
}

char *cb_get(cbfr_t *cb)
{
    assert(cb != NULL);

    if (cb->start == cb->end) // Empty buffer
    {
        return NULL;
    }
    char *line = cb->lines[cb->start];
    cb_inc_start(cb);
    return line;
}

void cb_free(cbfr_t *cb)
{
    assert(cb != NULL);

    char *line;
    while (cb->start != cb->end)
    {
        line = cb_get(cb);
        free(line);
    }
    free(cb);
}

void print_line(char *line)
{
    assert(line != NULL);

    for (int i = 0; line[i] != '\0'; i++)
    {
        putchar(line[i]);
        if (line[i] == '\n')
        {
            return;
        }
    }
}
