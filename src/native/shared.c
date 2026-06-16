#include "shared.h"


/************************************************************************************************
 * @brief read a file to malloc'd Nul-terminated buffer
 * @param path file path to read
 * @param out_nbytes out length incl '\0' (can be NULL)
 * @return malloc'd buffer or NULL on failure
 ************************************************************************************************/
char *read_ciphertext(
    const char *path,
    int *out_nbytes
) {

    // open file
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    // read entire file into malloc'd buffer
    // fseek to end, ftell size, rewind back to start.
    if (fseek(fp, 0, SEEK_END) != 0) { 
        fclose(fp);
        return NULL; 
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp); 
        return NULL; 
    }
    rewind(fp);

    // malloc buffer
    char *buf = (char *)malloc((int)size + 1);
    if (!buf) {
        fclose(fp);
        return NULL; 
    }

    // read into memory and add EoS
    int n = (int)fread(buf, 1, (int)size, fp);
    buf[n] = '\0';
    fclose(fp);

    // trim trailing space/newlines
    int clen = (int)strlen(buf);
    while (clen > 0 && (buf[clen - 1] == '\n' || buf[clen - 1] == '\r')) {
        buf[--clen] = '\0'; // move EoS
    }

    if (out_nbytes) {
        *out_nbytes = clen + 1;
    }

    return buf;
}



/************************************************************************************************
 * @brief build input dict of unique lowercase letters (in order of first seen)
 * @param text ptr to string
 * @param input_dict ptr to output buffer (where size >= MAX_DICT) to store unique letters from text
 ************************************************************************************************/
void build_input_dict( 
    const char *text,
    char *input_dict
) {

    int seen[26] = {0};
    int idx = 0;

    // get chars and build dict
    for (int i = 0; text[i] != '\0'; i++) {
        
        unsigned char uc = (unsigned char)text[i];
        char c = (char)tolower(uc);

        // check if valid letter
        if (isalpha((unsigned char)c)) {

            // get index
            int k = c - 'a';

            if (k >= 0 && k < 26 && !seen[k]) {

                // set seen
                seen[k] = 1;

                // add letter to dict
                input_dict[idx++] = c;

                // safety guard in case
                if (idx >= MAX_DICT - 1) {
                    break;
                }
            }
        }
    }

    // EoS
    input_dict[idx] = '\0';
}



/************************************************************************************************
 * @brief swap two chars
 * @param a ptr to string to swap with b
 * @param b ptr to string to swap with a
 ************************************************************************************************/
void swap(
    char *a, 
    char *b
) {
    char t = *a;
    *a = *b;
    *b = t;
}



/************************************************************************************************
 * @brief compute n! as unsigned long long (valid up to n=20 w/out overflow)
 * @param n num terms to multiply (non-negative integer)
 * @return factorial of n as unsigned long long, or 0 if n < 0 || n > 20
 ************************************************************************************************/
unsigned long long fact_ull(
    int n
) {

    if (n < 0 || n > 20) {
        return 0;
    }

    unsigned long long f = 1;

    for (int i = 2; i <= n; i++) {
        f *= (unsigned long long)i;
    }

    return f;
}



/************************************************************************************************
 * @brief print permutations summary (serial/MPI)
 * @param title label for this run (ex: "rank 0", "rank 1", "serial")
 * @param size total MPI ranks (use 1 for serial)
 * @param nletters num unique letters in input_dict (n)
 * @param depth prefix target depth 
 * @param total_visited total num permutation leaves visited
 * @param total_hits total num valid dictionary hits
 * @param runtime runtime in seconds
 ************************************************************************************************/
void print_permutations_summary(
    const char *title,
    int size,
    int nletters,
    int depth,
    unsigned long long total_visited,
    unsigned long long total_hits,
    double runtime
) {
    unsigned long long expected_total = 0ULL;

    // empty permutation
    if (nletters == 0) {

        expected_total = 1ULL;

    } else {
        expected_total = fact_ull(nletters);
    }
    
    printf("\n************************************************************************************************\n");
    printf("[%s] permutation summary:\n", title);
    printf("ranks:      %d\n", size);
    printf("n_letters:  %d\n", nletters);
    printf("depth:      %d\n", depth);
    printf("expected:   %llu\n", expected_total);
    printf("visited:    %llu\n", total_visited);
    printf("valid_hits: %llu\n", total_hits);
    printf("runtime_sec:  %.6f\n", runtime);
    printf("************************************************************************************************\n");
}
