#ifndef HIT_BUFFER_H
#define HIT_BUFFER_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

/************************************************************************************************
 * @brief structs for managing hit records
 ************************************************************************************************/
typedef struct HitList HitList;


HitList *hl_create(int n, int L, int chunk_cap);
void hl_push(HitList *hl, int rank, const char *perm, const char *plain);
void hl_flatten(const HitList *hl, char **out, int *out_bytes);
void hl_free(HitList *hl);
int hl_record_size(int n, int L);
void hl_print(const HitList *hl, FILE *out);

#endif
