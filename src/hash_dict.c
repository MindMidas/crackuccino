#include "hash_dict.h"


/************************************************************************************************
 * @brief node in a hash table linked list
 ************************************************************************************************/
typedef struct Node {
    char *word; // ptr to dynamically allocated lowercase word string
    struct Node *next; // ptr to next node in linked list (NULL if end)
} Node;



/************************************************************************************************
 * @brief hash dictionary struct
 ************************************************************************************************/
struct HashDict {
    int table_size; // total num of buckets in hash table
    int count; // total num of stored words
    Node **buckets; // array of ptrs to linked lists of Node structs
};



/************************************************************************************************
 * @brief lowercase copy of input str
 * @param s ptr to source str
 * @return malloc'd lowercase str or NULL
 ************************************************************************************************/
static char *strdup_lowercase(
    const char *s
) {
    // get len
    int n = strlen(s);

    // malloc
    char *copy = malloc(n + 1);
    if (!copy) {
        return NULL;
    }

    // convert to lowercase
    for (int i = 0; i < n; i++) {
        copy[i] = (char)tolower((unsigned char)s[i]);
    }

    // add EoS
    copy[n] = '\0';

    return copy;
}



/************************************************************************************************
 * @brief case-insensitive string compare
 * @param a ptr to first string
 * @param b ptr to second string
 * @return 1 if equal, 0 otherwise
 ************************************************************************************************/
static int str_cmp(
    const char *a,
    const char *b
) {
    
    // loop until EoS
    while (*a && *b) {

        // converts both chars to lower and match
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        
        // go next
        a++;
        b++;
    }

    // confirm both hit EoS
    return *a == *b;
}



/************************************************************************************************
 * @brief hash string (case-insensitive djb2)
 * @param word ptr to input string
 * @return unsigned long hash value
 * @note credit http://www.cse.yorku.ca/~oz/hash.html
 ************************************************************************************************/
static unsigned long hash_lower(
    const char *word
) {

    unsigned long hash = 5381;
    int c;

    while ((c = (unsigned char)*word++))
        hash = ((hash << 5) + hash) + tolower(c); // hash * 33 + c

    return hash;
}



/************************************************************************************************
 * @brief create a new hash dictionary
 * @param table_size num buckets
 * @return ptr to new HashDict or NULL on failure
 ************************************************************************************************/
HashDict *hd_create(
    int table_size
) {
    
    if (table_size <= 0) {
        return NULL;
    }

    // allocate memory for HashDict struct
    HashDict *dict = calloc(1, sizeof(HashDict));
    if (!dict) {
        // failure 
        return NULL;
    }

    // allocate memory for the array of bucket ptrs
    dict->buckets = calloc(table_size, sizeof(Node *));
    if (!dict->buckets) {
        // failure 
        free(dict);
        return NULL;
    }

    // set values
    dict->table_size = table_size;
    dict->count = 0;

    return dict;
}



/************************************************************************************************
 * @brief free all memory in dictionary
 * @param dict pointer to dictionary
 ************************************************************************************************/
void hd_destroy(HashDict *dict) {

    if (!dict) {
        return;
    }

    // loop through every bucket in hash table
    for (int i = 0; i < dict->table_size; i++) {

        // get head of linked list for bucket
        Node *curr = dict->buckets[i];

        // traverse linked list and free each node
        while (curr) {

            // get ptr of next 
            Node *next = curr->next;

            // free curr
            free(curr->word);
            free(curr);

            // go next
            curr = next;
        }
    }

    // free buckets and HashDict struct
    free(dict->buckets);
    free(dict);
}



/************************************************************************************************
 * @brief insert word into dictionary
 * @param dict ptr to dictionary
 * @param word word to insert
 * @return 1 if inserted, 0 if duplicate, -1 on error
 ************************************************************************************************/
int hd_insert(
    HashDict *dict,
    const char *word
) {

    if (!dict || !word || *word == '\0') {
        return -1;
    }

    // compute hash value of word
    unsigned long hash = hash_lower(word);

    // reduce hash to valid bucket idx with mod
    int idx = (int)(hash % dict->table_size);

    // check if word already exists in linked list
    for (Node *cur = dict->buckets[idx]; cur; cur = cur->next) {
        if (str_cmp(cur->word, word)) {
            return 0;
        }
    }
    
    // allocate new Node for word
    Node *n = malloc(sizeof(Node));
    if (!n) {
        // failure 
        return -1;
    }

    // store lowercase copy of input word
    n->word = strdup_lowercase(word);
    if (!n->word) {
        free(n);
        return -1;
    }

    // insert new node at head of bucket linked list
    n->next = dict->buckets[idx];
    dict->buckets[idx] = n;

    // increment num words for bucket
    dict->count++;

    // success
    return 1;
}



/************************************************************************************************
 * @brief check if word exists in dictionary
 * @param dict ptr to dictionary
 * @param word word to check
 * @return 1 if found, 0 otherwise
 ************************************************************************************************/
int hd_exists(
    const HashDict *dict,
    const char *word
) {

    if (!dict || !word || *word == '\0') {
        return 0;
    }

    // compute hash and idx for word
    unsigned long hash = hash_lower(word);
    int idx = (int)(hash % dict->table_size);

    // traverse linked list at bucket idx
    for (Node *cur = dict->buckets[idx]; cur; cur = cur->next)
        // match found
        if (str_cmp(cur->word, word)) {
            return 1;
        }

    // no match
    return 0;
}



