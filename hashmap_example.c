#include "hashmap.h"

struct user {
    char *id;
    char *name;
};

int user_compare(const void *a, const void *b) {
    const struct user *ua = a;
    const struct user *ub = b;
    return strcmp(ua->id, ub->id);
}

uint64_t user_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct user *user = item;
    return sip_hash(user->id, strlen(user->id), seed0, seed1);
}

int main() {
    // create a new hash map where each item is a `struct user`.
    // The second argument is the initial capacity.
    // The third and fourth arguments are optional seeds that are passed to the following hash function.
    // The fourth argument is compare function
    struct hashmap *map = chm_new(sizeof(struct user), 0, 0, 0,
                                      user_hash, user_compare);


    // Here we'll load some users into the hash map. Each set operation
    // performs a copy of the data that is pointed to in the second argument.
    chm_put(map, &(struct user){ .id="10165102232", .name="Dale" });
    chm_put(map, &(struct user){ .id="10165102233", .name="Roger" });
    chm_put(map, &(struct user){ .id="10165102234", .name="Jane" });

    struct user *user;

    printf("-- get some users --\n");
    user = chm_get(map, &(struct user){ .id="10165102234" });
    printf("%s: name=%s\n", user->id, user->name);

    user = chm_get(map, &(struct user){ .id="10165102233" });
    printf("%s: name=%s\n", user->id, user->name);

    user = chm_get(map, &(struct user){ .id="10165102232" });
    printf("%s: name=%s\n", user->id, user->name);

    user = chm_get(map, &(struct user){ .id="10165102240" });
    printf("%s\n", user?"exists":"not exists");

    chm_free(map);

}

// output:
// -- get some users --
// Jane age=47
// Roger age=68
// Dale age=44
// not exists
//

