#include "decrypt_serial.h"


/************************************************************************************************
 * @brief print direct decrypt usage
 * @param prog program name
 ************************************************************************************************/
static void print_key_usage(
    const char *prog
) {
    fprintf(stderr, "Usage: %s <ciphertext.txt> --input-dict <letters> --encrypt-dict <letters>\n", prog);
    fprintf(stderr, "       %s -h | --help\n", prog);
    fprintf(stderr, "Example: %s ciphertext.txt --input-dict brewmafshcpuino --encrypt-dict epcisunfobmwhar\n", prog);
    fprintf(stderr, "This path uses the known mapping directly and does not brute force.\n");
}



/************************************************************************************************
 * @brief normalize and validate a dictionary argument
 * @param value raw arg
 * @param out normalized output
 * @param label label for errors
 * @return true if valid
 ************************************************************************************************/
static bool normalize_dict_arg(
    const char *value,
    char *out,
    const char *label
) {
    int seen[26] = {0};
    int len = (int)strlen(value);

    if (len >= MAX_DICT) {
        fprintf(stderr, "error: %s must be at most 26 letters\n", label);
        return false;
    }

    for (int i = 0; i < len; i++) {
        char c = (char)tolower((unsigned char)value[i]);

        if (!isalpha((unsigned char)c)) {
            fprintf(stderr, "error: %s must use only letters a-z\n", label);
            return false;
        }

        int k = c - 'a';
        if (seen[k]) {
            fprintf(stderr, "error: %s cannot repeat letters\n", label);
            return false;
        }

        seen[k] = 1;
        out[i] = c;
    }

    out[len] = '\0';
    return true;
}



/************************************************************************************************
 * @brief validate direct decrypt mappings use same unique letters
 * @param input_dict plaintext letter order
 * @param encrypt_dict encrypted letter order
 * @return true if valid
 ************************************************************************************************/
static bool validate_direct_mapping(
    const char *input_dict,
    const char *encrypt_dict
) {
    int input_len = (int)strlen(input_dict);
    int encrypt_len = (int)strlen(encrypt_dict);

    if (input_len != encrypt_len) {
        fprintf(stderr, "error: input and encrypted dictionaries must have the same length\n");
        return false;
    }

    int input_seen[26] = {0};
    int encrypt_seen[26] = {0};

    for (int i = 0; i < input_len; i++) {
        input_seen[input_dict[i] - 'a'] = 1;
        encrypt_seen[encrypt_dict[i] - 'a'] = 1;
    }

    for (int i = 0; i < 26; i++) {
        if (input_seen[i] != encrypt_seen[i]) {
            fprintf(stderr, "error: input and encrypted dictionaries must contain the same letters\n");
            return false;
        }
    }

    return true;
}



/************************************************************************************************
 * @brief program entry: direct decrypt using a known encrypted mapping
 * @param argc argument count
 * @param argv ciphertext path, --input-dict, and --encrypt-dict
 ************************************************************************************************/
int main(
    int argc,
    char *argv[]
) {
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_key_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc != 6) {
        print_key_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *ciphertext_path = argv[1];
    const char *input_value = NULL;
    const char *encrypt_value = NULL;

    for (int i = 2; i < argc; i += 2) {
        if (i + 1 >= argc) {
            print_key_usage(argv[0]);
            return EXIT_FAILURE;
        }

        if (strcmp(argv[i], "--input-dict") == 0) {
            input_value = argv[i + 1];
        } else if (strcmp(argv[i], "--encrypt-dict") == 0) {
            encrypt_value = argv[i + 1];
        } else {
            print_key_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!input_value || !encrypt_value) {
        print_key_usage(argv[0]);
        return EXIT_FAILURE;
    }

    char input_dict[MAX_DICT];
    char encrypt_dict[MAX_DICT];

    if (!normalize_dict_arg(input_value, input_dict, "input dict")) {
        return EXIT_FAILURE;
    }

    if (!normalize_dict_arg(encrypt_value, encrypt_dict, "encrypt dict")) {
        return EXIT_FAILURE;
    }

    if (!validate_direct_mapping(input_dict, encrypt_dict)) {
        return EXIT_FAILURE;
    }

    char *ciphertext = read_ciphertext(ciphertext_path, NULL);
    if (!ciphertext) {
        fprintf(stderr, "open failed\n");
        return EXIT_FAILURE;
    }

    char *plaintext = (char *)malloc(strlen(ciphertext) + 1);
    if (!plaintext) {
        fprintf(stderr, "malloc failed\n");
        free(ciphertext);
        return EXIT_FAILURE;
    }

    decrypt_with_permutation(ciphertext, encrypt_dict, input_dict, plaintext);

    printf("ciphertext: %s\n", ciphertext);
    printf("input_dict: %s\n", input_dict);
    printf("encrypt_dict: %s\n", encrypt_dict);
    printf("plaintext: %s\n", plaintext);

    free(plaintext);
    free(ciphertext);

    return EXIT_SUCCESS;
}
