//
// Copyright (c) 2021 JiangNanMax. All rights reserved.
//

#include "hashmap.h"

#define panic(msg) { \
    fprintf(stderr, "panic: %s (%s:%d).\n", msg, __FILE__, __LINE__); \
    exit(1); \
}

struct bucket {
    uint64_t hash;
    bool used;
};

struct hashmap {
    size_t elementSize;     // 用户自定义元素大小
    size_t bucketSize;      // 桶大小 bucket {meta{hash, used} + user_define_data}
    size_t capacity;        // 容量
    size_t count;           // 当前数量
    size_t mask;            // 取模用
    size_t growAt;          // 扩容阈值
    size_t shrinkAt;        // 缩容阈值
    bool oom;               // if out-of-memory
    void *buckets;          // data
    uint64_t (*hash)(const void *item, size_t len, uint64_t seed0, uint64_t seed1);
    uint64_t seed0;
    uint64_t seed1;
    int (*compare)(const void *a, const void *b);
};

struct iterator {
    size_t index;   // index of next bucket to visit
    size_t visited;
    bool hasNext;
};

struct hashmap *chm_new(size_t elementSize, size_t cap,
                        uint64_t seed0, uint64_t seed1,
                        uint64_t (*hash)(const void *data, size_t len,
                                         uint64_t seed0, uint64_t seed1),
                        int (*compare)(const void *a, const void *b)) {
    struct hashmap *map = malloc(sizeof(struct hashmap));
    if (!map) {
//        panic("apply memory for map fail")
        return NULL;
    }
    int defaultCap = 16;
    while (defaultCap < cap) {
        defaultCap *= 2;
    }
    cap = defaultCap;
    memset(map, 0, sizeof(struct hashmap));
    map->elementSize = elementSize;
    // 相比于他的bucket size，少了求整数倍的那一步，留着作为对比
    map->bucketSize = sizeof(struct bucket) + elementSize;
    map->capacity = cap;
    map->mask = cap - 1;
    map->growAt = cap * 0.75;
    map->shrinkAt = cap * 0.1;
    map->hash = hash;
    map->seed0 = seed0;
    map->seed1 = seed1;
    map->compare = compare;
    map->buckets = malloc(map->bucketSize * cap);
    if (!map->buckets) {
        chm_free(map);
        return NULL;
    }
    memset(map->buckets, 0, map->bucketSize * cap);
    return map;
}

void chm_free(struct hashmap *map) {
    free(map->buckets);
    free(map);
}

void chm_clear(struct hashmap *map) {
    memset(map->buckets, 0, map->bucketSize * map->capacity);
    map->count = 0;
}

size_t chm_count(struct hashmap *map) {
    return map->count;
}

bool chm_oom(struct hashmap *map) {
    return map->oom;
}

static struct bucket *bucket_at(struct hashmap *map, size_t index) {
    return (struct bucket*)((char*)map->buckets + map->bucketSize * index);
}

static void *bucket_item(struct bucket *entry) {
    return (char*)entry + sizeof(struct bucket);
}

static bool resize(struct hashmap *map, size_t newCap) {
    struct bucket *newBuckets = malloc(map->bucketSize * newCap);
    if (!newBuckets) {
        return false;
    }
    memset(newBuckets, 0, map->bucketSize * newCap);
    size_t newMask = newCap - 1;
    for (size_t i = 0; i < map->capacity; i++) {
        struct bucket *entry = bucket_at(map, i);
        if (!entry->used) {
            continue;
        }
        size_t index = entry->hash & newMask;
        for (;;) {
            struct bucket *slot = (struct bucket *)((char *) newBuckets + map->bucketSize * index);
            if (!slot->used) {
                memcpy(slot, entry, map->bucketSize);
                break;
            }
            index = (index + 1) & newMask;
        }
    }
    free(map->buckets);
    map->buckets = newBuckets;
    map->capacity = newCap;
    map->mask = newMask;
    map->growAt = newCap * 0.75;
    map->shrinkAt = newCap * 0.1;
    return true;
}

static uint64_t get_hash(struct hashmap *map, void *item) {
    return map->hash(item, map->elementSize, map->seed0, map->seed1);
}

