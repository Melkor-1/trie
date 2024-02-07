#include <stdio.h>

static bool process_args(int32_t root_idx,
    const struct flags *restrict options, const char *restrict prefix,
    const char *restrict out_file)
{
    bool rv = true;
    const int32_t subtree_idx = find_prefix(root_idx, prefix);

    if (subtree_idx == INVALID_OFFSET) {
        fprintf(stderr, "Error: Unable to find prefix.\n");
        rv = !rv;
        goto cleanup;
    }

    /* Handle --complete first as we are going to modify root later in case
     * prefix is not a null pointer. 
     */
    if (options->cflag) {
        print_suggestions(subtree_idx);
    }

    if (options->sflag) {
        if (prefix) {
            root_idx = subtree_idx;
        }

        /* At this point, we can free the file and Trie.pool, but we'd have
         * to change the structure of the code. */
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


