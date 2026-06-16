#include "decrypt_mpi.h"

static const char *progress_dir = NULL;
static const unsigned long long PROGRESS_WRITE_INTERVAL = 65536ULL;


typedef struct RankProgressContext {
    int rank;
    const char *prefix;
    const unsigned long long *completed_tasks;
    unsigned long long last_visited;
    unsigned long long last_hits;
} RankProgressContext;


/************************************************************************************************
 * @brief write one rank progress snapshot for the web API
 * @param rank current MPI rank
 * @param state queued, running, or complete
 * @param prefix current prefix task
 * @param visited permutations visited by this rank
 * @param hits valid hits found by this rank
 * @param completed_tasks prefix tasks completed by this rank
 ************************************************************************************************/
static void write_rank_progress(
    int rank,
    const char *state,
    const char *prefix,
    const char *sample_permutation,
    unsigned long long visited,
    unsigned long long hits,
    unsigned long long completed_tasks
) {
    if (!progress_dir) {
        return;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/rank_%d.json", progress_dir, rank);

    FILE *fp = fopen(path, "w");
    if (!fp) {
        return;
    }

    fprintf(
        fp,
        "{\"rank\":%d,\"state\":\"%s\",\"prefix\":\"%s\","
        "\"samplePermutation\":\"%s\",\"permutations\":%llu,"
        "\"hits\":%llu,\"completedTasks\":%llu}\n",
        rank, state, prefix, sample_permutation, visited, hits, completed_tasks
    );
    fclose(fp);
}


static void append_rank_trace(
    int rank,
    const char *prefix,
    const char *sample_permutation,
    unsigned long long visited
) {
    if (!progress_dir || !sample_permutation || sample_permutation[0] == '\0') {
        return;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/rank_%d_trace.jsonl", progress_dir, rank);

    FILE *fp = fopen(path, "a");
    if (!fp) {
        return;
    }

    fprintf(
        fp,
        "{\"permutation\":\"%s\",\"visited\":%llu,\"prefix\":\"%s\"}\n",
        sample_permutation, visited, prefix
    );
    fclose(fp);
}


/************************************************************************************************
 * @brief publish throttled progress from the inner permutation loop
 * @param context RankProgressContext for the current prefix task
 * @param visited current visited permutations counter
 * @param hits current valid hits counter
 ************************************************************************************************/
static void publish_inner_progress(
    void *context,
    unsigned long long visited,
    unsigned long long hits,
    const char *permutation
) {
    RankProgressContext *progress = (RankProgressContext *)context;
    if (!progress) {
        return;
    }

    bool enough_work = visited >= progress->last_visited
        && visited - progress->last_visited >= PROGRESS_WRITE_INTERVAL;
    bool hit_changed = hits != progress->last_hits;
    if (!enough_work && !hit_changed) {
        return;
    }

    write_rank_progress(
        progress->rank,
        "running",
        progress->prefix,
        permutation,
        visited,
        hits,
        *progress->completed_tasks
    );
    append_rank_trace(progress->rank, progress->prefix, permutation, visited);
    progress->last_visited = visited;
    progress->last_hits = hits;
}


/************************************************************************************************
 * @brief broadcast a NUL-terminated C string from root to all ranks (alloc on receivers)
 * @param root root rank
 * @param comm MPI communicator
 * @param buf_inout on root: ptr to valid buffer; on others: will be malloc'd
 * @return 0 on success, non-zero on failure (non-root failures call MPI_Abort)
 ************************************************************************************************/
int mpi_bcast_cstring(
    int root,
    MPI_Comm comm,
    char **buf_inout
) {

    // ask MPI what rank this is
    int rank;
    MPI_Comm_rank(comm, &rank);

    // root computes ciphertext size (includes '\0' so receivers get full C string)
    int nbytes = 0;
    if (rank == root) {
        nbytes = (int)strlen(*buf_inout) + 1;
    }


    // broadcast size to everyone
    MPI_Bcast(&nbytes, 1, MPI_INT, root, comm);

    // size invalid
    if (nbytes <= 0) {
        return -1;
    }

    // non-root ranks allocate a buffer of appropriate size
    if (rank != root) {

        *buf_inout = (char *)malloc((int)nbytes);

        // if allocation fails on any rank, abort full MPI job
        if (!*buf_inout) {
            MPI_Abort(comm, 1);
        }
    }

    // broadcast actual bytes (including '\0')
    MPI_Bcast(*buf_inout, nbytes, MPI_CHAR, root, comm);

    return 0;
}



/************************************************************************************************
 * @brief compute num of ordered permutations (prefix combinations) of length d from a set of 
 * n unique elements. It just counts how many ways you can pick and arrange the first d letters 
 * out of n, which helps estimate how many prefix tasks will exist for MPI work distribution.
 * @param n total num available elements (e.g., num of unique letters).
 * @param d len of prefix (num of fixed elements in permutation).
 * @return num ordered prefixes (P(n, d)) as an unsigned long long.
 ************************************************************************************************/
static unsigned long long perm_count_prefix(
    int n,
    int d
) {
    // P(n, d) = n * (n-1) * ... * (n-d+1), with P(n,0)=1
    if (d <= 0) {
        return 1ULL;
    }

    unsigned long long p = 1ULL;

    for (int i = 0; i < d; i++) {
        p *= (unsigned long long)(n - i);
    }

    return p;
}



/************************************************************************************************
 * @brief split permutation work across MPI ranks by fixing a prefix of letters.
 * This function recursively builds all possible prefixes up to a chosen depth (target).
 * Each unique prefix becomes one "task" with an increasing task_id. Tasks are given to ranks
 * using (task_id % size == rank). When a rank owns a task, it calls permute() to finish
 * the remaining part of the permutation.
 *
 * @param perm curr perm str
 * @param pos curr pos being fixed
 * @param target prefix depth before splitting
 * @param n_letters num letters in input_dict
 * @param ciphertext ciphertext str
 * @param input_dict unique letters from ciphertext
 * @param hd hash dict
 * @param decoded buffer for decoded plaintext
 * @param local_visited counter for visited permutations
 * @param local_hits counter for valid hits
 * @param rank current MPI rank
 * @param size total MPI ranks
 * @param hits HitList collector
 * @param task_id global task counter
 ************************************************************************************************/
static void enumerate_prefix_tasks(
    char *perm,
    int pos,
    int target,
    int n_letters,
    const char *ciphertext,
    const char *input_dict,
    const HashDict *hd,
    char *decoded,
    unsigned long long *local_visited,
    unsigned long long *local_hits,
    int rank,
    int size,
    HitList *hits,
    unsigned long long *task_id,
    unsigned long long *completed_tasks
) {
    // if target = 0, whole perm space is one task
    if (target == 0) {
        unsigned long long id = (*task_id)++;
        if ((id % (unsigned long long)size) == (unsigned long long)rank) {
            write_rank_progress(rank, "running", "*", "", *local_visited, *local_hits, *completed_tasks);
            RankProgressContext progress = {
                rank, "*", completed_tasks, *local_visited, *local_hits
            };
            permute(perm, 0, n_letters - 1, ciphertext, input_dict, hd,
                    decoded, local_visited, local_hits, rank, hits,
                    publish_inner_progress, &progress);
            (*completed_tasks)++;
            write_rank_progress(rank, "running", "*", "", *local_visited, *local_hits, *completed_tasks);
        }
        return;
    }

    // if we reached prefix depth, issue this task
    if (pos == target) {
        unsigned long long id = (*task_id)++;
        if ((id % (unsigned long long)size) == (unsigned long long)rank) {
            char prefix[MAX_DICT];
            memcpy(prefix, perm, (unsigned)target);
            prefix[target] = '\0';
            write_rank_progress(rank, "running", prefix, "", *local_visited, *local_hits, *completed_tasks);
            RankProgressContext progress = {
                rank, prefix, completed_tasks, *local_visited, *local_hits
            };
            permute(perm, target, n_letters - 1, ciphertext, input_dict, hd,
                    decoded, local_visited, local_hits, rank, hits,
                    publish_inner_progress, &progress);
            (*completed_tasks)++;
            write_rank_progress(rank, "running", prefix, "", *local_visited, *local_hits, *completed_tasks);
        }
        return;
    }

    // otherwise, keep fixing next letters to build prefixes
    for (int i = pos; i < n_letters; i++) {
        swap(&perm[pos], &perm[i]);  // fix one letter
        enumerate_prefix_tasks(perm, pos + 1, target, n_letters,
                               ciphertext, input_dict, hd, decoded,
                               local_visited, local_hits,
                               rank, size, hits, task_id, completed_tasks);
        swap(&perm[pos], &perm[i]);  // undo for backtracking
    }
}



/************************************************************************************************
 * @brief program entry: MPI parallel decrypt (partition by fixing first letter per rank)
 * @param argc argument count (expects 3)
 * @param argv argv[1]=ciphertext path, argv[2]=dictionary path, optional argv[3]=[-s|--stats],
 * optional argv[4]=[-d <depth>]
 ************************************************************************************************/
int main(int argc, char **argv) {

    MPI_Init(&argc, &argv); // start up MPI

    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get curr rank among processes
    MPI_Comm_size(MPI_COMM_WORLD, &size); // get num processes

    // start timer
    struct timeval start_time;
    time_now(&start_time);

    // args: <ciphertext.txt> <dictionary> [ -s | --stats ] [ -d <depth> ]
    bool show_stats = false;
    int depth_override = -1; // <0 means "auto"
    if (argc < 3) {
        if (rank == 0) {
            fprintf(stderr, "Usage: %s <ciphertext.txt> <dictionary> [-s|--stats] [-d <depth>]\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    // parse optional flags from argv[3..]
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stats") == 0) {
            show_stats = true;
        } else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                if (rank == 0) fprintf(stderr, "error: -d requires an integer argument\n");
                MPI_Finalize();
                return EXIT_FAILURE;
            }
            depth_override = atoi(argv[++i]);
            if (depth_override < 1) {
                if (rank == 0) fprintf(stderr, "error: -d <depth> must be >= 1\n");
                MPI_Finalize();
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--progress-dir") == 0) {
            if (i + 1 >= argc) {
                if (rank == 0) fprintf(stderr, "error: --progress-dir requires a path\n");
                MPI_Finalize();
                return EXIT_FAILURE;
            }
            progress_dir = argv[++i];
        } else {
            if (rank == 0) {
                fprintf(stderr, "Unknown option: %s\nUsage: %s <ciphertext.txt> <dictionary> [-s|--stats] [-d <depth>]\n",
                        argv[i], argv[0]);
            }
            MPI_Finalize();
            return EXIT_FAILURE;
        }
    }


    // rank 0: read ciphertext
    char *ciphertext = NULL;
    if (rank == 0) {
        ciphertext = read_ciphertext(argv[1], NULL);
        if (!ciphertext) { 
            fprintf(stderr, "ciphertext file open failed\n"); 
            MPI_Abort(MPI_COMM_WORLD, 1); 
        }
    }

    // rank 0: broadcast ciphertext
    mpi_bcast_cstring(0, MPI_COMM_WORLD, &ciphertext);


    // rank 0...n: build input_dict
    char input_dict[MAX_DICT];
    build_input_dict(ciphertext, input_dict);
    int nletters = (int)strlen(input_dict);

    // rank 0..n: decide target depth
    int target = 0;
    if (nletters > 0) {
        // -d flag given
        if (depth_override > 0) {

            target = depth_override;
            if (target >= nletters) {
                target = nletters - 1; 
            }
        
        // -d flag not given -> auto
        } else {

            // make at least size*CHUNK_MULT chunks
            const unsigned CHUNK_MULT = 8; // tune for more smaller chunks
            target = 1;
            unsigned long long ways = perm_count_prefix(nletters, target);

            // decides how deep (how many leading letters) we should fix before splitting work across ranks
            while (ways < (unsigned long long)size * CHUNK_MULT && target < nletters) {
                target++;
                ways = perm_count_prefix(nletters, target);
            }

            // clamp incase
            if (target >= nletters) {
                target = nletters - 1;
            }
        }
    }

    // final check for both -d and AUTO branches
    // prevents target == nletters (yields no work)
    if (nletters > 0 && target >= nletters) {
        target = nletters - 1;
    }

    // rank 0...n: build hitlist data struct to store valid hits
    int L = (int)strlen(ciphertext);
    HitList *hits = hl_create(nletters, L, 20);


    // rank 0...n: load hd locally
    HashDict *hd = hd_create(HD_SIZE);
    if (!hd) { 
        free(ciphertext); 
        MPI_Abort(MPI_COMM_WORLD, 1); 
    }

    // rank 0...n: read dictionary file into hd
    int inserted = hd_load_file(hd, argv[2]);
    if (inserted < 0) {
        // failed insert
        fprintf(stderr, "[rank %d] failed to open/load dictionary: %s\n", rank, argv[2]);
        hd_destroy(hd);
        free(ciphertext);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // rank 0...n: prep local permutation and decoded buffer
    char permutation[MAX_DICT];
    strcpy(permutation, input_dict);

    // rank 0...n: allocate reusable decoded buffer
    char *decoded = (char *)malloc((int)strlen(ciphertext) + 1);
    if (!decoded) {
        hd_destroy(hd);
        free(ciphertext);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }


    // rank 0...n: run permutation search  
    unsigned long long local_visited = 0ULL;
    unsigned long long local_hits = 0ULL;
    unsigned long long completed_tasks = 0ULL;
    write_rank_progress(rank, "queued", "", "", 0ULL, 0ULL, 0ULL);

    // base case: empty plaintext
    if (nletters == 0) {

        if (rank == 0) {
            local_visited = 1ULL;
            local_hits = 1ULL;
            hl_push(hits, 0, "", ciphertext);
        } else {
            local_visited = 0ULL;
            local_hits = 0ULL;
        }

    } else {
        // copy perm into input_dict
        strcpy(permutation, input_dict);

        // enumerate prefix tasks of depth target and split by mod
        unsigned long long task_id = 0ULL; // shared ordering across the enumeration

        enumerate_prefix_tasks(permutation, 0, target, nletters, ciphertext, 
                                input_dict, hd, decoded, &local_visited, 
                                &local_hits, rank, size, hits, &task_id, &completed_tasks);
    }

    write_rank_progress(rank, "complete", "", "", local_visited, local_hits, completed_tasks);

    // rank 0...n: reduce visited and hit counts to rank 0
    unsigned long long total_visited = 0ULL;
    unsigned long long total_hits = 0ULL;
    MPI_Reduce(&local_visited, &total_visited, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_hits, &total_hits, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);


    // rank 0...n: flatten per-rank hits and gather to rank 0 
    char *send_buf = NULL;
    int send_bytes = 0;
    hl_flatten(hits, &send_buf, &send_bytes);

    // rank 0...n: sends sizes so rank 0 can gather sizes 
    int *sizes = NULL, *displacement = NULL;
    if (rank == 0) {
        sizes = (int*)malloc(sizeof(int) * size);
    }
    MPI_Gather(&send_bytes, 1, MPI_INT, sizes, 1, MPI_INT, 0, MPI_COMM_WORLD);


    // rank 0: malloc recv buffer and calculate displacement
    char *recv_buf = NULL;
    int total_bytes = 0;

    if (rank == 0) {

        displacement = (int*)malloc(sizeof(int) * size);
        int offset = 0;

        // loop over ranks adding sizes to displacement so we know how to print
        for (int i = 0; i < size; i++) { 
            displacement[i] = offset; 
            offset += sizes[i];
        }

        total_bytes = offset;

        // malloc total or 1 byte incase
        recv_buf = (char*)malloc(total_bytes > 0 ? (unsigned)total_bytes : 1u);
    }

    // rank 0...n: sends send_buf of send_bytes size
    // rank 0: recieves data in recv_buf, uses sizes + displacement for placement 
    MPI_Gatherv(send_buf, send_bytes, MPI_BYTE, recv_buf, sizes, displacement, MPI_BYTE, 0, MPI_COMM_WORLD);

    // stop timer
    double elapsed = calc_time(start_time);

    // rank 0: print valid hits and summary
    if (rank == 0) {

        // print valid hits
        int rec_size = hl_record_size(nletters, L); // [rank][permutation[nletters+1]][plaintext[L+1]]

        // loop over outputs jumping rec_size each time
        for (int curr = 0; curr < total_bytes; curr += rec_size) {
            
            // ptr to start of curr record
            const char *rec = recv_buf + curr;

            // read the sender rank
            int src_rank; 
            memcpy(&src_rank, rec, sizeof(int));

            // read permutation and plaintext
            const char *perm = rec + sizeof(int);
            const char *plain = perm + (nletters + 1);

            // print
            printf("[rank %d] [permutation: %s] found: %s\n", src_rank, perm, plain);
        }

        // print summary
        if (show_stats) {
            print_permutations_summary("mpi", size, nletters, target, total_visited, total_hits, elapsed);
        }
    }

    // cleanup
    if (rank == 0) {
        free(sizes);
        free(displacement);
        free(recv_buf);
    }
    free(send_buf);
    hl_free(hits);
    free(decoded);
    hd_destroy(hd);
    free(ciphertext);

    MPI_Finalize();

    return EXIT_SUCCESS;
}
