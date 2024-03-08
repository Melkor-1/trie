#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#define _POSIX_C_SOURCE 200819L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>

#include <getopt.h>

#define IO_IMPLEMENTATION
#define IO_STATIC
#include "io.h"

#define PROGRAM_NAME    "auto-complete"
#define OUTPUT_DOT_FILE "graph.dot"
#define AC_BUFFER_CAP   1024 * 2

struct flags {
    bool kflag;                 /* Keep the transient .DOT file. */
    bool hflag;                 /* Help message. */
    bool sflag;                 /* Generate a .SVG file. */
    bool cflag;                 /* Suggest autocompletions. */
    bool pflag;                 /* Prefix for the .DOT file. */
};

#ifdef DEBUG
#include "size.h"
#define debug_printf(fmt, ...) \
    fprintf(stdout, "%s:%d:%s(): " fmt, __FILE__, __LINE__, \
            __func__, __VA_ARGS__)
#define D(x) x
#else
#define D(x) (void) 0
#endif

static void help(FILE *sink)
{
    fprintf(sink, "\nUSAGE\n"
        "\t" PROGRAM_NAME " [OPTIONS] [filename]\n\n"
        "DESCRIPTION\n"
        "\t" PROGRAM_NAME
        " is a program for auto-completion and graph visualization.\n\n"
        "OPTIONS:\n" "\t-k, --keep\t\tKeep the transient .DOT file.\n"
        "\t-h, --help\t\tDisplay this help message and exit.\n"
        "\t-s, --svg \t\tGenerate a .SVG file (with optional prefix).\n"
        "\t-c, --complete PREFIX   Suggest autocompletions for prefix.\n"
        "\t-p, --prefix PREFIX\tPrefix for the .DOT file.\n\n");
    exit(EXIT_SUCCESS);
}

static void usage_err(const char *prog_name)
{
    fprintf(stderr, "The syntax of the command is incorrect.\n"
        "Try %s -h for more information.\n", prog_name);
    exit(EXIT_FAILURE);
}

static void parse_options(const struct option * long_options,
                          struct flags *        opt_ptr, 
                          int                   argc, 
                          char **restrict       argv,
                          const char **restrict prefix)
{
    int c = 0;
    int err_flag = 0;

    while (true) {
        c = getopt_long(argc, argv, "shkc:p:", long_options, NULL);
        
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'k':
                ++err_flag;
                opt_ptr->kflag = true;
                break;
            case 'h':
                help(stdout);
                break;
            case 's':
                if (opt_ptr->cflag) {
                    fprintf(stderr,
                        "Error: -s/--svg specified after -c/--complete.\n");
                    usage_err(argv[0]);
                }

                if (opt_ptr->pflag) {
                    --err_flag;
                }

                if (opt_ptr->kflag) {
                    --err_flag;
                }

                opt_ptr->sflag = true;
                break;
            case 'c':
                if (opt_ptr->sflag) {
                    fprintf(stderr,
                        "Error: -c/--complete specified after -s/--svg.\n");
                    usage_err(argv[0]);
                }

                *prefix = optarg;

                if (strlen(*prefix) >= AC_BUFFER_CAP) {
                    fprintf(stderr, "Error: PREFIX too long.\n");
                    exit(EXIT_FAILURE);
                }

                opt_ptr->cflag = true;
                break;
            case 'p':
                if (!opt_ptr->sflag) {
                    err_flag = true;
                }

                *prefix = optarg;
                opt_ptr->pflag = true;
                break;

                /* case '?' */
            default:
                usage_err(argv[0]);
        }
    }

    /* If the -p or -k flag was specified without -s: */
    /* Note: GNU provides an extension for optional arguments, which
     *       can be used to provide an optional prefix for -s. */
    if (err_flag) {
        if (opt_ptr->pflag) {
            fputs("Error: -p specified without -s.\n", stderr);
        }

        if (opt_ptr->kflag) {
            fputs("Error: -k specified without -s.\n", stderr);
        }
        usage_err(argv[0]);
    }
}

