#ifndef HASH_DICT_H
#define HASH_DICT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define HD_LINE_BUFFER 512 // max word size with special chars
#define HD_SIZE 104729 // num buckets in hash dict

/************************************************************************************************
 * @brief hash dictionary type (case-insensitive; stores lowercase).
 ************************************************************************************************/
typedef struct HashDict HashDict;


HashDict *hd_create(int table_size);
void hd_destroy(HashDict *dict);
int hd_insert(HashDict *dict, const char *word);
int hd_exists(const HashDict *dict, const char *word);
int all_words_in_dict(const HashDict *hd, const char *text);
int hd_load_file(HashDict *dict, const char *filename);
int hd_size(const HashDict *dict);
void hd_stats(const HashDict *dict, FILE *out);
int hd_dump_file(const HashDict *dict, FILE *out);


#endif
