#include "node.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

struct timespec start, end;
double timeuse;

int get_sub_num(int base, int nbits)
{
    int res = 1;
    while(nbits)
    {
        res *= base;
        nbits--;
    }
    return res;
}

void print_time()
{
    clock_gettime(CLOCK_REALTIME, &end);
    timeuse = (double)(((double)(end.tv_nsec - start.tv_nsec))/1000.0);
    printf("The time cost is %fus.\n",timeuse);
}

void set_valid(struct node* op_node, int offset)
{
    unsigned short int set_num = (1 << offset);
    if(op_node->valid & set_num) return;
    op_node->valid += set_num;
}

void set_type(struct node* op_node, int offset, int num)
{
    unsigned short int set_num = (1 << offset);
    if((op_node->type & set_num) != 0 && num != 0) return;
    if((op_node->type & set_num) == 0 && num == 0) return;
    unsigned short int temp = op_node->type & (~set_num);
    op_node->type = temp + (num << offset);
}   

void init_node(struct node* tree)
{
    tree->leaf = NULL;
    tree->inter = NULL;
    tree->type = ~0;
    tree->valid = 0;
}

int count_zero(struct node* op_node)
{
    int searched_bits = 0;
    int zero_count = 0;
    while((op_node->valid >> searched_bits) != 0)
    {
        if(((op_node->valid >> searched_bits) & 1) == 1 && ((op_node->type >> searched_bits) & 1) == 0)
            zero_count++;
        searched_bits++;
    }
    return zero_count;
}

int count_one(struct node* op_node)
{
    int searched_bits = 0;
    int one_count = 0;
    while((op_node->valid >> searched_bits) != 0)
    {
        if(((op_node->valid >> searched_bits) & 1) == 1 && ((op_node->type >> searched_bits) & 1) == 1)
            one_count++;
        searched_bits++;
    }
    return one_count;
}

int insert_inter(struct node* op_node, int offset, int nbits, struct node* real_insert)
{
    int subnode_num = get_sub_num(2, nbits);
    int one_count = count_one(op_node);
    int array_loc = one_count;
    if(one_count == 1)
    {
        op_node->inter = malloc(sizeof(struct node*));
        op_node->inter[0] = real_insert;
        return 0;
    }
    else
    {
        op_node->inter = realloc(op_node->inter, one_count * sizeof(struct node*));
        int num = subnode_num - 1;
        while(num != offset)
        {
            if(((op_node->valid) & (1 << num)) != 0 && ((op_node->type) & (1 << num)) != 0)
            {
                op_node->inter[array_loc - 1] = op_node->inter[array_loc - 2];
                array_loc--;                
            }
            num--;
        }
        op_node->inter[array_loc - 1] = real_insert;
        return array_loc - 1;
    }
}

void init_node_inter(struct node* node_inter, int ports, int nbits, int length)
{
    int subnode_num = get_sub_num(2, nbits);
    struct leaf_node** leaf_array;
    leaf_array = malloc(subnode_num * sizeof(struct leaf_node*));
    for(int i = 0; i < subnode_num; i++)
    {
        struct leaf_node* init_leaf;
        init_leaf = malloc(sizeof(struct leaf_node));
        init_leaf->port = ports;
        init_leaf->length = length;
        leaf_array[i] = init_leaf;
    }
    node_inter->leaf = leaf_array;
    node_inter->inter = NULL;
    int valid = get_sub_num(2, subnode_num) - 1;
    node_inter->valid = valid;
    node_inter->type = 0;
}

int leaf_push_insert_inter(struct node* op_node,int offset,int nbits)
{
    struct node* insert_temp;
    insert_temp = (struct node*)malloc(sizeof(struct node));
    int push_loc = 0;
    for(int i = 0; i < offset; i++)
    {
        if((op_node->valid & (1 << i)) != 0 && (op_node->type & (1 << i)) == 0) 
            push_loc++;
    }
    int port = op_node->leaf[push_loc]->port;
    int length = op_node->leaf[push_loc]->length;
    int zero_count = count_zero(op_node);
    if(zero_count == 1)
    {
        free(*op_node->leaf);
        op_node->leaf = NULL;
    }
    else
    {
        int p;
        for(p = push_loc; p < zero_count - 1; p++)
        {
            op_node->leaf[p] = op_node->leaf[p+1];
        }
        op_node->leaf = realloc(op_node->leaf, (zero_count - 1) * sizeof(struct leaf_node*));
    }
    set_type(op_node, offset, 1);
    struct node* new_inter;
    new_inter = (struct node*)malloc(sizeof(struct node));
    init_node_inter(new_inter, port, nbits, length);
    int q = insert_inter(op_node, offset, nbits, new_inter);
    return q;
    
}

