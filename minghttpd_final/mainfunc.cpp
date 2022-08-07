#include <stdio.h>
#include <httpd.h>
#include <iostream>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <threadpool.h>
#include <lst_timer.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
using namespace std;

#define maxeventcount 200
#define fdlimit 65535
#define timeslot 5
static int pipefd[2];
static sort_timer_lst timer_lst;
static int epfd = 0;

int setnonblocking( int fd ) //����ǰ�ļ�����������Ϊ������
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void sig_handler( int sig ) //�źŴ�����
{
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );
    //errno = save_errno;
}

void addsig( int sig )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );//�����źŴ�����
}

void timer_handler()
{
    timer_lst.tick();
    alarm( timeslot );
}

void cb_func( client_data* user_data )
{
    epoll_ctl( epfd, EPOLL_CTL_DEL, user_data->sockfd, 0 );
    //assert( user_data );
    close( user_data->sockfd );
    cout<< "because of timeout ,close fd: " << user_data->sockfd;
}

int main()
{
    httpd httpnow;//����httpd����
    struct mypar par;
    int server_sock = -1;
    u_short port = 8000;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);
    int connfd_fd =-1;
    //�����̳߳�
    threadpool pool(5);
    cout << "threadpool created..." << endl;
    server_sock = httpnow.startup(&port);
    //����epoll�¼�
    int nfds;
    //�������ڴ���accept��epollר�õ��ļ�������
    epfd = epoll_create(5);
    struct epoll_event ev, events[maxeventcount];//�Կռ任ʱ��
    ev.data.fd = server_sock;
    //����Ҫ������¼�����
    ev.events = EPOLLIN | EPOLLET;
    //ע��epoll�¼�
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_sock, &ev);
    cout << "epoll established and the server_sock is " << server_sock << " the port is: " << port << endl;

    int ret = socketpair( PF_UNIX, SOCK_STREAM, 0, pipefd );//ά���ܵ���ͨ�������������ź�
    setnonblocking(pipefd[1]);
    //addfd(epfd,pipefd[0]);
    ev.data.fd = pipefd[0];
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl( epfd, EPOLL_CTL_ADD, pipefd[0], &ev );
    setnonblocking( pipefd[0] );

    addsig( SIGALRM );
    client_data* users = new client_data[fdlimit];
    bool timeout = false;
    alarm( timeslot );

    while (true) {
        nfds = epoll_wait(epfd, events, maxeventcount, 500);
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server_sock) {//��ʾ�����ӵļ���
                connfd_fd = accept(server_sock, (sockaddr*)&client_name, &client_name_len);
                if (connfd_fd < 0)
                {
                    perror("client connect failed !!!");
                    exit(1);
                }
                char* str = inet_ntoa(client_name.sin_addr);
                cout << "accapt a connection from " << str << endl;//���client
                ev.data.fd = connfd_fd;
                //��������ע��Ķ������¼�
                ev.events = EPOLLIN | EPOLLET;
                //ע��ev
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd_fd, &ev);
		        //���붨ʱ�������ڲ��µ�����
		        users[connfd_fd].address = client_name;
                users[connfd_fd].sockfd = connfd_fd;
                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd_fd];
                timer->cb_func = cb_func;
                time_t cur = time( NULL );
                timer->expire = cur + 3 * timeslot;
                users[connfd_fd].timer = timer;
                timer_lst.add_timer( timer );
            }
	    else if (events[i].data.fd == pipefd[0] && events[i].events & EPOLLIN){ //���յ�alarm�ź���
		        int sig;
                char signals[1024];
                ret = recv( pipefd[0], signals, sizeof( signals ), 0 );
                if( ret == -1 || ret == 0 )
                {
                    cout << "pipe has sig, but no sig!"<<endl;
                    continue;
                }
                else
                {
                    for( int i = 0; i < ret; ++i )
                    {
                        switch( signals[i] )
                        {
                            case SIGALRM:
                            {
                                timeout = true;//�յ��źź��ִ��һ�²���
                                break;
                            }
                        }
                    }
                }
	    }
            else if (events[i].events & EPOLLIN)//������Ѿ����ӵ��û��������յ����ݣ���ô���ж��롣
            {
                std::cout << "start worker thread ID:" << std::this_thread::get_id() << std::endl;
                par.client_sock = events[i].data.fd;
                par.httpnow = &httpnow;
			util_timer* timer = users[events[i].data.fd].timer;
			if(timer){
				time_t cur = time( NULL );
                        timer->expire = cur + 3 * timeslot;
                        cout<<"adjust timer once"<<endl;
                        timer_lst.adjust_timer( timer );		//����ʱ��	
			}
                pool.enqueue(httpd::accept_request, (void*)(&par));
            }
        }
	if(timeout){
	timer_handler();
	timeout=false;	
	}
    }
    close(server_sock);
    cout << "Bye~" << endl;
    return 0;
}


/*
Դ����
httpd httpnow;
    struct mypar par;
    int server_sock = -1;
    u_short port = 8000;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);
    pthread_t newthread;

    server_sock = httpnow.startup(&port);
    cout << "http server_sock is " << server_sock << " the port is: " << port<<endl;
    while (true)
    {
       client_sock = accept(server_sock,(struct sockaddr*)&client_name,&client_name_len);
      par.client_sock=client_sock;
      par.httpnow = &httpnow;
        if (client_sock == -1)
            httpnow.error_die("accept");
if (pthread_create(&newthread, NULL, httpd::accept_request, (void*)(&par)) != 0)
perror("pthread_create");
    }

    close(server_sock);

*/
