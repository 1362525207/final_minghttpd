#pragma once
#ifndef HTTPD_H
#define HTTPD_H

#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#define ISspace(x) isspace((int)(x))
#define SERVER_STRING "WebServer: ming httpd/0.1.1\r\n"
using namespace std;
class httpd;
struct mypar{
	httpd* httpnow;
	int client_sock;
};
class httpd {
public:
	//static void* accept_request(void* A,void* from_client){//处理http请求
	static void* accept_request(void* nowpar){
	mypar* newpar = static_cast<mypar*>(nowpar);
	//httpd *thiscla = static_cast<httpd*>(A);
	httpd *thiscla=(newpar->httpnow);
	//int client = *(int *)from_client;
 	int client = newpar->client_sock;
	char buf[1024];//缓存存储
	int numchars;//读取一行的长度
	char method[255];//判断是post还是get
	char url[255];
	char path[512];//文件访问路径
	size_t i=0, j=0;
	struct stat st;
	int cgi = 0;//cgi标志符
	char* query_string = NULL;//截止的信息
	numchars = thiscla->get_line(client, buf, sizeof(buf));//获得第一行信息
	while (!ISspace(buf[j]) && (i < sizeof(method) - 1))//获取方法
	{
		method[i] = buf[j];
		i++; j++;
	}
	method[i] = '\0';//POST GET !!!!!!
	if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) //既不是get又不是post
	{
		thiscla->unimplemented(client);
		return NULL;
	}
	if (strcasecmp(method, "POST") == 0)  cgi = 1;//如果是post，则启动cgi
	i = 0;
	while (ISspace(buf[j]) && (j < sizeof(buf))) j++;//定位到url的开始地方
	while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
	{
		url[i] = buf[j];//读取url资源信息
		i++; j++;
	}
	url[i] = '\0';
	if (strcasecmp(method, "GET") == 0) {
		query_string = url;
		while ((*query_string != '?') && (*query_string != '\0'))
			query_string++;
		if (*query_string == '?')//如果有参数，需要启动cgi，定位到参数的第一个
		{
			cgi = 1;
			*query_string = '\0';//url则是资源的开头
			query_string++;
		}
	}
	sprintf(path, "minghttp%s", url);
	//cout << path<<endl;
	if (path[strlen(path) - 1] == '/') strcat(path, "test.html");//如果有/，则加上一部分
	if (stat(path, &st) == -1) {
		while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
			numchars = thiscla->get_line(client, buf, sizeof(buf));
		thiscla->not_found(client);
	}
	else {
		
		if ((st.st_mode & S_IFMT) == S_IFDIR) {//如果请求参数为目录, 自动打开test.html
			strcat(path, "/test.html");
		}
		if ((st.st_mode & S_IXUSR) ||
			(st.st_mode & S_IXGRP) ||
			(st.st_mode & S_IXOTH))
			//S_IXUSR:文件所有者具可执行权限
			//S_IXGRP:用户组具可执行权限
			//S_IXOTH:其他用户具可读取权限 
			cgi = 1;
		if (!cgi) {
			cout<<1<<endl;
			thiscla->serve_file(client, path);
		}
		else thiscla->execute_cgi(client, path, method, query_string);
	}
	close(client);
	cout << "connection close....client: " << client << '\n' << endl;
	//printf("connection close....client: %d \n", client);
	return NULL;
	}
public:
	void bad_request(int);//报错错误的http请求
	void cat(int, FILE*);//只读方式获取信息
	void cannot_execute(int);//报错不能执行cgi
	void error_die(const char*);//错误信号指出并退出
	void execute_cgi(int, const char*, const char*, const char*);//执行cgi服务器
	int get_line(int, char*, int);//捕获一行信息
	void headers(int, const char*);//获取头部信息
	void not_found(int);//报错400
	void serve_file(int, const char*);//获取文件内容
	int startup(u_short*);//客户端连接
	void unimplemented(int);
private:

};


#endif // !HTTPD.H