void insert_leaf(struct node* op_node, int offset, int nbits, int port, int length)
{
    int subnode_num = get_sub_num(2, nbits);
    struct leaf_node* leaf;
    leaf = (struct leaf_node*)malloc(sizeof(struct leaf_node));
    leaf->port = port;
    leaf->length = length;
    int zero_count = count_zero(op_node);
    int array_loc = zero_count;
    if(zero_count == 1)
    {
        op_node->leaf = malloc(sizeof(struct leaf_node*));
        op_node->leaf[0] = leaf;
        return;
    }
    op_node->leaf = realloc(op_node->leaf, zero_count * sizeof(struct leaf_node*));
    int num = subnode_num - 1;
    while(num != offset)
    {
        if(((op_node->valid) & (1 << num)) != 0 && ((op_node->type) & (1 << num)) == 0)
        {
            op_node->leaf[array_loc - 1] = op_node->leaf[array_loc - 2];
            array_loc--;
        }
        num--;
    }
    op_node->leaf[array_loc - 1] = leaf;
}

void modify_leaf(struct node* op_node, int offset, int port, int length)
{
    int num = 0;
    int zero_count = count_zero(op_node);
    int array_loc = 0;
    while(num != offset)
    {
        if(((op_node->valid) & (1 << num)) != 0 && (((op_node->type) & (1 << num)) == 0)) 
            array_loc++;
        num++;
    }
    if(length >= op_node->leaf[array_loc]->length)
    {
        op_node->leaf[array_loc]->port = port;
        op_node->leaf[array_loc]->length = length;
    }

}

void modify_inter(struct node* op_node, int offset, int port, int nbits, int length)
{
    int num = 0;
    int one_count = 0;
    while(num != offset)
    {
        if(((op_node->valid) & (1 << num)) != 0 && ((op_node->type) & (1 << num)) != 0) 
            one_count++;
        num++;
    }
    struct node* temp = op_node->inter[one_count];
    int valid_offset = 0;
    while((~temp->valid) != 0)
    {     
        if(((temp->valid) & (1 << valid_offset)) == 0)
        {
            set_valid(temp, valid_offset);
            set_type(temp, valid_offset, 0);
            insert_leaf(temp, valid_offset, nbits, port, length);           
        }
        valid_offset++;
    }
    int leaf_loc = 0;
    int temp_offset = 0;
    while(temp->valid >> temp_offset !=0)
    {
        if(((temp->valid) & (1 << temp_offset)) != 0 && ((temp->type) & (1 << temp_offset)) == 0)
        {
            if(length > temp->leaf[leaf_loc]->length)
            {
                temp->leaf[leaf_loc]->port = port;
                temp->leaf[leaf_loc]->length = length;
                leaf_loc++;
            }
        }       
        temp_offset++;
    }
    temp_offset = 0;
    while(temp->valid >> temp_offset !=0)
    {
        if(((temp->valid) & (1 << temp_offset)) != 0 && ((temp->type) & (1 << temp_offset)) != 0)
        {
            modify_inter(temp, temp_offset, port, nbits, length);
        }
        temp_offset++;
    }
}

void node_insert(struct node* tree, unsigned int ip, int length, int port, int nbits)
{
    int searched_bits = 0;
    struct node* op_node;
    int subnode_num = get_sub_num(2, nbits);
    for(op_node = tree; ; searched_bits = searched_bits + nbits)
    {
        if(searched_bits + nbits < length)
        {
            int location = 32 - searched_bits - nbits;
            int branch_num = (ip >> location) & (subnode_num - 1);
            int is_valid = (op_node->valid) & (1 << branch_num);
            if(is_valid == 0)
            {
                set_valid(op_node, branch_num);
                set_type(op_node, branch_num, 1);
                struct node* inter_insert;
                inter_insert = (struct node*)malloc(sizeof(struct node));
                init_node(inter_insert);
                int insert_loc = insert_inter(op_node, branch_num, nbits, inter_insert);
                op_node = op_node->inter[insert_loc];
            }
            else
            {
                int is_leaf = (op_node->type) & (1 << branch_num);
                if(is_leaf == 0)
                {
                    int leaf_push_insert_loc = leaf_push_insert_inter(op_node, branch_num, nbits);
                    op_node = op_node->inter[leaf_push_insert_loc];
                }
                else
                {
                    int offset = 0;
                    int valid_count = 0;
                    while(offset < branch_num)
                    {
                        if(((op_node->valid & op_node->type)& (1 << offset)) == 1) 
                            valid_count++;
                        offset++;
                    }
                    op_node = op_node->inter[valid_count];
                }
            }
        }
        else
        {
            int location = 32 - length;
            int left_bit = length - searched_bits;
            int left_number = get_sub_num(2, left_bit) - 1;
            int branch_num = (ip >> location) & (left_number);
            for(int fn_offset = branch_num << (nbits - left_bit); fn_offset <= ((branch_num << (nbits - left_bit)) + (1 << (nbits - left_bit)) - 1); fn_offset++)
            {
                int is_valid = (op_node->valid) & (1 << fn_offset);
                if(is_valid == 0)
                {
                    set_valid(op_node, fn_offset);
                    set_type(op_node, fn_offset, 0); 
                    insert_leaf(op_node, fn_offset, nbits, port, length);
                }
                else
                {
                    int is_leaf = (op_node->type) & (1 << fn_offset);
                    if(is_leaf == 0)
                    {
                        modify_leaf(op_node, fn_offset, port, length);
                    }
                    else
                    {
                        modify_inter(op_node, fn_offset, port, nbits, length);
                    }
                }
            }
            return;
        }
    }
}

