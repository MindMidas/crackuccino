#include "decrypt_mpi.h"

#define PROGRESS_NUM_FIELDS 5
#define PROGRESS_BAR_WIDTH 28
#define PROGRESS_MAX_UPDATES 80ULL
#define PROGRESS_STATE_WAITING 0ULL
#define PROGRESS_STATE_RUNNING 1ULL
#define PROGRESS_STATE_COMPLETE 2ULL


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
 * @brief count how many prefix tasks belong to one rank
 * @param total_tasks total prefix tasks
 * @param rank current rank
 * @param size total ranks
 * @return num tasks assigned to this rank
 ************************************************************************************************/
static unsigned long long count_rank_tasks(
    unsigned long long total_tasks,
    int rank,
    int size
) {
    unsigned long long base = total_tasks / (unsigned long long)size;
    unsigned long long rem = total_tasks % (unsigned long long)size;

    if ((unsigned long long)rank < rem) {
        return base + 1ULL;
    }

    return base;
}



/************************************************************************************************
 * @brief copy current prefix into output buffer
 * @param out output prefix buffer
 * @param perm current permutation
 * @param len prefix len
 ************************************************************************************************/
static void copy_prefix(
    char *out,
    const char *perm,
    int len
) {
    if (len <= 0) {
        strcpy(out, "*");
        return;
    }

    memcpy(out, perm, (unsigned)len);
    out[len] = '\0';
}



/************************************************************************************************
 * @brief return text label for progress state
 * @param state progress state code
 * @return state label
 ************************************************************************************************/
static const char *progress_state_label(
    unsigned long long state
) {
    if (state == PROGRESS_STATE_RUNNING) {
        return "running";
    }

    if (state == PROGRESS_STATE_COMPLETE) {
        return "done";
    }

    return "waiting";
}



/************************************************************************************************
 * @brief return true if terminal supports clean screen redraws
 * @return true if ANSI redraw should be used
 ************************************************************************************************/
static bool progress_use_ansi(
    void
) {
    const char *plain = getenv("CRACKUCCINO_PROGRESS_PLAIN");

    // Open MPI often forwards rank stdout through a pipe and some launch
    // environments report TERM=dumb, so terminal detection is not reliable
    // here. --progress is a live UI by default. Scripts can force append-only
    // output when they need clean logs.
    if (plain && strcmp(plain, "0") != 0) {
        return false;
    }

    return true;
}



/************************************************************************************************
 * @brief build an ascii progress bar
 * @param out output buffer
 * @param out_size output buffer size
 * @param pct percent complete
 ************************************************************************************************/
static void build_progress_bar(
    char *out,
    size_t out_size,
    double pct
) {
    int filled = (int)((pct / 100.0) * (double)PROGRESS_BAR_WIDTH);

    if (filled < 0) {
        filled = 0;
    }

    if (filled > PROGRESS_BAR_WIDTH) {
        filled = PROGRESS_BAR_WIDTH;
    }

    if (out_size < PROGRESS_BAR_WIDTH + 1u) {
        return;
    }

    for (int i = 0; i < PROGRESS_BAR_WIDTH; i++) {
        if (filled >= PROGRESS_BAR_WIDTH) {
            out[i] = '=';
        } else if (i < filled) {
            out[i] = (i == filled - 1) ? '>' : '=';
        } else {
            out[i] = ' ';
        }
    }

    out[PROGRESS_BAR_WIDTH] = '\0';
}



/************************************************************************************************
 * @brief print the full rank progress table from rank 0
 * @param size total MPI ranks
 * @param nletters num unique letters
 * @param depth prefix depth
 * @param total_tasks total prefix tasks
 * @param start_time start time
 * @param nums progress rows, 5 fields per rank
 * @param prefixes prefix rows, MAX_DICT chars per rank
 ************************************************************************************************/