/* Instead of using the whole ASCII charset, we'd only use the set of printable
 * characters. This cuts down the struct size from 2056 bytes to 768 bytes if 
 * using 64-bit pointers, or from 1028 bytes to 384 bytes if using 32-bit integers.
 * (or so pahole outputs, there might be a slight difference in the size across 
 * different platforms).
 */

#define CHILDREN_COUNT 95

/* In the children array, 32 should map to 0, 65 should map to 33, and so on. */
#define ASCII_OFFSET ' '        /* ASCII code: 32, start of printable characters. */

/* This wastes half the indexing range. Perhaps we can use uint32_t instead of
 * int32_t, and use UINT32_MAX as the sentinel value.
 */
#define INVALID_OFFSET -1

#define INITIAL_POOL_CAP 1024 * 2

typedef struct {
    /* TODO: This still wastes a lot of memory. Something like Pythons's dict 
     * would be better. Explore the idea of using a dynamically growing 
     * hash-table for Node.children. 
     */

    /* We shall store indices within the `Trie.pool` array instead of storing
     * huge word size pointers. This also simplifies serializing and
     * deserializing the structure.
     *
     * See also: A case where int32_t wasn't sufficient - bbc.com/news/world-asia-30288542
     */
    int32_t children[CHILDREN_COUNT];
    bool terminal;
} Node;

struct Trie {
    Node *pool;
    int32_t count;
    int32_t capacity;
} Trie; 

static bool init_pool(void)
{
    Trie.pool = malloc(sizeof *Trie.pool * INITIAL_POOL_CAP);

    if (Trie.pool == NULL) {
        perror("malloc()");
        return false;
    }

    return Trie.capacity = INITIAL_POOL_CAP;
}

static inline void free_pool(void)
{
    Trie.capacity = 0;
    Trie.count = 0;
    free(Trie.pool);
}

static int32_t alloc_node(void)
{
    if (Trie.count >= Trie.capacity) {
        const int32_t remaining = INT32_MAX - Trie.capacity;

        /* We can no longer add more nodes. Bail out. */
        if (remaining == 0) {
            /* Is this helpful? */
            fputs("Error: too many nodes. Consider recompiling the program"
                "with a greater int width for more indexing range.\n", stderr);
            exit(EXIT_FAILURE);
        }

        Trie.capacity += remaining < INITIAL_POOL_CAP ? remaining : INITIAL_POOL_CAP;
        void *const tmp = realloc(Trie.pool, sizeof *Trie.pool * (size_t) Trie.capacity);

        if (tmp == NULL) {
            perror("realloc()");
            return INVALID_OFFSET;
        }

        Trie.pool = tmp;
    }

    Node *const tmp = &Trie.pool[Trie.count];

    memset(tmp->children, INVALID_OFFSET, sizeof tmp->children);
    Trie.pool[Trie.count].terminal = false;
    return Trie.count++;
}

static bool insert_text(int32_t root_idx, const char *text)
{
    while (*text != '\0') {
        const int32_t idx = *text - ASCII_OFFSET;

        if (Trie.pool[root_idx].children[idx] == INVALID_OFFSET) {
            const int32_t tmp = alloc_node();

            if (tmp == INVALID_OFFSET) {
                return false;
            }

            Trie.pool[root_idx].children[idx] = tmp;
        }
        root_idx = Trie.pool[root_idx].children[idx];
        ++text;
    }

    return Trie.pool[root_idx].terminal = true;
}

static char ac_buffer[AC_BUFFER_CAP];
static size_t ac_buffer_sz;

static void ac_buffer_push(char ch)
{
    /* No check here, for we would have exited in parse_options() if the length
     * of the prefix was greater than AC_BUFFER_CAP.
     */
    ac_buffer[ac_buffer_sz++] = ch;
}

static void ac_buffer_pop(void)
{
    if (ac_buffer_sz > 0) {
        ac_buffer_sz--;
    }
}

