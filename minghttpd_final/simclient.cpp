#define _GNU_SOURCE 1//for POLLRDHUP event
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<poll.h>
#include<fcntl.h>
#include<iostream>
#define BUFFER_SIZE 64
using namespace std;
int main(int argc,char* argv[]){
    if(argc<=2){
        cout<<"ip or port error!"<<endl;
        return 1;
    }
    const char* ip=argv[1];//server ip 
    int port=atoi(argv[2]);
    struct sockaddr_in server_address;//server address
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family=AF_INET;//ipv4
    inet_pton(AF_INET,ip,&server_address.sin_addr);
    server_address.sin_port=htons(port);//import ip ans port
    int sockfd=socket(PF_INET,SOCK_STREAM,0);//ipv4
    assert(sockfd>=0);
    if(connect(sockfd,(struct sockaddr*)&server_address,sizeof(server_address))<0){
        cout<<"connect fail"<<endl;
        close(sockfd);
        return 1;
     } 
    else cout<<"connect success!"<<endl;
    pollfd fds[2];//pollfd 标准输入和可读事件
    fds[0].fd = 0;
    fds[0].events=POLLIN;//读
    fds[0].revents = 0;
    fds[1].fd=sockfd;
    fds[1].events=POLLIN|POLLRDHUP;//TCP连接被对方关闭或者对方关闭了写操作
    fds[1].revents = 0;
    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret=pipe(pipefd);//管道实现通信
    assert(ret!=-1);
    while(1){
        ret=poll(fds,2,-1);//timeout = -1
        if(ret<0){
            cout<<"something error"<<endl;
            break;
        }
        if(fds[1].revents&POLLRDHUP){//tcp connection closed
            cout << "server close the connection" << endl;
            break;
        }
        else if(fds[1].revents&POLLIN){//can read the message
            memset(read_buf,'\0',BUFFER_SIZE);
            recv(fds[1].fd,read_buf,BUFFER_SIZE-1,0);
            cout<<"get data:"<<read_buf<<endl;
        }
        if(fds[0].revents&POLLIN){
            ret=splice(0,NULL,pipefd[1],NULL,32768,SPLICE_F_MORE|SPLICE_F_MOVE);
            ret=splice(pipefd[0],NULL,sockfd,NULL,32768,SPLICE_F_MORE|SPLICE_F_MOVE);
        }
    }
    close(sockfd);
    return 0;
}