static void print_progress_table(
    int size,
    int nletters,
    int depth,
    unsigned long long total_tasks,
    struct timeval start_time,
    const unsigned long long *nums,
    const char *prefixes
) {
    static int printed_lines = 0;
    bool ansi = progress_use_ansi();
    int table_lines = size + 5;

    if (ansi) {
        if (printed_lines > 0) {
            printf("\033[%dA\033[J", printed_lines);
        }
    } else {
        printf("\n");
    }

    printf("==> Crackuccino MPI search\n");
    printf("    ranks: %d | unique letters: %d | depth: %d | prefix tasks: %llu | elapsed: %.2fs\n\n",
           size, nletters, depth, total_tasks, calc_time(start_time));
    printf("%-6s %-8s %-11s %-39s %12s %8s  %-8s\n",
           "rank", "prefix", "tasks", "progress", "visited", "hits", "state");
    printf("----------------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < size; i++) {
        const unsigned long long *row = nums + (i * PROGRESS_NUM_FIELDS);
        const char *prefix = prefixes + (i * MAX_DICT);
        char task_summary[64];
        char bar[PROGRESS_BAR_WIDTH + 1];
        char progress_summary[PROGRESS_BAR_WIDTH + 16];
        double pct = 100.0;

        if (row[0] > 0ULL) {
            pct = ((double)row[1] / (double)row[0]) * 100.0;
        }

        build_progress_bar(bar, sizeof(bar), pct);
        snprintf(task_summary, sizeof(task_summary), "%llu/%llu", row[1], row[0]);
        snprintf(progress_summary, sizeof(progress_summary), "[%s] %6.2f%%", bar, pct);

        printf("%-6d %-8s %-11s %-39s %12llu %8llu  %-8s\n",
               i, prefix[0] ? prefix : "-", task_summary, progress_summary,
               row[2], row[3], progress_state_label(row[4]));
    }

    if (ansi) {
        printed_lines = table_lines;
    }

    fflush(stdout);
}



/************************************************************************************************
 * @brief gather one progress snapshot from all ranks and print it from rank 0
 * @param rank current MPI rank
 * @param size total MPI ranks
 * @param show_progress true if progress mode is enabled
 * @param nletters num unique letters
 * @param depth prefix depth
 * @param total_tasks total prefix tasks
 * @param prefix current rank prefix
 * @param assigned assigned tasks for current rank
 * @param completed completed tasks for current rank
 * @param visited visited permutations for current rank
 * @param hits valid hits for current rank
 * @param state current rank state
 * @param start_time start time
 ************************************************************************************************/
