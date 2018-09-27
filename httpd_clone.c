/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);
void serve_file(int, const char *);
void headers(int, const char *);
void cat(int, FILE *);
void not_found(int);
void unimplemented(int);
int get_line(int, char *, int);
void error_die(const char *);
int startup(u_short *);
void execute_cgi(int, const char *, const char *, const char *);
void bad_request(int);
void cannot_execute(int);

/****************************************/
/*一个请求导致调用服务器端口上的accept（）。
 * 适当地处理请求。
 * 参数：连接到客户端的套接字 */
/****************************************/
void accept_request(int client)
{
  char buf[1024];
  // 用来读所有的缓存数据 buffer
  int numchars;
  //
  char method[255];
  // 读 HTTP 头方法，包括 GET 和 POST
  char url[255];
  // 地址 url
  char path[512];
  // 相对路径
  size_t i,j;
  // size_t实质上只是一个typedef
  // 通常被typedef成unsigned int或unsigned long或unsigned long long。
  // 不同的平台会根据自己能表示的最大值而选择实现不同的typedef
  // 保证可移植性


  struct stat st;
  // struct stat is a system struct that is defined to store information about files
  int cgi = 0;
  // 用来标记是否是 cgi 请求
  char *query_string = NULL;

  numchars = get_line(client, buf, sizeof(buf));

  // 读 httpd 请求的第一行数据 (request line),把方法存进 method 中
  // 请求行 <method><uri><version>
  i = 0; j = 0;
  while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
  {
    method[i] = buf[j];
    i++; j++;
  }
  method[i] = '\0';

  // 如果请求的方法不是 GET 或 POST 则发送 response 告诉客户端没有实现该方法
  // strcasecmp 忽略大小写比较字符
  if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
    unimplemented(client);
    return ;
  }


  // 如果是 POST 方法就将 cgi 变量置一(true)
  // 注意strcasecmp 这个函数，如果两个 str 一样结果为0
  if (strcasecmp(method, "POST") == 0)
    cgi = 1;

  i = 0;
  // 跳过所有的空白字符
  while (ISspace(buf[j]) && (j < sizeof(buf)))
    j++;

  // 然后把 URL 读出来放到 url 字符数组中
  while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
    url[i] = buf[j];
    i++; j++;
  }
  url[i] = '\0';


  // 如果这是一个 get方法的话
  if (strcasecmp(method, "GET") == 0) {
    // 用一个指针指向 url
    query_string = url;

    // 去遍历这个 url, 跳过字符 ? 前面的所有字符，如果遍历完毕也没有找到 ? 则退出循环
    while ((*query_string != '?') && (*query_string != '\0'))
      query_string++;

    // 退出循环后检查当前的字符是 ? 还是字符串 url 的结尾
    if (*query_string == '?') {
      // 如果是 ？ 的话，证明这个请求需要用到 cgi，将 cgi 变量置一(true)
      cgi = 1 ;
      // 从 ？处把字符串 url 分隔成两份
      // 这里的 url 会长成类似这样： foo\0bar\0
      // 也就是分隔成两份的意思
      *query_string = '\0';
      // 使指针指向字符 ? 后面的那个字符
      query_string++;
    }
  }

  // 将前面分隔两份的前面那份字符串，拼接在字符串 htdocs 后存储在数组 path 中。
  sprintf(path, "htdocs%s", url);

  // 如果 path 数组中的这个字符串的最后一个字符是以字符 / 结尾的话，就拼接上一个 “index.html” 的字符串
  if (path[strlen(path) - 1] == '/')
    strcat(path, "index.html");


  // 在系统上查询该文件是否存在
  // int stat(const char *file_name, struct stat *buf);
  // 通过文件名filename获取文件信息，并保存在buf所指的结构体stat中
  // 执行成功则返回0，失败返回-1，错误代码存于errno
  if (stat(path, &st) == -1) {
    // 如果不存在，那把这次 http 的请求后续内容 (head 和 body) 全部读完并忽略
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
    // strcmp(s1, s2) s1 == s2 -> return 0
      numchars = get_line(client, buf, sizeof(buf));
    // 然后返回一个找不到文件的 response 给客户端
    not_found(client);
  } else {
    // 文件存在
    // st_mode; 文件的类型和存取的权限
    // S_IFMT   0170000    文件类型的位遮罩
    // 是路径， 是目录，那么我们 path 之后添加 "index.html"
    if ((st.st_mode & S_IFMT) == S_IFDIR)
      strcat(path, "/index.html");


    if ((st.st_mode & S_IXUSR) ||
        (st.st_mode & S_IXGRP) ||
        (st.st_mode & S_IXOTH)   )
      //  是可执行文件
      cgi = 1;


    if (!cgi)
      // 服务静态内容
      serve_file(client, path);

    else
      // 服务动态内容
      execute_cgi(client, path, method, query_string);
    }

    close(client);
}


