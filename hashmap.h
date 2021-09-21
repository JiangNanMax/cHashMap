//
// Copyright (c) 2021 JiangNanMax. All rights reserved.
//

#ifndef CHASHMAP_HASHMAP_H
#define CHASHMAP_HASHMAP_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

struct bucket;

struct hashmap;

struct iterator;

struct hashmap *chm_new(size_t elementSize, size_t cap,
                        uint64_t seed0, uint64_t seed1,
                        uint64_t (*hash)(const void *data, size_t len,
                                         uint64_t seed0, uint64_t seed1),
                        int (*compare)(const void *a, const void *b));

void *chm_put(struct hashmap *map, void *item);

void *chm_get(struct hashmap *map, void *item);

void *chm_delete(struct hashmap *map, void *item);

void *chm_probe(struct hashmap *map, size_t pos);

size_t chm_count(struct hashmap *map);

bool shm_oom(struct hashmap *map);

void chm_clear(struct hashmap *map);

void chm_free(struct hashmap *map);

uint64_t sip_hash(const void *data, size_t len,
                  uint64_t seed0, uint64_t seed1);

uint64_t murmur_hash(const void *data, size_t len,
                     uint64_t seed0, uint64_t seed1);

#endif //CHASHMAP_HASHMAP_H
