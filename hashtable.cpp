#include "hashtable.h"
#include <stdlib.h>
#include <string.h>

// simple djb2 hash function
static unsigned long hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

hashtable *ht_create(size_t size) {
    hashtable *ht = (hashtable*)malloc(sizeof(hashtable));
    if (!ht) return NULL;
    ht->size = size;
    ht->buckets = (ht_entry**)malloc(sizeof(ht_entry*) * size);
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    for (size_t i = 0; i < size; i++) {
        ht->buckets[i] = NULL;
    }
    return ht;
}

void ht_destroy(hashtable *ht) {
    if (!ht) return;
    for (size_t i = 0; i < ht->size; i++) {
        ht_entry *entry = ht->buckets[i];
        while (entry) {
            ht_entry *next = entry->next;
            free(entry->key);
            free(entry->value);
            free(entry);
            entry = next;
        }
    }
    free(ht->buckets);
    free(ht);
}

int ht_set(hashtable *ht, const char *key, const char *value) {
    if (!ht) return -1;
    unsigned long h = hash(key) % ht->size;
    ht_entry *entry = ht->buckets[h];
    // if the key exists, update the value
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            char *new_value = strdup(value);
            if (!new_value) return -1;
            free(entry->value);
            entry->value = new_value;
            return 0;
        }
        entry = entry->next;
    }
    // key doesnnt exist, add new entry
    entry = (ht_entry*)malloc(sizeof(ht_entry));
    if (!entry) return -1;
    entry->key = strdup(key);
    entry->value = strdup(value);
    if (!entry->key || !entry->value) {
        free(entry->key);
        free(entry->value);
        free(entry);
        return -1;
    }
    entry->next = ht->buckets[h];
    ht->buckets[h] = entry;
    return 0;
}

char *ht_get(hashtable *ht, const char *key) {
    if (!ht) return NULL;
    unsigned long h = hash(key) % ht->size;
    ht_entry *entry = ht->buckets[h];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}
