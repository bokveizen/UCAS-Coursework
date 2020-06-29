#include "common.h"
#include "mbox.h"
#include "sync.h"
#include "scheduler.h"

static bool_t unblock_one(node_t *wait_enqueue)
{
//  ASSERT(current_running->nested_count);

    pcb_t *pcb = NULL; 
    pcb = (pcb_t *)dequeue(wait_enqueue);
    if(pcb != NULL)
    {
        unblock(pcb);
        return TRUE;
    }
    return FALSE;
}

typedef struct
{
    char msg[MAX_MESSAGE_LENGTH];
} Message;

typedef struct
{
    char name[MBOX_NAME_LENGTH + 1];
    Message msg_queue[MAX_MBOX_LENGTH];
    int user_num;
    int msg_num;
    lock_t l;
    node_t send_wait_queue;
    node_t recv_wait_queue;
    int send_pos;
    int recv_pos;
} MessageBox;


static MessageBox MessageBoxen[MAX_MBOXEN];
lock_t BoxLock;

/* Perform any system-startup
 * initialization for the message
 * boxes.
 */
void init_mbox(void)
{
    int i;
    for(i=0; i<MAX_MBOXEN; i++)
    {
        MessageBoxen[i].name[0] = '\0';
        MessageBoxen[i].user_num = 0;
        MessageBoxen[i].msg_num = 0;
        lock_init(&MessageBoxen[i].l);
        queue_init(&MessageBoxen[i].send_wait_queue);
        queue_init(&MessageBoxen[i].recv_wait_queue);
        MessageBoxen[i].send_pos = 0;
        MessageBoxen[i].recv_pos = 0;
    }
}

/* Opens the mailbox named 'name', or
 * creates a new message box if it
 * doesn't already exist.
 * A message box is a bounded buffer
 * which holds up to MAX_MBOX_LENGTH items.
 * If it fails because the message
 * box table is full, it will return -1.
 * Otherwise, it returns a message box
 * id.
 */
mbox_t do_mbox_open(const char *name)
{
    (void)name;
    int i;
    for(i=0; i<MAX_MBOXEN; i++)
    {
        if(same_string(name, MessageBoxen[i].name))
        {
            if(current_running->mbox_use[i] = 0)
                MessageBoxen[i].user_num++;
            current_running->mbox_use[i] = 1;
            return i;
        } 
    }
    for(i=0; i<MAX_MBOXEN; i++)
    {
        if(strlen(MessageBoxen[i].name) == 0)
        {
            bcopy(name, MessageBoxen[i].name, strlen(name));
            {
                if(current_running->mbox_use[i] = 0)
                    MessageBoxen[i].user_num++;
                current_running->mbox_use[i] = 1;
                return i;
            }
        }
    }
}

/* Closes a message box
 */
void do_mbox_close(mbox_t mbox)
{
    (void)mbox;
    MessageBoxen[mbox].user_num--;
    current_running->mbox_use[mbox] = 0;
    if(MessageBoxen[mbox].user_num == 0)
    {
        int i = mbox;
        MessageBoxen[i].name[0] = '\0';
        int j;
        MessageBoxen[i].user_num = 0;
        MessageBoxen[i].msg_num = 0;
        lock_init(&MessageBoxen[i].l);
        queue_init(&MessageBoxen[i].send_wait_queue);
        queue_init(&MessageBoxen[i].recv_wait_queue);
        MessageBoxen[i].send_pos = 0;
        MessageBoxen[i].recv_pos = 0;
    }
}

/* Determine if the given
 * message box is full.
 * Equivalently, determine
 * if sending to this mbox
 * would cause a process
 * to block.
 */
int do_mbox_is_full(mbox_t mbox)
{
    (void)mbox;
    return (MessageBoxen[mbox].msg_num == MAX_MBOX_LENGTH);
}

int do_mbox_is_empty(mbox_t mbox)
{
    (void)mbox;
    return (MessageBoxen[mbox].msg_num == 0);
}

/* Enqueues a message onto
 * a message box.  If the
 * message box is full, the
 * process will block until
 * it can add the item.
 * You may assume that the
 * message box ID has been
 * properly opened before this
 * call.
 * The message is 'nbytes' bytes
 * starting at offset 'msg'
 */
void do_mbox_send(mbox_t mbox, void *msg, int nbytes)
{
    (void)mbox;
    (void)msg;
    (void)nbytes;
    if(do_mbox_is_full(mbox))
        block(&MessageBoxen[mbox].send_wait_queue);
    lock_acquire(&MessageBoxen[mbox].l);
    int i;
    for(i=0; i<nbytes; i++)
        MessageBoxen[mbox].msg_queue[MessageBoxen[mbox].send_pos].msg[i] = *((char *)msg + i);
    if(MessageBoxen[mbox].send_pos == MAX_MBOX_LENGTH - 1)
        MessageBoxen[mbox].send_pos = 0;
    else
        MessageBoxen[mbox].send_pos++;
    MessageBoxen[mbox].msg_num++;
    lock_release(&MessageBoxen[mbox].l);
    unblock_one(&MessageBoxen[mbox].recv_wait_queue);
}

/* Receives a message from the
 * specified message box.  If
 * empty, the process will block
 * until it can remove an item.
 * You may assume that the
 * message box has been properly
 * opened before this call.
 * The message is copied into
 * 'msg'.  No more than
 * 'nbytes' bytes will by copied
 * into this buffer; longer
 * messages will be truncated.
 */
void do_mbox_recv(mbox_t mbox, void *msg, int nbytes)
{
    (void)mbox;
    (void)msg;
    (void)nbytes;
    if(do_mbox_is_empty(mbox))
        block(&MessageBoxen[mbox].recv_wait_queue);
    lock_acquire(&MessageBoxen[mbox].l);
    int i;
    for(i=0; i<nbytes; i++)
         *((char *)msg + i) = MessageBoxen[mbox].msg_queue[MessageBoxen[mbox].recv_pos].msg[i];
    if(MessageBoxen[mbox].recv_pos == MAX_MBOX_LENGTH - 1)
        MessageBoxen[mbox].recv_pos = 0;
    else
        MessageBoxen[mbox].recv_pos++;
    MessageBoxen[mbox].msg_num--;
    lock_release(&MessageBoxen[mbox].l);
    unblock_one(&MessageBoxen[mbox].send_wait_queue);
}