static int32_t find_prefix(int32_t root_idx, const char *prefix)
{
    while (*prefix != '\0') {
        const int32_t child_idx = *prefix - ASCII_OFFSET;

        if (Trie.pool[root_idx].children[child_idx] == INVALID_OFFSET) {
            return INVALID_OFFSET;
        }

        root_idx = Trie.pool[root_idx].children[child_idx];
        ac_buffer_push(*prefix);
        ++prefix;
    }
    return root_idx;
}

static void dump_dot_prefix(FILE *sink, int32_t root_idx)
{
    for (size_t i = 0; i < CHILDREN_COUNT; ++i) {
        if (Trie.pool[root_idx].children[i] != INVALID_OFFSET) {
            const int32_t child_index = Trie.pool[root_idx].children[i];

            if (Trie.pool[child_index].terminal) {
                fprintf(sink,
                    "\tNode_%" PRId32 " [label=%c,fillcolor=lightgreen]\n",
                    child_index, (char) (i + ASCII_OFFSET));
            } else {
                fprintf(sink, "\tNode_%" PRId32 " [label=%c]\n", child_index,
                    (char) (i + ASCII_OFFSET));
            }

            fprintf(sink, "\tNode_%" PRId32 " -> Node_%" PRId32 " [label=%c]\n",
                root_idx, child_index, (char) (i + ASCII_OFFSET));
            dump_dot_prefix(sink, Trie.pool[root_idx].children[i]);
        }
    }
}

static void dump_dot_whole(FILE *sink)
{
    for (int32_t i = 0; i < Trie.count; ++i) {
        const int32_t index = i;

        for (size_t j = 0; j < CHILDREN_COUNT; ++j) {
            if (Trie.pool[i].children[j] != INVALID_OFFSET) {
                const int32_t child_index = Trie.pool[i].children[j];

                if (Trie.pool[child_index].terminal) {
                    fprintf(sink,
                        "\tNode_%" PRId32 " [label=%c,fillcolor=lightgreen]\n",
                        child_index, (char) (j + ASCII_OFFSET));
                } else {
                    fprintf(sink, "\tNode_%" PRId32 " [label=%c]\n",
                        child_index, (char) (j + ASCII_OFFSET));
                }

                fprintf(sink,
                    "\tNode_%" PRId32 " -> Node_%" PRId32 " [label=%c]\n",
                    index, child_index, (char) (j + ASCII_OFFSET));
            }
        }
    }
}

static void print_suggestions(int32_t root_idx)
{
    if (Trie.pool[root_idx].terminal) {
        fwrite(ac_buffer, ac_buffer_sz, 1, stdout);
        fputc('\n', stdout);
    }

    for (size_t i = 0; i < CHILDREN_COUNT; ++i) {
        if (Trie.pool[root_idx].children[i] != INVALID_OFFSET) {
            ac_buffer_push((char) (i + ASCII_OFFSET));
            print_suggestions(Trie.pool[root_idx].children[i]);
            ac_buffer_pop();
        }
    }
}

static bool populate_trie(int32_t root_idx, char **lines, size_t num_lines)
{
    for (size_t i = 0; i < num_lines; ++i) {
        if (!insert_text(root_idx, lines[i])) {
            return false;
        }
    }
    return true;
}

static bool generate_graph(void)
{
    if (system("dot -Tsvg " OUTPUT_DOT_FILE " -O")) {
        fprintf(stderr, "Error: failed to generate the .SVG file, %s.\n",
            strerror(errno));
        return false;
    }

    return true;
}

static bool generate_dot(int32_t root_idx, const char *prefix)
{
    FILE *const sink = fopen(OUTPUT_DOT_FILE, "w");

    if (sink == NULL) {
        perror("fopen()");
        return false;
    }

    fprintf(sink, "digraph Trie {\n"
        "\tnode [fillcolor=lightblue,style=filled,arrowhead=vee,color=black]\n"
        "\tNode_%" PRId32 " [label=%s]\n", root_idx, prefix ? prefix : "root");

    root_idx == 0 ? dump_dot_whole(sink) : dump_dot_prefix(sink, root_idx);
    fprintf(sink, "}\n");
    return !fclose(sink);
}

