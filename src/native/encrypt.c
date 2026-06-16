#include "encrypt.h"


/************************************************************************************************
 * @brief build encrypt dict by shuffling input_dict (Fisher–Yates).
 * @param input_dict ptr to dict of unique letters from input str
 * @param encrypt_dict ptr output buffer (where size >= MAX_DICT) to store unique shuffled letters
 * @note credit https://www.geeksforgeeks.org/dsa/shuffle-a-given-array-using-fisher-yates-shuffle-algorithm/
 ************************************************************************************************/
void build_encrypt_dict(
    const char *input_dict,
    char *encrypt_dict
) {

    // copy input_dict to encrypt_dict
    strcpy(encrypt_dict, input_dict);

    // get num letters
    int n = (int)strlen(encrypt_dict);

    // base case, nothing to shuffle
    if (n <= 1) {
        return; 
    }

    // shuffle
    for (int i = n-1; i > 0; i--) {

        // rand index from 0 to i
        int j = rand() % (i + 1);

        // swap
        swap(&encrypt_dict[i], &encrypt_dict[j]);
    }

    // safety fallback, in case of identical
    if (strcmp(encrypt_dict, input_dict) == 0 && n > 1) {
        
        swap(&encrypt_dict[0], &encrypt_dict[1]);
    }
}


/************************************************************************************************
 * @brief copy a user-supplied encrypt dict after validating it matches input_dict
 * @param input_dict ptr to dict of unique letters from input str
 * @param custom_dict ptr to user-supplied shuffled letters
 * @param encrypt_dict ptr output buffer (where size >= MAX_DICT)
 * @return 1 on valid mapping, 0 otherwise
 ************************************************************************************************/
int set_custom_encrypt_dict(
    const char *input_dict,
    const char *custom_dict,
    char *encrypt_dict
) {

    int n = (int)strlen(input_dict);
    if ((int)strlen(custom_dict) != n) {
        return 0;
    }

    int input_seen[26] = {0};
    int custom_seen[26] = {0};

    // count letters in both dicts so order can differ but membership cannot
    for (int i = 0; i < n; i++) {
        int input_k = input_dict[i] - 'a';
        int custom_k = custom_dict[i] - 'a';

        if (input_k < 0 || input_k >= 26 || custom_k < 0 || custom_k >= 26) {
            return 0;
        }

        input_seen[input_k]++;
        custom_seen[custom_k]++;
    }

    for (int i = 0; i < 26; i++) {
        if (input_seen[i] != custom_seen[i]) {
            return 0;
        }
    }

    // keep the random path's behavior: avoid identity mappings when possible
    if (n > 1 && strcmp(input_dict, custom_dict) == 0) {
        return 0;
    }

    strcpy(encrypt_dict, custom_dict);
    return 1;
}



/************************************************************************************************
 * @brief encrypt plaintext using pos mapping between input_dict and encrypt_dict
 * @param plaintext plaintext from input str
 * @param input_dict ptr to dict of unique letters from input str
 * @param encrypt_dict ptr to dict of shuffled version of input_dict
 * @param ciphertext ptr to output buffer (size >= strlen(plaintext) + 1)
 ************************************************************************************************/
void encrypt_plaintext(
    const char *plaintext,
    const char *input_dict,
    const char *encrypt_dict,
    char *ciphertext
) {

    // build identity letter map a...z
    char map[26];
    for (int i = 0; i < 26; i++) {
        map[i] = (char)('a' + i);
    }


    // apply mapping from input_dict to encrypt_dict
    int len = (int)strlen(input_dict);
    for (int i = 0; i < len; i++) {

        int k = input_dict[i] - 'a';

        // store its encrypted replacement
        if (k >= 0 && k < 26) {
            map[k] = encrypt_dict[i];
        }
    }


    // transform plaintext to ciphertext
    int n = (int)strlen(plaintext);
    for (int i = 0; i < n; i++) {

        // get letter and convert to lowercase
        unsigned char uc = (unsigned char)plaintext[i];
        char c = (char)tolower(uc);

        // check if valid letter
        if (isalpha((unsigned char)c)) {
            int k = c - 'a';

            // map encrypted letter
            ciphertext[i] = map[k];
        
        // map spaces and non-alpha
        } else {
            ciphertext[i] = plaintext[i];
        }
    }

    // add EoS
    ciphertext[n] = '\0';
}



/************************************************************************************************
 * @brief write ciphertext to ./ciphertext.txt, exits on error
 * @param ciphertext ptr to encrypted string to write
 ************************************************************************************************/
void write_ciphertext(
    const char *ciphertext
) {

    FILE *fp = fopen("ciphertext.txt", "w");

    // failure
    if (!fp) {
        perror("ciphertext.txt");
        exit(EXIT_FAILURE);
    }
    
    // write and close
    fprintf(fp, "%s\n", ciphertext);
    fclose(fp);
}



/************************************************************************************************
 * @brief program entry: builds dicts, encrypts, and writes to ciphertext.txt
 * @param argc argument count (expects plaintext and optional encrypt dict)
 * @param argv argv[1] plaintext string (quoted if contains spaces)
 ************************************************************************************************/
int main(
    int argc, 
    char *argv[]
) {

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: %s \"plaintext\" [encrypt_dict]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // get plaintext and set dicts
    const char *plaintext = argv[1];
    char input_dict[MAX_DICT];
    char encrypt_dict[MAX_DICT];

    // malloc ciphertext buffer
    int n = (int)strlen(plaintext);
    char *ciphertext = (char *)malloc(n + 1);
    if (!ciphertext) {
        fprintf(stderr, "malloc failed\n");
        return EXIT_FAILURE;
    }

    // set seed for shuffling
    srand((unsigned)time(NULL));

    // build dicts 
    build_input_dict(plaintext, input_dict);
    if (argc == 3) {
        if (!set_custom_encrypt_dict(input_dict, argv[2], encrypt_dict)) {
            fprintf(stderr, "encrypt_dict must be a shuffled permutation of input_dict.\n");
            free(ciphertext);
            return EXIT_FAILURE;
        }
    } else {
        build_encrypt_dict(input_dict, encrypt_dict);
    }
    encrypt_plaintext(plaintext, input_dict, encrypt_dict, ciphertext);
    write_ciphertext(ciphertext);

    // echo fpr debug
    printf("input_dict: %s\n", input_dict);
    printf("encrypt_dict: %s\n", encrypt_dict);
    printf("ciphertext: %s\n", ciphertext);
    printf("wrote: ./ciphertext.txt\n");

    free(ciphertext);
    
    return EXIT_SUCCESS;
}
