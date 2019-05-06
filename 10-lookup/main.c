#include <stdio.h>
#include <sys/time.h>
#include "trie_tree.h"
#include "poptrie.h"

int read_ip_port(FILE *input, u32 *ip, u32 *mask)
{
	int i1, i2, i3, i4, port;
	int ret = fscanf(input, "%d.%d.%d.%d%u%d", &i1, &i2, &i3, &i4, mask, &port);
	if (ret != 6) {
        if(ret != -1)
		    printf("parse ip-mask string error(ret = %d)!\n", ret);
		return 0;
	}

	*ip = (i1 << 24) | (i2 << 16) | (i3 << 8) | i4;

	return 1;
}

void dump_iptree(trie_node_t *tree, int tab)
{
    trie_node_t *p = tree;
    if(p->isIP)
    {
        for(int i = 0; i < tab; i++)
            printf(" ");
        //printf("%d "IP_FMT" %u\n", p->isIP, HOST_IP_FMT_STR(p->ip), p->mask);
    }
    for(int k = 0; k < 2; k++)
        if(p->child[k] != NULL)
            dump_iptree(p->child[k], tab+1);
}

extern u32 trie_malloc_sz;
u32 all_ip[700000];

int main()
{
    FILE *input = fopen("forwarding-table.txt", "r");
    iptree = new_trie_node(0);
    u32 ip, mask, num = 0;

    printf("build the basic ip trie...\n");
    while(read_ip_port(input, &ip, &mask))
    {
        int ret = insert_trie_node(ip, mask);
        all_ip[num++] = ip;
    }
    fclose(input);
    printf("trie malloc size: %lu (%u nodes)\n", trie_malloc_sz*sizeof(trie_node_t), trie_malloc_sz);
    printf("\n");

    //dump_iptree(iptree, 0);
    struct timeval tv1, tv2;

    printf("lookup the basic ip trie...\n");
    FILE *output = fopen("lookup-table.txt", "wr");

    gettimeofday(&tv1, NULL);
    for(int i = 0; i < num; i++)
    {
        u32 p = trie_lookup(all_ip[i]);
        //fprintf(output, IP_FMT" %u\n", HOST_IP_FMT_STR(ip), p);
    }
    gettimeofday(&tv2, NULL);
    printf("trie time per lookup: %.2lf ns.\n", (1000.0*(tv2.tv_sec*1000000+tv2.tv_usec-tv1.tv_sec*1000000-tv1.tv_usec))/num);
    fclose(output);

    input = fopen("forwarding-table.txt", "r");

    printf("\nbuild the multi-key ip trie...\n");
    multi_trie = new_multi_trie_node();
    while(read_ip_port(input, &ip, &mask))
    {
        insert_multi_trie_node(ip, mask);
    }
    fclose(input);

    printf("build the poptrie ip trie through multi-key ip trie...\n");
    build_poptrie(multi_trie);

    output = fopen("poptrie-lookup-table.txt", "wr");

    printf("lookup the poptrie ip trie...\n");
    gettimeofday(&tv1, NULL);
    for(int i = 0; i < num; i++)
    {
        u32 p = poptrie_lookup(all_ip[i]);
        //fprintf(output, IP_FMT" %u\n", HOST_IP_FMT_STR(ip), p);
    }
    gettimeofday(&tv2, NULL);
    printf("poptrie time per lookup: %.2lf ns.\n", (1000.0*(tv2.tv_sec*1000000+tv2.tv_usec-tv1.tv_sec*1000000-tv1.tv_usec))/num);
    fclose(output);

    //compare result
    printf("\ncompare result...\n");
    //input = fopen("lookup-table.txt", "r");
    //output = fopen("poptrie-lookup-table.txt", "r");
    u32 mask1, mask2, res = 0;
    for(int i = 0; i < num; i++)
    {
        mask1 = trie_lookup(all_ip[i]);
        mask2 = poptrie_lookup(all_ip[i]);
        if(mask1 != mask2)
        {
            res = 1;
            printf("ERROR: poptrie lookup wrong, expected: "IP_FMT"/%u, get: "IP_FMT"/%u\n",
                HOST_IP_FMT_STR(all_ip[i]), mask1, HOST_IP_FMT_STR(all_ip[i]), mask2);
        }
    }
    if(!res)
        printf("poptrie lookup result is right!\n");

    return 0;
}