static bool process_args(int32_t                root_idx,
                         const struct flags *   options, 
                         const char *restrict   prefix,
                         const char *restrict   out_file)
{
    bool rv = true;

    /* Handle --complete first as we are going to modify root later in case
     * prefix is not a null pointer. 
     */
    if (options->cflag) {
        const int32_t subtree_idx = find_prefix(root_idx, prefix);

        if (subtree_idx == INVALID_OFFSET) {
            fprintf(stderr, "Error: Unable to find prefix.\n");
            rv = !rv;
            goto cleanup;
        }

        print_suggestions(subtree_idx);
    }

    if (options->sflag) {
        if (prefix) {
            const int32_t subtree_idx = find_prefix(root_idx, prefix);

            if (subtree_idx == INVALID_OFFSET) {
                fprintf(stderr, "Error: Unable to find prefix.\n");
                rv = !rv;
                goto cleanup;
            }
            root_idx = subtree_idx;
        }

        if (!generate_dot(root_idx, prefix)
            || !generate_graph()) {
            rv = !rv;
        }
    }

  cleanup:
    if (!options->kflag) {
        remove(out_file);
    }

    return rv;
}

int main(int argc, char *argv[])
{

    /* Sanity check. POSIX requires the invoking process to pass a non-NULL 
     * argv[0]. 
     */
    if (!argv[0]) {
        fprintf(stderr,
            "A NULL argv[0] was passed through an exec system call.\n");
        return EXIT_FAILURE;
    }

    static const struct option long_options[] = {
        { "keep", no_argument, NULL, 'k' },
        { "help", no_argument, NULL, 'h' },
        { "svg", no_argument, NULL, 's' },
        { "complete", required_argument, NULL, 'c' },
        { "prefix", required_argument, NULL, 'p' },
        { NULL, 0, NULL, 0 },
    };

    FILE *in_file = stdin;
    struct flags options = { false, false, false, false, false };
    const char *search_prefix = NULL;

    parse_options(long_options, &options, argc, argv, &search_prefix);

    if (options.sflag || options.cflag) {
        if ((optind + 1) == argc) {
            in_file = fopen(argv[optind], "r");
            if (in_file == NULL) {
                perror(argv[optind]);
                return EXIT_FAILURE;
            }
        }
    } else {
        usage_err(PROGRAM_NAME);
    }

    if (optind > argc) {
        usage_err(PROGRAM_NAME);
    }

    size_t nbytes = 0;
    char *const content = io_read_file(in_file, &nbytes);
    size_t nlines = 0;
    char **lines = io_split_lines(content, &nlines);

    if (lines == NULL) {
        free(content);
        perror("fread()");
        return EXIT_FAILURE;
    }

    bool rv = true;
    int32_t root_idx = INVALID_OFFSET;

    if (!init_pool()
        || (root_idx = alloc_node()) == INVALID_OFFSET
        || !populate_trie(root_idx, lines, nlines)) {
        rv = !rv;
        goto cleanup;

    }

    D(
        char *const total = calculate_size((size_t) Trie.capacity * sizeof *Trie.pool);
        char *const used = calculate_size((size_t) Trie.count * sizeof *Trie.pool);

        debug_printf("Total lines read: %zu.\n" "Total nodes allocated: %"
                    PRId32 ".\n" "Total nodes used: %" PRId32 ".\n"
                    "Total memory allocated: %s.\n" "Total memory used: %s.\n",
                    nlines, 
                    Trie.capacity, 
                    Trie.count, 
                    total, 
                    used); 
        /* free(total); */
        /* free(used); */
    );
    
    rv = process_args(root_idx, &options, search_prefix, OUTPUT_DOT_FILE);

  cleanup:
    /* We're exiting. There's no need of freeing memory. */
    /* free_pool();                /1* NO-OP if init_pool() returns false. *1/ */
    /* free(content); */
    /* free(lines); */

    if (in_file != stdin) {
        fclose(in_file);
    }

    return rv ? EXIT_SUCCESS : EXIT_FAILURE;
}