int search_leaf(struct node* op_node, int searching_bits)
{
    int leaf_loc = 0;
    int offset = 0;
    while(offset != searching_bits)
    {
        if(((op_node->type)  & (1 << offset)) == 0 && ((op_node->valid) & (1 << offset)) != 0) 
            leaf_loc++;
        offset++;
    }
    print_time();
    return op_node->leaf[leaf_loc]->port;
}

struct node* search_inter(struct node* op_node, int searching_bits)
{
    int inter_loc = 0;
    int offset = 0;
    //while(offset <= searching_bits && searching_bits || offset < searching_bits && !searching_bits)
    while(offset < searching_bits)
    {   
        //printf("op_node->type = %d, op_node->valid = %d, searching_bits = %d\n", op_node->type, op_node->valid, searching_bits);
        //printf("offset = %d, searching_bits = %d\n", offset, searching_bits);
        if(((op_node->type) & (1 << offset)) == 1 && ((op_node->valid) & (1 << offset)) == 1) 
            inter_loc++;
        offset++;                   
    }
    //printf("offset = %d, searching_bits = %d\n", offset, searching_bits);
    //printf("inter_loc = %d\n", inter_loc);
    return op_node->inter[inter_loc];
}

int find_ip(struct node* my_tree, unsigned int ip, int nbits)
{
    clock_gettime(CLOCK_REALTIME, &start);
    struct node* op_node = my_tree;
    int searched_bits = 0;
    int port = -1;
    int subnode_num = get_sub_num(2, nbits);
    int location, searching_bits;
    while(1)
    {
        if(32 - searched_bits >= nbits)
        {
            //printf("if(32 - searched_bits >= nbits)\n");
            location = 32 - searched_bits - nbits;
            searching_bits = (ip >> location) & (subnode_num - 1);
            if(!op_node || !op_node->inter) return -1;
            if(((op_node->valid) & (1 << searching_bits) == 0))
            {
                //printf("((op_node->valid) & (1 << searching_bits) == 0)\n");
                print_time();
                return -1;
            }
            else
            {
                //printf("op_node->type = %d, op_node->valid = %d, searching_bits = %d\n", op_node->type, op_node->valid, searching_bits);
                if(((op_node->type) & (1 << searching_bits)) == 0)
                {
                    //printf("((op_node->type) & (1 << searching_bits)) == 0\n");
                    if(!op_node || !op_node->inter) return -1;
                    else return search_leaf(op_node, searching_bits);
                }
                else
                {
                    //printf("search inter\n");
                    if(!op_node || !op_node->inter) return -1;
                    else op_node = search_inter(op_node, searching_bits);
                    //printf("search inter OK\n");
                }
            }
        }
        else
        {
            location = 0;
            int left_bit = 32 - searched_bits;
            int left_number = get_sub_num(2, left_bit) - 1;
            searching_bits = (ip >> location) & (left_number);
            int fn_offset = searching_bits << (nbits - left_bit);

            if(((op_node->valid) & (1 << fn_offset)) == 0)
            {
                print_time();
                return -1;              
            } 

            if(((op_node->type) & (1 << fn_offset)) != 0) 
            {
                print_time();           
                return -2;
            }
            if(!op_node || !op_node->inter) return -1;
            else return search_leaf(op_node, fn_offset);
        }
        searched_bits += nbits;
    }
}


