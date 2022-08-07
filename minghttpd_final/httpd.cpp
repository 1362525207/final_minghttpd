#include <httpd.h>


void httpd::bad_request(int client) {//报错错误的http请求
	char buf[1024];
	//发送400,也就是错误的请求
	sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "Content-type: text/html\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "<P>Your browser sent a bad request, ");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "such as a POST without a Content-Length.\r\n");
	send(client, buf, sizeof(buf), 0);
}
void httpd::cat(int client, FILE* resource) {
	char buf[1024];
	fgets(buf, sizeof(buf), resource);//读取一行
	while (!feof(resource))//判断是不是最后一行
	{
		send(client, buf, strlen(buf), 0);
		fgets(buf, sizeof(buf), resource);
	}
}//只读方式获取信息
void httpd::cannot_execute(int client) {//报错不能执行cgi
	char buf[1024];
	//发送500
	sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<P>Error prohibited CGI execution.\r\n");//<P>是空一个段
	send(client, buf, strlen(buf), 0);
}
void httpd::error_die(const char* ername) {//错误信号指出并退出
	perror(ername);
	exit(1);
}
void httpd::execute_cgi(int client,const char* path,
	const char* method, const char* query_string) {//执行cgi服务器
	char buf[1024];
	int cgi_output[2];//管道
	int cgi_input[2];
	pid_t pid;
	int status;
	int i;
	char c;
	int numchars = 1;
	int content_length = -1;
	buf[0] = 'A'; buf[1] = '\0';
	if (strcasecmp(method, "GET") == 0) { // 跳过首部信息并提取content-length
		while ((numchars > 0) && strcmp("\n", buf))  
			numchars = get_line(client, buf, sizeof(buf));//跳过首部信息
	}
	else {//如果是post，首先跳过首部信息，拿到content-length
		numchars = get_line(client, buf, sizeof(buf));
		while ((numchars > 0) && strcmp("\n", buf)) {
			buf[15] = '\0';
			if (strcasecmp(buf, "Content-Length:") == 0) content_length = atoi(&(buf[16]));
			numchars = get_line(client, buf, sizeof(buf));
		}
		if (content_length == -1) {
			bad_request(client);
			return;
		}
	}
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	if (pipe(cgi_output) < 0) { //建立管道
		cannot_execute(client);
		return;
	}
	if (pipe(cgi_input) < 0) {
		cannot_execute(client);
		return;
	}
	if ((pid = fork()) < 0) {//建立进程
		cannot_execute(client);
		return;
	}
	if (pid == 0)  /* 子进程: 运行CGI脚本 */
	{
		char meth_env[255];
		char query_env[255];
		char length_env[255];
		dup2(cgi_output[1], 1);
		dup2(cgi_input[0], 0);//stdin的重定向到读取端
		close(cgi_output[0]);//关闭了cgi_output中的读通道
		close(cgi_input[1]);//关闭了cgi_input中的写通道
		sprintf(meth_env, "REQUEST_METHOD=%s", method);//环境变量：请求方法
		putenv(meth_env);
		if (strcasecmp(method, "GET") == 0) {
			//存储QUERY_STRING
			sprintf(query_env, "QUERY_STRING=%s", query_string);//环境变量：GET参数
			putenv(query_env);
		}
		else {   /* POST */
			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);//存储CONTENT_LENGTH
			putenv(length_env);
		}
		execl(path, path, NULL);//执行CGI脚本
		exit(0);
	}
	else {
		close(cgi_output[1]);
		close(cgi_input[0]);
		if (strcasecmp(method, "POST") == 0)
			for (i = 0; i < content_length; i++)
			{
				recv(client, &c, 1, 0);
				write(cgi_input[1], &c, 1);
			}
		//读取cgi脚本返回数据
		while (read(cgi_output[0], &c, 1) > 0)
			//发送给浏览器
		{
			send(client, &c, 1, 0);
		}
		//运行结束关闭
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);
	}
}
int httpd::get_line(int sock, char* buf, int size) {//捕获一行http信息,返回长度信息
	int i = 0;
	char c = '\0';
	int n;
	while ((i < size - 1) && (c != '\n')) {//\r\n表示的是回车换行
		n = recv(sock, &c, 1, 0);
		if (n > 0) {//说明还有数据
			if (c == '\r') {//说明可能到末尾了
				n = recv(sock, &c, 1, MSG_PEEK);
				if ((n > 0) && (c == '\n')) // 最后一次将是换行符并退出
					recv(sock, &c, 1, 0);
				else
					c = '\n';
			}
			buf[i] = c; i++;
		}
		else c='\n';//退出循环了
	}
	buf[i] = '\0';
	return(i);
}
void httpd::headers(int client, const char* filename) {//获取头部信息
	char buf[1024];
	(void)filename;
	strcpy(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}
void httpd::not_found(int client) {//报错400
	char buf[1024];
	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "your request because the resource specified\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "is unavailable or nonexistent.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}
void httpd::serve_file(int client, const char* filename) {//获取文件内容
	FILE* resource = NULL;
	int numchars = 1;
	char buf[1024];
	buf[0] = 'A';//保证\n对比能通过
	buf[1] = '\0';
	//cout<<filename<<endl;
	while ((numchars > 0) && strcmp("\n", buf)) {
	    numchars = get_line(client, buf, sizeof(buf));
	   
	} /* read & discard headers */
	resource = fopen(filename, "r");//只读形式打开文件
	
	if (resource == NULL) not_found(client);
	else
	{
		headers(client, filename);
		cat(client, resource);
	}
	fclose(resource);
}
int httpd::startup(u_short* port) {//客户端连接
	int httpfd = 0; int option = 1;
	struct sockaddr_in name;//描述监听的地址；
	httpfd = socket(PF_INET, SOCK_STREAM, 0);//表示ipv4以及流数据，用于TCP协议
	if (httpfd == -1) {//创建socket失败
		error_die("socket");//该函数后面会直接报出错误的类型
	}
	socklen_t optlen = sizeof(option);
	setsockopt(httpfd, SOL_SOCKET, SO_REUSEADDR, (void*)&option, optlen);//在套接字级别上设置选项，打开地址复用功能
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons(*port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);//本地网络
	if (bind(httpfd, (struct sockaddr*)&name, sizeof(name)) < 0)
		error_die("bind");
	if (*port == 0) //动态进行端口创建
	{
		socklen_t namelen = sizeof(name);
		if (getsockname(httpfd, (struct sockaddr*)&name, &namelen) == -1)
			error_die("getsockname");
		*port = ntohs(name.sin_port);
	}
	if (listen(httpfd, 5) < 0)
		error_die("listen");
	return(httpfd);
};
void httpd::unimplemented(int client) {
	char buf[1024];
	//发送501说明相应方法没有实现
	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</TITLE></HEAD>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}