static void mpi_progress_snapshot(
    int rank,
    int size,
    bool show_progress,
    int nletters,
    int depth,
    unsigned long long total_tasks,
    const char *prefix,
    unsigned long long assigned,
    unsigned long long completed,
    unsigned long long visited,
    unsigned long long hits,
    unsigned long long state,
    struct timeval start_time
) {
    if (!show_progress) {
        return;
    }

    unsigned long long send_nums[PROGRESS_NUM_FIELDS] = {
        assigned,
        completed,
        visited,
        hits,
        state
    };

    char send_prefix[MAX_DICT];
    snprintf(send_prefix, sizeof(send_prefix), "%s", prefix ? prefix : "-");

    unsigned long long *recv_nums = NULL;
    char *recv_prefixes = NULL;

    if (rank == 0) {
        recv_nums = (unsigned long long *)malloc(sizeof(unsigned long long) * PROGRESS_NUM_FIELDS * (unsigned)size);
        recv_prefixes = (char *)malloc((unsigned)size * MAX_DICT);

        if (!recv_nums || !recv_prefixes) {
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Gather(send_nums, PROGRESS_NUM_FIELDS, MPI_UNSIGNED_LONG_LONG,
               recv_nums, PROGRESS_NUM_FIELDS, MPI_UNSIGNED_LONG_LONG,
               0, MPI_COMM_WORLD);
    MPI_Gather(send_prefix, MAX_DICT, MPI_CHAR,
               recv_prefixes, MAX_DICT, MPI_CHAR,
               0, MPI_COMM_WORLD);

    if (rank == 0) {
        print_progress_table(size, nletters, depth, total_tasks, start_time, recv_nums, recv_prefixes);
        free(recv_nums);
        free(recv_prefixes);
    }
}



/************************************************************************************************
 * @brief print MPI decrypt usage
 * @param prog program name
 ************************************************************************************************/
static void print_mpi_usage(
    const char *prog
) {
    fprintf(stderr, "Usage: mpirun -np <ranks> %s <ciphertext.txt> <dictionary> [-s|--stats] [--progress] [-d <depth>]\n", prog);
    fprintf(stderr, "       %s -h | --help\n", prog);
    fprintf(stderr, "Dictionary files are plain newline-separated word lists.\n");
    fprintf(stderr, "--progress redraws a clean rank table with tasks, percent, visited permutations, and hits.\n");
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
 * @param completed_tasks completed prefix tasks by this rank
 * @param assigned_tasks total prefix tasks assigned to this rank
 * @param total_tasks total prefix tasks
 * @param progress_every_tasks progress snapshot interval
 * @param rank_prefix current or last prefix for this rank
 * @param rank_state current state for this rank
 * @param start_time start time
 * @param show_progress true to print terminal progress
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
    unsigned long long *completed_tasks,
    unsigned long long assigned_tasks,
    unsigned long long total_tasks,
    unsigned long long progress_every_tasks,
    char *rank_prefix,
    unsigned long long *rank_state,
    struct timeval start_time,
    bool show_progress
) {
    // if target = 0, whole perm space is one task
    if (target == 0) {
        unsigned long long id = (*task_id)++;
        bool owned = ((id % (unsigned long long)size) == (unsigned long long)rank);
        bool show_task = show_progress &&
                         (id == 0ULL || (id % progress_every_tasks) == 0ULL || (id + 1ULL) == total_tasks);

        if (owned) {
            strcpy(rank_prefix, "*");
            *rank_state = PROGRESS_STATE_RUNNING;
        }

        if (show_task) {
            mpi_progress_snapshot(rank, size, show_progress, n_letters, target, total_tasks,
                                  rank_prefix, assigned_tasks, *completed_tasks,
                                  *local_visited, *local_hits, *rank_state,
                                  start_time);
        }

        if (owned) {
            permute(perm, 0, n_letters - 1, ciphertext, input_dict, hd,
                    decoded, local_visited, local_hits, rank, hits);

            (*completed_tasks)++;
            *rank_state = (*completed_tasks >= assigned_tasks) ? PROGRESS_STATE_COMPLETE : PROGRESS_STATE_WAITING;
        }

        if (show_task) {
            mpi_progress_snapshot(rank, size, show_progress, n_letters, target, total_tasks,
                                  rank_prefix, assigned_tasks, *completed_tasks,
                                  *local_visited, *local_hits, *rank_state,
                                  start_time);
        }
        return;
    }

    // if we reached prefix depth, issue this task
    if (pos == target) {
        unsigned long long id = (*task_id)++;
        bool owned = ((id % (unsigned long long)size) == (unsigned long long)rank);
        bool show_task = show_progress &&
                         (id == 0ULL || (id % progress_every_tasks) == 0ULL || (id + 1ULL) == total_tasks);
        char prefix[MAX_DICT];
        copy_prefix(prefix, perm, target);

        if (owned) {
            strcpy(rank_prefix, prefix);
            *rank_state = PROGRESS_STATE_RUNNING;
        }

        if (show_task) {
            mpi_progress_snapshot(rank, size, show_progress, n_letters, target, total_tasks,
                                  rank_prefix, assigned_tasks, *completed_tasks,
                                  *local_visited, *local_hits, *rank_state,
                                  start_time);
        }

        if (owned) {
            permute(perm, target, n_letters - 1, ciphertext, input_dict, hd,
                    decoded, local_visited, local_hits, rank, hits);

            (*completed_tasks)++;
            *rank_state = (*completed_tasks >= assigned_tasks) ? PROGRESS_STATE_COMPLETE : PROGRESS_STATE_WAITING;
        }

        if (show_task) {
            mpi_progress_snapshot(rank, size, show_progress, n_letters, target, total_tasks,
                                  rank_prefix, assigned_tasks, *completed_tasks,
                                  *local_visited, *local_hits, *rank_state,
                                  start_time);
        }
        return;
    }

    // otherwise, keep fixing next letters to build prefixes
    for (int i = pos; i < n_letters; i++) {
        swap(&perm[pos], &perm[i]);  // fix one letter
        enumerate_prefix_tasks(perm, pos + 1, target, n_letters,
                               ciphertext, input_dict, hd, decoded,
                               local_visited, local_hits,
                               rank, size, hits, task_id,
                               completed_tasks, assigned_tasks, total_tasks,
                               progress_every_tasks, rank_prefix, rank_state,
                               start_time, show_progress);
        swap(&perm[pos], &perm[i]);  // undo for backtracking
    }
}



/************************************************************************************************
 * @brief program entry: MPI parallel decrypt (partition by fixing first letter per rank)
 * @param argc argument count (expects 3)
 * @param argv argv[1]=ciphertext path, argv[2]=dictionary path, optional flags:
 * [-s|--stats], [--progress], [-d <depth>]
 ************************************************************************************************/
int main(int argc, char **argv) {

    MPI_Init(&argc, &argv); // start up MPI

    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get curr rank among processes
    MPI_Comm_size(MPI_COMM_WORLD, &size); // get num processes

    // start timer
    struct timeval start_time;
    time_now(&start_time);

    // args: <ciphertext.txt> <dictionary> [ -s | --stats ] [ --progress ] [ -d <depth> ]
    bool show_stats = false;
    bool show_progress = false;
    int depth_override = -1; // <0 means "auto"
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        if (rank == 0) {
            print_mpi_usage(argv[0]);
        }
        MPI_Finalize();
        return EXIT_SUCCESS;
    }

    if (argc < 3) {
        if (rank == 0) {
            print_mpi_usage(argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    // parse optional flags from argv[3..]
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stats") == 0) {
            show_stats = true;
        } else if (strcmp(argv[i], "--progress") == 0) {
            show_progress = true;
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
        } else {
            if (rank == 0) {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                print_mpi_usage(argv[0]);
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

            // make more smaller chunks in progress mode so the table updates often
            const unsigned CHUNK_MULT = show_progress ? 64u : 8u;
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
    unsigned long long total_tasks = (nletters == 0) ? 1ULL : perm_count_prefix(nletters, target);
    unsigned long long local_assigned_tasks = count_rank_tasks(total_tasks, rank, size);
    unsigned long long local_completed_tasks = 0ULL;
    unsigned long long progress_every_tasks = 1ULL;
    char rank_prefix[MAX_DICT];
    unsigned long long rank_state = PROGRESS_STATE_WAITING;

    strcpy(rank_prefix, "-");

    if (local_assigned_tasks == 0ULL) {
        rank_state = PROGRESS_STATE_COMPLETE;
    }

    if (total_tasks > PROGRESS_MAX_UPDATES) {
        progress_every_tasks = total_tasks / PROGRESS_MAX_UPDATES;

        if (progress_every_tasks < 1ULL) {
            progress_every_tasks = 1ULL;
        }
    }

    // base case: empty plaintext
    if (nletters == 0) {
        if (rank == 0) {
            strcpy(rank_prefix, "*");
            rank_state = PROGRESS_STATE_RUNNING;
        }

        mpi_progress_snapshot(rank, size, show_progress, nletters, target, total_tasks,
                              rank_prefix, local_assigned_tasks, local_completed_tasks,
                              local_visited, local_hits, rank_state,
                              start_time);

        if (rank == 0) {
            local_visited = 1ULL;
            local_hits = 1ULL;
            local_completed_tasks = 1ULL;
            rank_state = PROGRESS_STATE_COMPLETE;
            hl_push(hits, 0, "", ciphertext);
        } else {
            local_visited = 0ULL;
            local_hits = 0ULL;
        }

        mpi_progress_snapshot(rank, size, show_progress, nletters, target, total_tasks,
                              rank_prefix, local_assigned_tasks, local_completed_tasks,
                              local_visited, local_hits, rank_state,
                              start_time);

    } else {
        // copy perm into input_dict
        strcpy(permutation, input_dict);

        // enumerate prefix tasks of depth target and split by mod
        unsigned long long task_id = 0ULL; // shared ordering across the enumeration

        enumerate_prefix_tasks(permutation, 0, target, nletters, ciphertext, 
                                input_dict, hd, decoded, &local_visited, 
                                &local_hits, rank, size, hits, &task_id,
                                &local_completed_tasks, local_assigned_tasks,
                                total_tasks, progress_every_tasks,
                                rank_prefix, &rank_state,
                                start_time, show_progress);
    }

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
        if (show_progress) {
            printf("\nvalid solutions:\n");
        }

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
