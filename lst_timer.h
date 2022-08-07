#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>

#define BUFFER_SIZE 64
class util_timer;
struct client_data
{//客户数据建立，包括地址信息，文件描述符以及缓冲区模拟，以及专属的时间点
    sockaddr_in address;
    int sockfd;
    char buf[ BUFFER_SIZE ];
    util_timer* timer;
};

class util_timer //存储信息包括双向指针，客户数据信息，期待处理的时间，以及到时间后需要进行的函数
{
public:
    util_timer() : prev( NULL ), next( NULL ){}

public:
   time_t expire; 
   void (*cb_func)( client_data* );//在main文件里
   client_data* user_data;
   util_timer* prev;
   util_timer* next;
};

class sort_timer_lst //链表，存储各个时间节点
{
public:
    sort_timer_lst() : head( NULL ), tail( NULL ) {}
    ~sort_timer_lst()
    {//删除所有的指针
        util_timer* tmp = head;
        while( tmp )
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
    void add_timer( util_timer* timer )
    {
        if( !timer )
        {
            return;
        }
        if( !head )
        {
            head = tail = timer;
            return; 
        }
        if( timer->expire < head->expire )
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer( timer, head );
    }
    void adjust_timer( util_timer* timer ) //调整该节点的位置，摘出来该节点并重新插入
    {
        if( !timer )
        {
            return;
        }
        util_timer* tmp = timer->next;
        if( !tmp || ( timer->expire < tmp->expire ) )
        {
            return;
        }
        if( timer == head )
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer( timer, head );
        }
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer( timer, timer->next );
        }
    }
    void del_timer( util_timer* timer )
    {
        if( !timer )
        {
            return;
        }
        if( ( timer == head ) && ( timer == tail ) )
        {
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }
        if( timer == head )
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        if( timer == tail )
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }
    void tick()
    {
        if( !head )
        {
            return;
        }
        cout<< "timer tick" <<endl;
        time_t cur = time( NULL );
        util_timer* tmp = head;
        while( tmp )
        {
            if( cur < tmp->expire )
            {
                break;
            }
            tmp->cb_func( tmp->user_data );
            head = tmp->next;
            if( head )
            {
                head->prev = NULL;
            }
            delete tmp;//防止内存泄漏
            tmp = head;
        }
    }

private:
    void add_timer( util_timer* timer, util_timer* lst_head )//链表的插入算法
    {
        util_timer* prev = lst_head;
        util_timer* tmp = prev->next;
        while( tmp )
        {
            if( timer->expire < tmp->expire )
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        if( !tmp )
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
        
    }

private:
    util_timer* head;
    util_timer* tail;
};

#endif
