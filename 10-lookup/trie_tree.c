#include "trie_tree.h"
#include "types.h"
#include "log.h"

extern trie_tree_t iptree;

trie_node_t *new_trie_node(u8 isIP, u32 ip, u32 mask)
{
    trie_node_t *trie = (trie_node_t *)malloc(sizeof(trie_node_t));
    trie->isIP = isIP;
    trie->ip = ip;
    trie->mask = mask;
    for(int i = 0; i < 2; i++)
        trie->child[i] = NULL;
    return trie;
}

int insert_trie_node(u32 ip, u32 mask)
{
    trie_node_t *p = &iptree;
    for(int k = 1; k <= mask; k++)
    {
        u32 ind = (ip >> (32-k)) & 0x1;
        if(p->child[ind] == NULL)
        {
            p->child[ind] = new_trie_node(0, 0, k);
        }
        p = p->child[ind];
    }

    if(p->isIP)
    {
        log(DEBUG, "ip "IP_FMT"/%d already exists!", HOST_IP_FMT_STR(ip), mask);
        return -1;
    }
    
    p->ip = ip;
    p->mask = mask;
    return 1;
}

trie_node_t *trie_lookup(u32 ip, u32 mask)
{
    trie_node_t *p = &iptree, *find = p;
    int k = 1;
    while(p)
    {
        if(p->isIP)
            find = p;
        
        u32 ind = (ip >> (32-k)) & 0x1;
        p = p->child[ind];
        k++;
    }

    return find;
}