/****************************************/
/* 通知客户端它发出的请求有问题。
 * 参数：客户端套接字 */
/****************************************/
void bad_request(int client) {
  char buf[1024];

  sprintf(buf, "HTTP/1.0 400 BAD REQUEST");
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


/****************************************/
/* 通知客户端无法执行CGI脚本。
 * 参数：客户端套接字描述符。*/
/****************************************/
void cannot_execute(int client){
  char buf[1024];

  sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
  send(client, buf, sizeof(buf), 0);

}


/****************************************/
/* 执行CGI脚本。 会设置合适的环境变量。
 * 参数：客户端套接字描述符
 *      CGI脚本的路径 */
/****************************************/
void execute_cgi(int client, const char * path, const char * method, const char * query_string){
  char buf[1024];
  int cgi_output[2];
  int cgi_input[2];
  pid_t pid;
  int status;
  int i;
  char c;
  int numchars = 1;
  int content_length = -1;

  // 往 buf 中填东西以保证能进入下面的 while
  buf[0] = 'A'; buf[1] = '\0';
  // 如果 http 的请求是 GET 方法的话去读并忽略请求剩下的内容
  if (strcasecmp(method, "GET") == 0)
    while ((numchars >0) && strcmp("\n", buf)) /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
  else {
    // POST
    numchars = get_line(client, buf, sizeof(buf));
    // 这个循环的目的是读出指示 body 长度大小的参数，并记录 body 的长度大小。其余的 header 里面的参数一律忽略
    // 注意这里只读完 header 的内容，body 的内容没有读
    // HTTP POST请求的参数是在请求主体 request body 中传递的
    while ((numchars > 0) && strcmp("\n", buf)) {
      // strlen("Content-Length:") = 15
      buf[15] = '\0';
      if (strcasecmp(buf, "Content-Length:") == 0)
        content_length = atoi(&(buf[16])); //记录 body 的长度大小
      numchars = get_line(client, buf, sizeof(buf));
    }

    // 如果 http 请求的 header 没有指示 body 长度大小的参数，则报错返回
    if (content_length == -1) {
      bad_request(client);
      return;
    }
  }

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf),0);

  // 下面这里创建两个管道，用于两个进程间通信
  if (pipe(cgi_output) < 0) {
    cannot_execute(client);
    return;
  }
  if (pipe(cgi_input) < 0) {
    cannot_execute(client);
    return;
  }

  // 创建一个子进程
  if ((pid = fork()) < 0) {
    cannot_execute(client);
    return;
  }

  // 子进程用来执行 cgi 脚本
  if (pid == 0) { /* child: CGI script */
    char meth_env[255];
    char query_env[255];
    char length_env[255];

    // dup2 将子进程的输出由标准输出重定向到 cgi_output 的管道写端上
    dup2(cgi_output[1], 1);
    // 将紫禁城的输出由标准输入重定向到 cgi_input 的管道读端上
    dup2(cgi_input[0], 0);
    // 关闭 cgi_output 的管道读端与 cgi_input 管道写端
    close(cgi_output[0]);
    close(cgi_input[1]);

    // 构造一个环境变量
    sprintf(meth_env,"REQUEST_METHOD=%s", method);
    // 将这个环境变量加进子进程的运行环境中
    putenv(meth_env);

    // 根据 http 请求的不同方法，构造并存储不同的环境变量
    if (strcasecmp(method, "GET") == 0) {
      sprintf(query_env, "QUERY_STRING=%s", query_string);
      putenv(query_env);
    } else { /* POST */
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
      putenv(length_env);
    }

    // 将子进程替换成另一个进程并执行 cgi 脚本
    execl(path, path, NULL);
    exit(0);
  } else { /* parent */
    // 父进程则关闭了 cgi_output 管道的写端和 cgi_input 管道的读端
    close(cgi_output[1]);
    close(cgi_input[0]);

    // 如果是 POST 方法的话就继续读 body 的内容，并写道 cgi_input 管道里让子进程去读
    if (strcasecmp(method, "POST") == 0)
      for (i = 0; i < content_length; i++) {
        recv(client, &c, 1, 0);
        write(cgi_input[1], &c, 1);
      }

    // 然后从 cgi_output 管道中读取子进程的输出，并发送到客户端去
    while (read(cgi_output[0], &c, 1) > 0)
      send(client, &c, 1, 0);

    // 关闭管道
    close(cgi_output[0]);
    close(cgi_input[1]);
    // 等待子进程的退出
    waitpid(pid, &status, 0);
    }
}


