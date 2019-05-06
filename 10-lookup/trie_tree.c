#include <stdlib.h>
#include "trie_tree.h"
#include "types.h"
#include "log.h"

u32 trie_malloc_sz = 0;

trie_node_t *new_trie_node(u8 isIP)
{
    trie_node_t *trie = (trie_node_t *)malloc(sizeof(trie_node_t));
    trie_malloc_sz ++;
    trie->isIP = isIP;
    for(int i = 0; i < 2; i++)
        trie->child[i] = NULL;
    return trie;
}

int insert_trie_node(u32 ip, u32 mask)
{
    trie_node_t *p = iptree;
    for(int k = 1; k <= mask; k++)
    {
        u32 ind = (ip >> (32-k)) & 0x1;
        if(p->child[ind] == NULL)
        {
            p->child[ind] = new_trie_node(0);
        }
        p = p->child[ind];
    }

    if(p->isIP)
    {
        log(DEBUG, "ip "IP_FMT"/%d already exists!", HOST_IP_FMT_STR(ip), mask);
        return 0;
    }
    
    p->isIP = mask;
    return 1;
}

u32 trie_lookup(u32 ip)
{
    trie_node_t *p = iptree;
    u32 find = 0, k = 1;
    while(p)
    {
        if(p->isIP)
            find = p->isIP;
        
        u32 ind = (ip >> (32UL-k)) & 0x1;
        p = p->child[ind];
        k++;
    }

    return find;
}