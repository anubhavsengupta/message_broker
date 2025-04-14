#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ht_entry {
    char *key;
    char *value;
    struct ht_entry *next;
} ht_entry;

typedef struct hashtable {
    size_t size;
    ht_entry **buckets;
} hashtable;

// create a new hashtable with the given number of buckets
hashtable *ht_create(size_t size);

// destroy the hashtable and free all memory
void ht_destroy(hashtable *ht);

// set a key to a value returns 0 on success, -1 on error
int ht_set(hashtable *ht, const char *key, const char *value);

// get the value for a key ----> returns pointer to value or NULL if not found
char *ht_get(hashtable *ht, const char *key);

#ifdef __cplusplus
}
#endif

#endif