/****************************************/
/* 将常规文件发送给客户端。 使用标题, 如果发生错误则报告。
 * 参数：指向从套接字的文件描述符
 *      要提供的文件的名称 */
/****************************************/
void serve_file(int client, const char *filename) {
  FILE *resource = NULL;
  int numchars = 1;
  char buf[1024];

  // 确保 buf 里面有东西，能进入下面的 while 循环
  buf[0] = 'A'; buf[1] = '\0';

  while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
    numchars = get_line(client, buf, sizeof(buf));

  // 打开 filename 文件
  resource = fopen(filename, "r");
  if (resource == NULL)
    not_found(client);
  else{
    // 打开成功, 把文件的基本信息封装成 response 的 header 并返回
    headers(client, filename);
    // 接着把文件的内容读出来作为 response 的 body 发送到客户端
    cat(client, resource);
  }

  fclose(resource);
}


/****************************************/
/* 返回有关文件的信息HTTP标头。
 * 参数：用于打印标题的套接字
 *     文件名称 */
/****************************************/
void headers(int client, const char* filename){
  char buf[1024];
  (void)filename; /* could use filename to determine file type */

  strcpy(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
}


/****************************************/
/* 将文件的全部内容放在套接字上。 这个功能以 UNIX“cat”命令命名，
 * 因为它可能更容易做 pipe ，fork 和 exec（“cat”）之类的事情。
 * 参数：客户端套接字描述符
 *     该文件的文件指针 */
/****************************************/
void cat(int client, FILE *resource){
  char buf[1024];

  // 从文件中读取指定内容
  fgets(buf, sizeof(buf),resource);
  while (!feof(resource)) {
    send(client, buf, strlen(buf), 0);
    fgets(buf, sizeof(buf), resource);
  }
}

/****************************************/
/* 为客户端提供404错误。*/
/****************************************/
void not_found(int client){
  char buf[1024];

  sprintf(buf, "HTTP/1.0 404 Not Found\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><HEAD><TITLE>Not not_found\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</TITLE></HEAD>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>The server could not fulfill.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "your request because the resource specified.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "is unavailable or nonexist\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/****************************************/
/* 通知客户端所请求的Web方法尚未实施。
 * 参数：客户端套接字 */
/****************************************/
void unimplemented(int client) {
  char buf[1024];

  // sprintf(char * restrict str, const char * restrict format, ...);
  // sprintf 实现生成 buf 为 string 的方法
  // send 然后送出， 怀疑之所以拆成这么多段是因为 buf 大小限制以及 send() 函数限制
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

/**********************************************************************/
/* 从套接字中获取一行，无论该行是否以换行符结尾，回车或CRLF组合。
 * 用空字符终止读取的字符串。
 * 如果缓冲区结尾没有找到新行指示符， 字符串以null结尾。
 * 如果有上面三种终止符任意一种读到，最后一个字符将是换行符，并以null字符结束。
 * 参数：套接字描述符
 *      用于保存数据的缓冲区
 *      缓冲区的大小
 * 返回：存储的字节数（不包括null）

 * 就像 strlen("hello") -> 5
 * 也不包括终止字符 null -> \0
 */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
  int i = 0;
  char c = '\0';
  int n;

  while ((i < size - 1) && (c != '\n'))
  {

    n = recv(sock, &c, 1, 0);
    // 读一个字节的数据存放在 c 中
    // int recv(int sockfd, void *buf, int len, unsigned int flags);
    // sockfd 是要读的套接字描述符。buf 是要读的信息的缓冲。len 是缓冲的最大长度。 flags 可以设置为 0。
    // recv() 返回实际读入缓冲的 数据的字节数。或者在错误的时候返回-1， 同时设置 errno。

    /* DEBUG printf("%02X\n",c ); */
    // 成功读到的数据 n > 0
    if (n > 0)
    {
      // 如果读到的是 \r
      if ( c == '\r')
      {
        n = recv(sock, &c, 1, MSG_PEEK);
        //  MSG_PEEK - peek at incoming message
        /* DEBUG printf("%02X\n",c ); */

        // 如果读到的数是 \n
        if ((n > 0) && (c == '\n'))
          recv(sock, &c, 1, 0);
        // 其它状况，比如我们没有再读到数据
        else
          c = '\n';
      }

      buf[i] = c;
      i++;
    }
    else
      c = '\n';
  }
  buf[i] = '\0';


  return(i);
}

/****************************************/
/* 使用perror（）打印出错误消息
 *（针对系统错误; 基于errno的值，表示系统调用错误）
 * 并退出程序指示错误。*/
/****************************************/
void error_die(const char *sc)
{
  //包含于<stdio.h>,基于当前的 errno 值，在标准错误上产生一条错误消息
  perror(sc);
  exit(1);
}



/****************************************/
/* 此函数启动侦听Web连接在指定的端口上。
 * 如果端口为0，则动态分配端口并修改原始端口变量以反映实际情况。
 * 参数：指向包含要连接的端口的变量的指针
 * 返回：套接字 */
/****************************************/
int startup(u_short *port)
{
  int httpd = 0;
  // 这里我们主要只考虑和处理 IPv4 的状况
  struct sockaddr_in name;


  // PF_INET 跟 AF_INET 其实一样，这里我们就取 protocol family 之意
  // SOCK_STREAM 格式
  httpd = socket(PF_INET, SOCK_STREAM, 0);
  if (httpd == -1)
    error_die("socket");

  memset(&name, 0, sizeof(name));
  name.sin_family = AF_INET;
  // htons: host to network short
  // htonl: host to network long
  name.sin_port = htons(*port);
  // 通过将 0 赋给 my_addr.sin_port，你告诉 bind() 自己选择合适的端口。
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  // my_addr.sin_addr.s_addr 设置为 INADDR_ANY，你告诉它自动填上它所运行的机器的 IP 地址。

  if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    error_die("bind");

  // 如果 bind 后端口仍为0，调用 getsockname() 获取端口号
  if (*port == 0) {
    socklen_t namelen = sizeof(name);

    // int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    // getsockname 用来获取 sockfd 当前关联的地址,结果存在 addr 指向的空间中。
    // addrlen 应该首先初始化用来表明 addr 指向的空间的大小,调用返回时 addrlen 被修改为套接字地址的实际大小。
    if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
      error_die("getsockname");
    // 这里我们需要再次转换，将网络字节转成本机字节
    *port = ntohs(name.sin_port);
  }

  // 队列最长等待5个客户端
  if (listen(httpd, 5) < 0)
    error_die("listen");

  return httpd;
}


int main(void) {
  int server_sock = -1;
  // server_sock 是用来监听连接的套接字
  u_short port = 0;
  // 因为 port 最大是 65535（16位）， 所以我们定义为 u_short
  int client_sock = -1;
  // client_sock 是用来跟连上的 client 交流的套接字
  struct sockaddr_in client_name;
  // 这个和 sockaddr 是通用的结构, 这是要求接入的信息要去的地方
  socklen_t client_name_len = sizeof(client_name);
  // 注意我们这里之所以用 sizeof(client_name), 后面 accept 中传入 &client_name_len
  // 是因为 addrlen 即是输入也是输出参数， accept 中的最后一个 addrlen 是要用指针

  server_sock = startup(&port);
  printf("httpd running on port %d\n", port);

  while (1) {
    // 连接
    client_sock = accept(server_sock,(struct sockaddr *)&client_name, &client_name_len);
    printf("got acoonnection from %s\n", inet_ntoa(client_name.sin_addr));


    // 错误检查
    if (client_sock == -1)
      error_die("accept");

    accept_request(client_sock);
  }

  close(server_sock);

  return 0;
}
