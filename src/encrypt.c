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
 * @brief print encrypt usage
 * @param prog program name
 ************************************************************************************************/
static void print_encrypt_usage(
    const char *prog
) {
    fprintf(stderr, "Usage: %s \"plaintext\" [--key <encrypted_letter_order>]\n", prog);
    fprintf(stderr, "       %s -h | --help\n", prog);
    fprintf(stderr, "Example: %s \"hello world today\" --key hrldowe\n", prog);
    fprintf(stderr, "--key can be full or partial. Missing letters are appended automatically.\n");
    fprintf(stderr, "Without --key, Crackuccino shuffles the plaintext letters randomly.\n");
}



/************************************************************************************************
 * @brief build custom encrypted letter order against input dict
 * @param input_dict unique letters from plaintext
 * @param key full or partial custom encrypted letter order
 * @param encrypt_dict output buffer
 * @return true if valid, false otherwise
 ************************************************************************************************/
static bool build_custom_key(
    const char *input_dict,
    const char *key,
    char *encrypt_dict
) {
    int input_len = (int)strlen(input_dict);
    int key_len = (int)strlen(key);

    if (key_len <= 0) {
        fprintf(stderr, "error: key cannot be empty\n");
        return false;
    }

    if (key_len > input_len) {
        fprintf(stderr, "error: key can be at most %d letters long for this plaintext\n", input_len);
        return false;
    }

    int input_seen[26] = {0};
    int key_seen[26] = {0};

    for (int i = 0; i < input_len; i++) {
        int k = input_dict[i] - 'a';
        if (k < 0 || k >= 26) {
            return false;
        }
        input_seen[k] = 1;
    }

    for (int i = 0; i < key_len; i++) {
        char c = (char)tolower((unsigned char)key[i]);

        if (!isalpha((unsigned char)c)) {
            fprintf(stderr, "error: key must use only letters a-z\n");
            return false;
        }

        int k = c - 'a';

        if (key_seen[k]) {
            fprintf(stderr, "error: key cannot repeat letters\n");
            return false;
        }

        if (!input_seen[k]) {
            fprintf(stderr, "error: key letters must come from detected letters: %s\n", input_dict);
            return false;
        }

        key_seen[k] = 1;
        encrypt_dict[i] = c;
    }

    int out_idx = key_len;

    // append unmapped letters in detected order so partial keys still produce a full mapping
    for (int i = 0; i < input_len; i++) {
        int k = input_dict[i] - 'a';

        if (!key_seen[k]) {
            encrypt_dict[out_idx++] = input_dict[i];
        }
    }

    encrypt_dict[out_idx] = '\0';

    return true;
}



/************************************************************************************************
 * @brief program entry: builds dicts, encrypts, and writes to ciphertext.txt
 * @param argc argument count
 * @param argv argv[1] plaintext string, optional --key <encrypted_letter_order>
 ************************************************************************************************/
int main(
    int argc, 
    char *argv[]
) {

    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_encrypt_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc != 2 && argc != 4) {
        print_encrypt_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 4 && strcmp(argv[2], "--key") != 0) {
        print_encrypt_usage(argv[0]);
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

    if (argc == 4) {
        if (!build_custom_key(input_dict, argv[3], encrypt_dict)) {
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
