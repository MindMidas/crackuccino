#include "decrypt_serial.h"


/************************************************************************************************
 * @brief decrypt ciphertext using mapping defined by input_dict to perm
 * @param ciphertext ptr to ciphertext string
 * @param input_dict dict of unique letters from ciphertext
 * @param permutation permutation (candidate mapping for decryption)
 * @param plaintext output buffer (size >= strlen(ciphertext) + 1), receives decoded text
 ************************************************************************************************/
void decrypt_with_permutation(
    const char *ciphertext,
    const char *input_dict,
    const char *permutation,
    char *plaintext
) {

    // build identity letter map a...z
    char map[26];
    for (int i = 0; i < 26; i++) {
        map[i] = (char)('a' + i);
    }

    // map each ciphertext letter in input_dict to its matching letter in permutation
    int len = (int)strlen(input_dict);
    for (int i = 0; i < len; i++) {
        int k = input_dict[i] - 'a';
        map[k] = permutation[i]; // ciphertext-letter -> candidate plaintext-letter
    }

    // create decrypted plaintext by applying map to each char
    int n = (int)strlen(ciphertext);
    for (int i = 0; i < n; i++) {

        // convert to lowercase
        char c = (char)tolower((unsigned char)ciphertext[i]);

        // check if c in a...z
        if (isalpha((unsigned char)c)) {
            plaintext[i] = map[c - 'a']; // substitute via map

        // map spaces and non-alpha
        } else {
            plaintext[i] = ciphertext[i];
        }
    }

    // add EoS
    plaintext[n] = '\0';
}



/************************************************************************************************
 * @brief recursively generate all permutations and print valid decryptions
 * @param permutation current permutation buffer (mutated in place)
 * @param l left index (starts at 0)
 * @param r right index (len-1)
 * @param ciphertext ciphertext string
 * @param input_dict unique letters from ciphertext
 * @param hd ptr to hash dictionary
 * @param decoded buffer (size >= strlen(ciphertext)+1), allocated once by caller
 * @param total_leaves ptr to counter of total permutations visited
 * @param valid_hits ptr to counter of valid plaintexts
 * @param rank MPI rank
 * @param HitList valid hits collector (pass NULL to print instead)
 ************************************************************************************************/
void permute(
    char *permutation,
    int l,
    int r,
    const char *ciphertext,
    const char *input_dict,
    const HashDict *hd,
    char *decoded,
    unsigned long long *total_leaves,
    unsigned long long *valid_hits,
    int rank,
    HitList *hits,
    ProgressCallback progress_callback,
    void *progress_context
) {

    // base case for recursion
    if (l == r) {
        
        // reached a leaf (one full permutation)
        (*total_leaves)++;

        // decrypt and validate
        decrypt_with_permutation(ciphertext, input_dict, permutation, decoded);

        // lookup in hd
        if (all_words_in_dict(hd, decoded)) {

            // reached a valid permutation
            (*valid_hits)++;

            // collect or print valid hit
            if (hits) {
                hl_push(hits, rank, permutation, decoded);
            } else {
                printf("[rank %d] [permutation: %s] found: %s\n", rank, permutation, decoded);
            }
            
        }

        if (progress_callback) {
            progress_callback(progress_context, *total_leaves, *valid_hits, permutation);
        }

        return;
    }

    // recursive step, swap current letter with each choice, recurse, then undo
    for (int i = l; i <= r; i++) {
        swap(&permutation[l], &permutation[i]); // swap
        permute(
            permutation, l + 1, r, ciphertext, input_dict, hd, decoded,
            total_leaves, valid_hits, rank, hits, progress_callback, progress_context
        ); // recurse
        swap(&permutation[l], &permutation[i]); // undo
    }
}



/************************************************************************************************
 * @brief program entry: reads ciphertext + dictionary, builds input_dict, generates all letter 
 * permutations, and prints valid decryptions
 * @param argc argument count (expects 3)
 * @param argv argv[1]=ciphertext path, argv[2]=dictionary path, optional argv[3]=[-s|--stats]
 ************************************************************************************************/
#ifdef BUILD_SERIAL_APP
int main(
    int argc,
    char *argv[]
) {

    bool show_stats = false;

    // allow optional -s / --stats flag
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <ciphertext.txt> <dictionary> [-s|--stats]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 4) {
        if (strcmp(argv[3], "-s") == 0 || strcmp(argv[3], "--stats") == 0) {
            show_stats = true;
        } else {
            fprintf(stderr, "Unknown option: %s\nUsage: %s <ciphertext.txt> <dictionary> [-s|--stats]\n",
                    argv[3], argv[0]);
            return EXIT_FAILURE;
        }
    }

    // start timer
    struct timeval start_time;
    time_now(&start_time);

    int cbytes = 0;
    char *ciphertext = read_ciphertext(argv[1], &cbytes);
    if (!ciphertext) { 
        fprintf(stderr, "open failed\n"); 
        return EXIT_FAILURE; 
    }


    // build and load hash dictionary
    HashDict *hd = hd_create(HD_SIZE);
    if (!hd) {
        fprintf(stderr, "hd_create failed\n");
        free(ciphertext);
        return EXIT_FAILURE;
    }

    int inserted = hd_load_file(hd, argv[2]);
    if (inserted < 0) {
        fprintf(stderr, "failed to open/load dictionary: %s\n", argv[2]);
        hd_destroy(hd);
        free(ciphertext);
        return EXIT_FAILURE;
    }

    // build input_dict and set initial permutation
    char input_dict[MAX_DICT];
    build_input_dict(ciphertext, input_dict);

    char permutation[MAX_DICT];
    strcpy(permutation, input_dict);

    int len = (int)strlen(input_dict);

    // handle no letters
    if (len == 0) {
        printf("ciphertext: %s\nserial plaintext: %s\n", ciphertext, ciphertext);
        hd_destroy(hd);
        free(ciphertext);
        return EXIT_SUCCESS;
    }

    // allocate reusable buffer for decrypted text (len of ciphertext + EoS)
    int n = (int)strlen(ciphertext);
    char *decoded = (char *)malloc(n + 1);
    if (!decoded) {
        hd_destroy(hd);
        free(ciphertext); 
        return EXIT_FAILURE; 
    }

    // counters
    unsigned long long total_leaves = 0ULL;
    unsigned long long valid_hits = 0ULL;


    // build hitlist data struct to store valid hits
    HitList *hits = hl_create(len, n, 20);

    // brute force: gen all perms and test
    permute(
        permutation, 0, len - 1, ciphertext, input_dict, hd, decoded,
        &total_leaves, &valid_hits, 0, hits, NULL, NULL
    );

    // stop timer
    double elapsed = calc_time(start_time);

    hl_print(hits, NULL);

    // print summary
    if (show_stats) {
        print_permutations_summary("serial", 1, len, 1, total_leaves, valid_hits, elapsed);
    }

    // free dictionary, ciphertext, and decoded
    hd_destroy(hd);
    hl_free(hits);
    free(ciphertext);
    free(decoded);
    
    
    return EXIT_SUCCESS;
}
#endif