/************************************************************************************************
 * @brief return 1 if every space-separated word in text exists in the hash dict,
 * punctuation inside words (ex: "John's") is allowed
 * @param hd ptr to hash dictionary
 * @param text ptr to decoded plaintext
 * @return 1 if all words are found, else 0
 ************************************************************************************************/
int all_words_in_dict(
    const HashDict *hd,
    const char *text
) {

    if (!hd || !text) {
        return 0;
    }

    char word[HD_LINE_BUFFER];
    int j = 0;
    int saw_word = 0;

    for (int i = 0;; i++) {
        unsigned char c = (unsigned char)text[i];

        // treat non alpha as word boundary, except apostrophe which is allowed inside word
        if (!isalpha((unsigned char)c) && c != '\'') {

            if (j > 0) {

                word[j] = '\0';
                saw_word = 1;

                // word not in hd
                if (!hd_exists(hd, word)) {
                    return 0;
                }

                // reset for next word
                j = 0;
            }

            if (c == '\0') break; // end of string

        // continue reading chars into word
        } else {
            // include any visible characters (letters, punctuation, etc...)
            if (j < (int)sizeof(word) - 1) {

                word[j++] = (char)tolower(c);

            } else {

                // too long for buffer, consider invalid
                return 0;
            }
        }
    }

    // return true at this point if a word was seen
    return saw_word ? 1 : 0;
}





/************************************************************************************************
 * @brief trim leading/trailing spaces from string
 * @param word ptr to string to trim
 * @return ptr to trimmed string
 ************************************************************************************************/
static char *trim(
    char *word
) {

    // move ptr forward while there's leading whitespace chars (' ', '\n', etc...)
    while (isspace((unsigned char)*word)) {
        word++;
    }

    // if string becomes empty after skipping spaces, return
    if (*word == '\0'){
        return word;
    }

    // set end to point to the last char in word (before '\0')
    char *end = word + strlen(word) - 1;

    // move backward while there are trailing whitespace chars (' ', '\n', etc...)
    while (end > word && isspace((unsigned char)*end)) {
        end--;
    }

    // place a EoS one pos after last non-space
    end[1] = '\0';

    return word;
}



/************************************************************************************************
 * @brief load words from a text file
 * @param dict ptr to dictionary
 * @param filename path to text file
 * @return number of inserted words, -1 if file open fails
 ************************************************************************************************/
int hd_load_file(
    HashDict *dict,
    const char *filename
) {

    if (!dict || !filename) {
        return -1;
    }

    // open file for reading
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return -1;
    }

    // set buff and counter
    char buf[HD_LINE_BUFFER];
    int inserted = 0;

    // read line-by-line
    while (fgets(buf, sizeof(buf), fp)) {

        // trim word
        char *word = trim(buf);

        // no word
        if (*word == '\0') {
            continue;
        }

        // try inserting word
        int result = hd_insert(dict, word);

        // increment insert
        if (result == 1) {
            inserted++;
        }
    }

    // close file
    fclose(fp);

    return inserted;
}



/************************************************************************************************
 * @brief get num stored words
 * @param dict ptr to dictionary
 * @return total word count
 ************************************************************************************************/
int hd_size(
    const HashDict *dict
) {
    return dict ? dict->count : 0;
}



/************************************************************************************************
 * @brief print stats for debugging
 * @param dict ptr to dictionary
 * @param out output stream
 ************************************************************************************************/
void hd_stats(
    const HashDict *dict,
    FILE *out
) {
    
    if (!dict || !out) {
        return;
    }

    int used = 0, max_chain = 0;

    for (int i = 0; i < dict->table_size; i++) {
        int chain = 0;

        for (Node *cur = dict->buckets[i]; cur; cur = cur->next) {
            chain++;
        }

        if (chain) {
            used++;
        }

        if (chain > max_chain) {
            max_chain = chain;
        }
    }

    double load = (double)dict->count / (double)dict->table_size;
    fprintf(out, "************************************************************************************************\n");
    fprintf(out, "HD stats:\n");
    fprintf(out, " |-> table size: %d\n", dict->table_size);
    fprintf(out, " |-> word count: %d\n", dict->count);
    fprintf(out, " |-> used buckets: %d\n", used);
    fprintf(out, " |-> max chain length: %d\n", max_chain);
    fprintf(out, " |-> load factor: %.4f\n", load);
    fprintf(out, "************************************************************************************************\n\n");
}



/************************************************************************************************
 * @brief write all words to a text file (one per line)
 * @param dict ptr to dictionary
 * @param out output stream
 * @return number of words written, -1 on error opening file
 ************************************************************************************************/
int hd_dump_file(
    const HashDict *dict,
    FILE *out
) {
    if (!dict || !out) {
        return -1;
    }

    int written = 0;

    // iterate all buckets and their linked lists
    for (int i = 0; i < dict->table_size; i++) {

        for (Node *cur = dict->buckets[i]; cur; cur = cur->next) {
            
            // print in file
            if (fprintf(out, "%s\n", cur->word) < 0) {
                return written ? written : -1;
            }

            written++;
        }
    }

    return written;
}
