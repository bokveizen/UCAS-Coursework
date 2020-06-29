struct leaf_node
{
    int port;
    int length;
};

struct node
{
    struct leaf_node** leaf;
    struct node** inter;
    unsigned short int type;
    unsigned short int valid;
};

//工具函数
//--------------------------------------------------------
void print_time();
int get_sub_num(int base, int nbits);
void set_valid(struct node* op_node, int offset);
void set_type(struct node* op_node, int offset, int num);
int count_zero(struct node* op_node);
int count_one(struct node* op_node);
//--------------------------------------------------------

//建树函数
//--------------------------------------------------------
void init_node(struct node* tree);
void node_insert(struct node* tree, unsigned int ip, int length, int port, int nbits); 
int insert_inter(struct node* op_node, int offset, int nbits, struct node* real_insert);
void insert_leaf(struct node* op_node, int offset, int nbits, int port, int length);
void modify_inter(struct node* op_node, int offset, int port, int nbits, int length);
void modify_leaf(struct node* op_node, int offset, int port, int length);
//--------------------------------------------------------

//叶推函数
//--------------------------------------------------------
void init_node_inter(struct node* real_insert, int ports, int nbits, int length);
int leaf_push_insert_inter(struct node* op_node, int offset, int nbits);
//--------------------------------------------------------

//搜索函数
//--------------------------------------------------------
struct node* search_inter(struct node* op_node, int searching_bits);
int search_leaf(struct node* op_node, int searching_bits);
int find_ip(struct node* my_tree, unsigned int ip, int nbits);
//--------------------------------------------------------