void *chm_put(struct hashmap *map, void *item) {
    if (!item) {
        panic("ite is null")
    }
    map->oom = false;
    if (map->count >= map->growAt) {
        if (!resize(map, map->capacity * 2)) {
            map->oom = true;
            return NULL;
        }
    }
    struct bucket *entry = malloc(map->bucketSize);
    memset(entry, 0, map->bucketSize);
    entry->hash = get_hash(map, item);
    entry->used = true;
    memcpy(bucket_item(entry), item, map->elementSize);
    size_t index = entry->hash & map->mask;
    for (;;) {
        struct bucket *slot = bucket_at(map, index);
        if (slot->used) {
            // 出现值覆盖的话返回旧值
            if (map->compare(item, bucket_item(slot)) == 0) {
                // 这里可以只copy item
                void *oldItem = bucket_item(slot);
                memcpy(slot, entry, map->bucketSize);
                return oldItem;
            }
        } else {
            memcpy(slot, entry, map->bucketSize);
            map->count++;
            break;
        }
        index = (index + 1) & map->mask;
    }
    return NULL;
}

void *chm_get(struct hashmap *map, void *item) {
    if (!item) {
        panic("item is null")
    }
    uint64_t hash = get_hash(map, item);
    size_t index = hash & map->mask;
    for (;;) {
        struct bucket *entry = bucket_at(map, index);
        if (entry->used) {
            if (map->compare(item, bucket_item(entry)) == 0) {
                return bucket_item(entry);
            }
        } else {
            return NULL;
        }
        index = (index + 1) & map->mask;
    }
}

void *chm_delete(struct hashmap *map, void *item) {
    if (!item) {
        panic("item is null")
    }
    uint64_t hash = get_hash(map, item);
    size_t index = hash & map->mask;
    for (;;) {
        struct bucket *entry = bucket_at(map, index);
        if (entry->used) {
            if (map->compare(item, bucket_item(entry)) == 0) {
                entry->used = false;
                map->count--;
                if (map->count <= map->shrinkAt) {
                    resize(map, map->capacity / 2);
                }
                return bucket_item(entry);
            }
        } else {
            return NULL;
        }
        index = (index + 1) & map->mask;
    }
}

void *chm_probe(struct hashmap *map, size_t pos) {
    size_t index = pos & map->mask;
    struct bucket *entry = bucket_at(map, index);
    if (entry->used) {
        return bucket_item(entry);
    }
    return NULL;
}

struct iterator *chm_iterator_new(struct hashmap *map) {
    struct iterator *it = malloc(sizeof(struct iterator));
    it->index = 0;
    it->visited = 0;
    it->hasNext = map->count > 0 ? true : false;
    return it;
}

bool chm_iterator_has_next(struct iterator *it) {
    return it->hasNext;
}

// todo
// bug!
void *chm_iterator_next(struct hashmap *map, struct iterator *it) {
    if (!it->hasNext) {
        return NULL;
    }
    void *item = NULL;
    for (; it->index < map->capacity; it->index++) {
        item = chm_probe(map, it->index);
        if (item != NULL) {
            break;
        }
    }
    it->index++;
    it->visited++;
    it->hasNext = it->visited < map->count ? true : false;
    return item;
}

void chm_iterator_reset(struct hashmap *map, struct iterator *it) {
    it->index = 0;
    it->visited = 0;
    it->hasNext = map->count > 0 ? true : false;
}


