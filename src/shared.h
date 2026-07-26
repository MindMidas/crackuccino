#ifndef SHARED_H
#define SHARED_H

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hash_dict.h"

// 26 letters + '\0'
#define MAX_DICT 27

typedef enum {
    PERMUTE_FULL, // permute 0...n-1 -> n!
    PERMUTE_SUFFIX_PER_RANK // fix first letters, permute 1...n-1 -> active * (n-1)!
} PermuteMode;


char *read_ciphertext(const char *path, int *out_nbytes);
void build_input_dict(const char *text, char *input_dict);
void swap(char *a, char *b);
unsigned long long fact_ull(int n);
void print_permutations_summary(const char *title, int size, int nletters, int depth,
    unsigned long long total_visited, unsigned long long total_hits, double runtime);



#endif