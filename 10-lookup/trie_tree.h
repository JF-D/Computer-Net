#ifndef __TRIE_TREE_H__
#define __TRIE_TREE_H__

#include <endian.h>
#include "types.h"

struct trie_node {
    u8 isIP;
    struct trie_node *child[2];
};

typedef struct trie_node trie_tree_t;
typedef struct trie_node trie_node_t;

trie_tree_t *iptree;

u32 trie_lookup(u32 ip);
trie_node_t *new_trie_node(u8 isIP);
int insert_trie_node(u32 ip, u32 mask);

#define IP_FMT	"%hhu.%hhu.%hhu.%hhu"
#define LE_IP_FMT_STR(ip) ((u8 *)&(ip))[3], \
						  ((u8 *)&(ip))[2], \
 						  ((u8 *)&(ip))[1], \
					      ((u8 *)&(ip))[0]

#define BE_IP_FMT_STR(ip) ((u8 *)&(ip))[0], \
						  ((u8 *)&(ip))[1], \
 						  ((u8 *)&(ip))[2], \
					      ((u8 *)&(ip))[3]

#define NET_IP_FMT_STR(ip)	BE_IP_FMT_STR(ip)

#if __BYTE_ORDER == __LITTLE_ENDIAN
#	define HOST_IP_FMT_STR(ip)	LE_IP_FMT_STR(ip)
#elif __BYTE_ORDER == __BIG_ENDIAN
#	define HOST_IP_FMT_STR(ip)	BE_IP_FMT_STR(ip)
#endif

#endif