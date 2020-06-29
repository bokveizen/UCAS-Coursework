#include "node.h"

#include <stdio.h>
#include <string.h>


int nbits;
void create_tree(struct node* my_tree, FILE* fp, int nbits, int lines);
void lookup_tree(struct node* my_tree, int nbits);

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        printf("ARG ERROR!\n");
        return 0;
    }

    #ifdef one_bit
        nbits = 1;
    #endif

    #ifdef two_bit
        nbits = 2;
    #endif

    #ifdef three_bit
        nbits = 3;
    #endif

    #ifdef four_bit
        nbits = 4;
    #endif

    struct node my_tree;
    init_node(&my_tree);

    FILE* fp = fopen(argv[1],"r");
    int lines = 0;
    char buf[1000];
    while(fgets(buf, 1000, fp)) 
    {
        if(strlen(buf) <= 2) break;
        lines++;
    }
    printf("%d lines.\n",lines);
    fclose(fp);

    fp = fopen(argv[1],"r");
    printf("Creating tree.\n");
    create_tree(&my_tree, fp, nbits, lines);
    printf("Tree created.\n");
    fclose(fp);

    lookup_tree(&my_tree, nbits);
}

void create_tree(struct node* my_tree, FILE* fp, int nbits, int lines)
{
    char dot;
    unsigned char ip1, ip2, ip3, ip4;
    unsigned int ip, ip_int1, ip_int2, ip_int3, ip_int4;
    int mask_len, port;
    int count = 0;
    while(count < lines)
    {
        fscanf(fp, "%hhu%c%hhu%c%hhu%c%hhu%c%d%c%d%c", &ip1, &dot, &ip2, &dot, &ip3, &dot, &ip4, &dot, &mask_len, &dot, &port, &dot);
        ip_int1 = ((unsigned int)ip1)<<24;
        ip_int2 = ((unsigned int)ip2)<<16;
        ip_int3 = ((unsigned int)ip3)<<8;
        ip_int4 = ((unsigned int)ip4);
        ip = ip_int1 + ip_int2 + ip_int3 + ip_int4;
        node_insert(my_tree, ip, mask_len, port, nbits);
        count++;
    }
}

void lookup_tree(struct node* my_tree, int nbits)
{
    char dot1, dot2, dot3;
    unsigned int ip1, ip2, ip3, ip4;
    unsigned int ip, ip_int1, ip_int2, ip_int3, ip_int4;
    while(1)
    {
        dot1 = dot2 = dot3 = ip1 = ip2 = ip3 = ip4 = 0;
        printf("\nPlease input ip to lookup:\n");
        scanf("%u%c%u%c%u%c%u", &ip1, &dot1, &ip2, &dot2, &ip3, &dot3, &ip4);
        char ch = 0;
        int input_error = 0;
        while((ch = getchar()) != '\n' && ch != EOF && ch != 10 && !ch);
        if(dot1 != '.' || dot2 != '.' || dot3 != '.' || ip1 >= 256 || ip2 >= 256 || ip3 >= 256 || ip4 >= 256)
        {
            printf("IP format ERROR!\n");
            input_error = 1;
        }
        else if (ch != 0 && ch != 10)
        {
            printf("Input too long, truncation done!\n");
        }

        if(!input_error)
        {
            printf("Searching %d.%d.%d.%d\n", ip1, ip2, ip3, ip4);
            ip_int1 = ((unsigned int)ip1)<<24;
            ip_int2 = ((unsigned int)ip2)<<16;
            ip_int3 = ((unsigned int)ip3)<<8;
            ip_int4 = ((unsigned int)ip4);
            ip = ip_int1 + ip_int2 + ip_int3 + ip_int4;
            int port = find_ip(my_tree, ip, nbits);
            if(port == -1 || port == -2) 
            {
                printf("Ip not fount!\n");
                
            }
            else printf("The port of this IP is %d.\n", port);
        }
    }
}

