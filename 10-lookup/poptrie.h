#ifndef __POPTRIE_H__
#define __POPTRIE_H__

#include "types.h"

#define MAX_KEY 16
#define popcnt(x) __builtin_popcount((u32)(x))

struct poptrie_node {
    u16 leafvec;
    u16 vector;
    u32 base0;
    u32 base1;
};

typedef u8 poptrie_leaf_t;
typedef struct poptrie_node poptrie_node_t;
struct poptrie_node *poptrie;
poptrie_leaf_t *L;
poptrie_node_t *N;

struct multi_trie_node {
    u16 isIP;
    u8 mask[MAX_KEY];
    struct multi_trie_node *child[MAX_KEY];
};

typedef struct multi_trie_node multi_trie_node_t;

multi_trie_node_t *multi_trie;

multi_trie_node_t *new_multi_trie_node();
void insert_multi_trie_node(u32 ip, u32 mask);
void dfs(multi_trie_node_t *multi_trie);
void dfs_build(multi_trie_node_t *multi_trie, poptrie_node_t *pop_trie);
void build_poptrie(multi_trie_node_t *multi_trie);
u32 poptrie_lookup(u32 ip);
u32 multi_trie_lookup(u32 ip);
#endif