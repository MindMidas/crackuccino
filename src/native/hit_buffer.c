#include "hit_buffer.h"


/************************************************************************************************
 * @brief internal chunk node holding a fixed number of hit records
 ************************************************************************************************/
typedef struct HitChunk {
    char *buf; // buffer storage for (cap * rec_size) bytes
    int used; // num records stored in this chunk
    int cap; // max num records this chunk can store
    struct HitChunk *next;
} HitChunk;



/************************************************************************************************
 * @brief list for collecting hit records across chunks
 ************************************************************************************************/
struct HitList {
    HitChunk *head;
    HitChunk *tail;
    int rec_size; // size in bytes of a single record
    int total_hits; // num records across all chunks
    int n; // perm len
    int L; // strlen(ciphertext)
    int chunk_cap; // records per chunk
};



/************************************************************************************************
 * @brief print all hits from hitlist
 * @param hl ptr to HitList
 * @param out output stream (uses stdout if NULL)
 ************************************************************************************************/
void hl_print(
    const HitList *hl,
    FILE *out
) {
    if (!hl) return;
    if (!out) out = stdout;

    const HitChunk *chunk = hl->head;

    while (chunk) {
        for (int i = 0; i < chunk->used; i++) {

            const char *rec = chunk->buf + (i * hl->rec_size);

            // [ int rank ][ char perm[hl->n+1] ][ char plain[hl->L+1] ]
            int src_rank;
            memcpy(&src_rank, rec, sizeof(int));

            const char *perm  = rec + sizeof(int);
            const char *plain = perm + (hl->n + 1);

            // match MPI print format
            fprintf(out, "[rank %d] [permutation: %s] found: %s\n", src_rank, perm, plain);
        }
        chunk = chunk->next;
    }
}



/************************************************************************************************
 * @brief compute serialized record size for given n and L
 * @param n permutation len
 * @param L plaintext len
 * @return record size in bytes: sizeof(int) + (n+1) + (L+1)
 ************************************************************************************************/
int hl_record_size(
    int n,
    int L
) {
    return (int)(sizeof(int)) + (n + 1) + (L + 1);
}



/************************************************************************************************
 * @brief allocate a new chunk for the hit list
 * @param hl ptr to an initialized HitList
 * @return ptr to a newly allocated HitChunk (fatal on fail)
 ************************************************************************************************/
static HitChunk* hl_new_chunk(
    const HitList *hl
) {

    HitChunk *c = (HitChunk*)malloc(sizeof(HitChunk));

    if (!c) {
        fprintf(stderr, "hit_buffer: failed to allocate HitChunk\n");
        exit(EXIT_FAILURE);
    }

    // add it
    c->cap = hl->chunk_cap;
    c->used = 0;
    c->next = NULL;

    // get num bytes to malloc
    int bytes = c->cap * hl->rec_size;

    // malloc with guard and cast unsigned for size_t
    c->buf = (char*)malloc((bytes > 0) ? (unsigned)bytes : 1u);

    if (!c->buf) {
        fprintf(stderr, "hit_buffer: failed to allocate chunk buffer (%d bytes)\n", bytes);
        free(c);
        exit(EXIT_FAILURE);
    }

    return c;
}



/************************************************************************************************
 * @brief create and init a HitList
 * @param n permutation len
 * @param L plaintext len
 * @param chunk_cap records per chunk
 * @return ptr to new HitList (fatal on fail)
 ************************************************************************************************/
HitList *hl_create(
    int n,
    int L, 
    int chunk_cap
) {

    HitList *hl = (HitList*)malloc(sizeof(HitList));

    if (!hl) {
        fprintf(stderr, "hit_buffer: failed to allocate HitList\n");
        exit(EXIT_FAILURE);
    }

    hl->head = NULL;
    hl->tail = NULL;
    hl->n = n;
    hl->L = L;
    hl->rec_size = hl_record_size(n, L);
    hl->total_hits = 0;
    hl->chunk_cap = (chunk_cap > 0) ? chunk_cap : 20;

    return hl;
}



/************************************************************************************************
 * @brief append one hit record to the list (and allocates chunk when tail full)
 * @param hl ptr to HitList
 * @param rank MPI rank that found the hit
 * @param perm permutation str of len n (n letters + '\0')
 * @param plain plaintext string of len L (L chars + '\0')
 ************************************************************************************************/
void hl_push(
    HitList *hl,
    int rank,
    const char *perm,
    const char *plain
) {

    // create new chunk if full or none
    if (!hl->tail || hl->tail->used == hl->tail->cap) {
        
        HitChunk *c = hl_new_chunk(hl);
        
        if (!hl->head) {
            hl->head = c;
        } else {
            hl->tail->next = c;
        }

        hl->tail = c;
    }

    // write one record at next slot
    int offset_bytes = hl->tail->used * hl->rec_size;
    char *p = hl->tail->buf + offset_bytes;

    // rank
    memcpy(p, &rank, sizeof(int));
    p += sizeof(int);

    // permutation
    memcpy(p, perm, hl->n);
    p[hl->n] = '\0';
    p += (hl->n + 1);

    // plaintext of ciphertext
    memcpy(p, plain, hl->L);
    p[hl->L] = '\0';

    // increment
    hl->tail->used += 1;
    hl->total_hits += 1;
}



/************************************************************************************************
 * @brief flatten the list into a single buffer for MPI_Gatherv
 * @param hl ptr to a HitList
 * @param out output ptr receiving the buffer (NULL if no hits)
 * @param out_bytes num bytes in *out (0 if no hits)
 ************************************************************************************************/
void hl_flatten(
    const HitList *hl,
    char **out,
    int *out_bytes
) {

    // calc total bytes
    int total_bytes = hl->total_hits * hl->rec_size;
    
    // no hits
    if (total_bytes <= 0) {
        *out = NULL;
        *out_bytes = 0;
        return;
    }

    // malloc buffer to hold all hits
    char *all = (char*)malloc((unsigned)total_bytes);
    if (!all) {
        fprintf(stderr, "hit_buffer: failed to allocate flatten buffer (%d bytes)\n", total_bytes);
        exit(EXIT_FAILURE);
    }

    // loop and copy all chunks into new buffer
    char *p = all;
    for (const HitChunk *c = hl->head; c; c = c->next) {
        int bytes = c->used * hl->rec_size;
        if (bytes > 0) {
            memcpy(p, c->buf, (unsigned)bytes);
            p += bytes;
        }
    }

    // set for main to collect
    *out = all;
    *out_bytes = total_bytes;
}



/************************************************************************************************
 * @brief free HitList
 * @param hl ptr to HitList
 ************************************************************************************************/
void hl_free(
    HitList *hl
) {

    if (!hl) {
        return;
    }

    HitChunk *c = hl->head;

    while (c) {
        HitChunk *nxt = c->next;
        free(c->buf);
        free(c);
        c = nxt;
    }

    free(hl);
}