//-----------------------------------------------------------------------------
// SipHash reference C implementation
//
// Copyright (c) 2012-2016 Jean-Philippe Aumasson
// <jeanphilippe.aumasson@gmail.com>
// Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//
// default: SipHash-2-4
//-----------------------------------------------------------------------------
static uint64_t SIP64(const uint8_t *in, const size_t inlen,
                      uint64_t seed0, uint64_t seed1)
{
#define U8TO64_LE(p) \
    {  (((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) | \
        ((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) | \
        ((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) | \
        ((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56)) }
#define U64TO8_LE(p, v) \
    { U32TO8_LE((p), (uint32_t)((v))); \
      U32TO8_LE((p) + 4, (uint32_t)((v) >> 32)); }
#define U32TO8_LE(p, v) \
    { (p)[0] = (uint8_t)((v)); \
      (p)[1] = (uint8_t)((v) >> 8); \
      (p)[2] = (uint8_t)((v) >> 16); \
      (p)[3] = (uint8_t)((v) >> 24); }
#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND \
    { v0 += v1; v1 = ROTL(v1, 13); \
      v1 ^= v0; v0 = ROTL(v0, 32); \
      v2 += v3; v3 = ROTL(v3, 16); \
      v3 ^= v2; \
      v0 += v3; v3 = ROTL(v3, 21); \
      v3 ^= v0; \
      v2 += v1; v1 = ROTL(v1, 17); \
      v1 ^= v2; v2 = ROTL(v2, 32); }
    uint64_t k0 = U8TO64_LE((uint8_t*)&seed0);
    uint64_t k1 = U8TO64_LE((uint8_t*)&seed1);
    uint64_t v3 = UINT64_C(0x7465646279746573) ^ k1;
    uint64_t v2 = UINT64_C(0x6c7967656e657261) ^ k0;
    uint64_t v1 = UINT64_C(0x646f72616e646f6d) ^ k1;
    uint64_t v0 = UINT64_C(0x736f6d6570736575) ^ k0;
    const uint8_t *end = in + inlen - (inlen % sizeof(uint64_t));
    for (; in != end; in += 8) {
        uint64_t m = U8TO64_LE(in);
        v3 ^= m;
        SIPROUND; SIPROUND;
        v0 ^= m;
    }
    const int left = inlen & 7;
    uint64_t b = ((uint64_t)inlen) << 56;
    switch (left) {
        case 7: b |= ((uint64_t)in[6]) << 48;
        case 6: b |= ((uint64_t)in[5]) << 40;
        case 5: b |= ((uint64_t)in[4]) << 32;
        case 4: b |= ((uint64_t)in[3]) << 24;
        case 3: b |= ((uint64_t)in[2]) << 16;
        case 2: b |= ((uint64_t)in[1]) << 8;
        case 1: b |= ((uint64_t)in[0]); break;
        case 0: break;
    }
    v3 ^= b;
    SIPROUND; SIPROUND;
    v0 ^= b;
    v2 ^= 0xff;
    SIPROUND; SIPROUND; SIPROUND; SIPROUND;
    b = v0 ^ v1 ^ v2 ^ v3;
    uint64_t out = 0;
    U64TO8_LE((uint8_t*)&out, b);
    return out;
}

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//
// Murmur3_86_128
//-----------------------------------------------------------------------------
static void MM86128(const void *key, const int len, uint32_t seed, void *out) {
#define	ROTL32(x, r) ((x << r) | (x >> (32 - r)))
#define FMIX32(h) h^=h>>16; h*=0x85ebca6b; h^=h>>13; h*=0xc2b2ae35; h^=h>>16;
    const uint8_t * data = (const uint8_t*)key;
    const int nblocks = len / 16;
    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;
    uint32_t c1 = 0x239b961b;
    uint32_t c2 = 0xab0e9789;
    uint32_t c3 = 0x38b34ae5;
    uint32_t c4 = 0xa1e38b93;
    const uint32_t * blocks = (const uint32_t *)(data + nblocks*16);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i*4+0];
        uint32_t k2 = blocks[i*4+1];
        uint32_t k3 = blocks[i*4+2];
        uint32_t k4 = blocks[i*4+3];
        k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
        h1 = ROTL32(h1,19); h1 += h2; h1 = h1*5+0x561ccd1b;
        k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;
        h2 = ROTL32(h2,17); h2 += h3; h2 = h2*5+0x0bcaa747;
        k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;
        h3 = ROTL32(h3,15); h3 += h4; h3 = h3*5+0x96cd1c35;
        k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;
        h4 = ROTL32(h4,13); h4 += h1; h4 = h4*5+0x32ac3b17;
    }
    const uint8_t * tail = (const uint8_t*)(data + nblocks*16);
    uint32_t k1 = 0;
    uint32_t k2 = 0;
    uint32_t k3 = 0;
    uint32_t k4 = 0;
    switch(len & 15) {
        case 15: k4 ^= tail[14] << 16;
        case 14: k4 ^= tail[13] << 8;
        case 13: k4 ^= tail[12] << 0;
            k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;
        case 12: k3 ^= tail[11] << 24;
        case 11: k3 ^= tail[10] << 16;
        case 10: k3 ^= tail[ 9] << 8;
        case  9: k3 ^= tail[ 8] << 0;
            k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;
        case  8: k2 ^= tail[ 7] << 24;
        case  7: k2 ^= tail[ 6] << 16;
        case  6: k2 ^= tail[ 5] << 8;
        case  5: k2 ^= tail[ 4] << 0;
            k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;
        case  4: k1 ^= tail[ 3] << 24;
        case  3: k1 ^= tail[ 2] << 16;
        case  2: k1 ^= tail[ 1] << 8;
        case  1: k1 ^= tail[ 0] << 0;
            k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
    };
    h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;
    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;
    FMIX32(h1); FMIX32(h2); FMIX32(h3); FMIX32(h4);
    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;
    ((uint32_t*)out)[0] = h1;
    ((uint32_t*)out)[1] = h2;
    ((uint32_t*)out)[2] = h3;
    ((uint32_t*)out)[3] = h4;
}

uint64_t sip_hash(const void *data, size_t len,
                  uint64_t seed0, uint64_t seed1) {
    return SIP64(data, len, seed0, seed1);
}

uint64_t murmur_hash(const void *data, size_t len,
                     uint64_t seed0, uint64_t seed1) {
    char out[16];
    MM86128(data, len, seed0, &out);
    return *(uint64_t*)out;
}
