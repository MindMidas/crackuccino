#ifndef DECRYPT_SERIAL_H
#define DECRYPT_SERIAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "shared.h"
#include "hash_dict.h"
#include "hit_buffer.h"
#include "time_util.h" 

typedef void (*ProgressCallback)(void *context, unsigned long long visited, unsigned long long hits, const char *permutation);

void decrypt_with_permutation(const char *ciphertext, const char *input_dict, const char *permutation, char *plaintext);
void permute(char *permutation, int l, int r, const char *ciphertext, const char *input_dict, const HashDict *hd, 
    char *decoded, unsigned long long *total_leaves, unsigned long long *valid_hits, int rank, HitList *hits,
    ProgressCallback progress_callback, void *progress_context);


#endif