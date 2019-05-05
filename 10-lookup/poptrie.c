#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "poptrie.h"

multi_trie_node_t *new_multi_trie_node()
{
    multi_trie_node_t *trie = (multi_trie_node_t *)malloc(sizeof(multi_trie_node_t));
    trie->isIP = 0;
    memset(trie->mask, 0, sizeof(trie->mask));
    for(int i = 0; i < MAX_KEY; i++)
        trie->child[i] = NULL;
    return trie;
}
#define max(a, b) (((a)>(b))?(a):(b))
void insert_multi_trie_node(u32 ip, u32 mask)
{
    multi_trie_node_t *p = multi_trie;
    int k;
    for(k = 4; k < mask; k += 4)
    {
        u32 ind = (ip >> (32-k)) & 0x0F;
        if(p->child[ind] == NULL)
        {
            p->child[ind] = new_multi_trie_node();
        }
        p = p->child[ind];
    }

    u32 ind = (ip >> (32-k)) & 0x0F;
    u32 t = 1;
    for(int i = 0; mask%4 && i < 4 - mask%4; i++)
        t *= 2;
    for(u32 i = 0; i < t; i++)
    {
        p->isIP |= ((u16)1 << (ind+i));
        p->mask[ind+i] = max(p->mask[ind+i], mask);
    }
}

u32 multi_trie_lookup(u32 ip)
{
    multi_trie_node_t *p = multi_trie;
    u32 find = 0, offset = 0;
    u16 v = (ip >> (28 - offset)) & 0x0F;
    while(p)
    {
        if(p->isIP & (1ULL << v))
            find = p->mask[v];
        
        p = p->child[v];
        offset += 4;
        v = (ip >> (28 - offset)) & 0x0F;
    }

    return find;
}

u32 nodes = 0, leafs = 0;
void dfs(multi_trie_node_t *multi_trie)
{
    for(int k = 0; k < MAX_KEY; k++)
    {
        if(multi_trie->child[k] != NULL)
            dfs(multi_trie->child[k]);
    }
    nodes += 1;
    leafs += popcnt(multi_trie->isIP);
}

u32 alloc_node = 1, alloc_leaf = 0;
void dfs_build(multi_trie_node_t *multi_trie, poptrie_node_t *pop_trie)
{
    pop_trie->base0 = alloc_leaf;
    pop_trie->base1 = alloc_node;

    for(int k = 0; k < MAX_KEY; k++)
    {
        if(multi_trie->isIP & (1 << k))
        {
            pop_trie->leafvec |= (1 << k);
            alloc_leaf++;
            L[alloc_leaf-1] = multi_trie->mask[k];
        }
        if(multi_trie->child[k] != NULL)
        {
            pop_trie->vector |= (1 << k);
            alloc_node++;
        }
    }

    for(int k = 0; k < MAX_KEY; k++)
    {
        if(multi_trie->child[k] != NULL)
        {
            u32 bc = popcnt(pop_trie->vector & (((u32)2 << k) - 1));
            u32 ind = pop_trie->base1 + bc - 1;
            dfs_build(multi_trie->child[k], &N[ind]);
        }
    }
}

void build_poptrie(multi_trie_node_t *multi_trie)
{
    dfs(multi_trie);
    N = (poptrie_node_t *)malloc(sizeof(poptrie_node_t) * nodes);
    L = (poptrie_leaf_t *)malloc(sizeof(poptrie_leaf_t) * leafs);
    memset(N, 0, sizeof(N));
    memset(L, 0, sizeof(L));

    poptrie = N;
    dfs_build(multi_trie, poptrie);
}

u32 poptrie_lookup(u32 ip)
{
    u32 ind = 0, offset = 0;
    u16 vector = N[0].vector;
    u16 v = (ip >> (28 - offset)) & 0x0F;
    while(vector & (1ULL << v))
    {
        u32 bc = popcnt(vector & (((u32)2 << v) - 1));
        ind = N[ind].base1 + bc - 1;
        vector = N[ind].vector;
        offset += 4;
        v = (ip >> (28 - offset)) & 0x0F;
    }
    u32 bc = popcnt((N[ind].leafvec) & ((2ULL << v) - 1));
    return L[N[ind].base0 + bc - 1];
}