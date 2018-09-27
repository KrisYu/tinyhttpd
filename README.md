

Trying to understand the famous 'tinyhttpd'.

### prerequisite：


免费资料：

- [socket 简单编程指南](https://www.cnblogs.com/life2refuel/p/5240175.html#undefined) 或者 [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) 前六章

或者书：

- 深入理解计算机操作系统 第十二章

### HTTP 请求

```
> telnet checkip.dyndns.org 80
Trying 216.146.43.70...
Connected to checkip.dyndns.com.
Escape character is '^]'.
> GET / HTTP/1.1
> host: checkip.dyndns.org
// 上面两行是输入，同时输完记得按两次 Enter

HTTP/1.1 200 OK
Content-Type: text/html
Server: DynDNS-CheckIP/1.0.1
Connection: close
Cache-Control: no-cache
Pragma: no-cache
Content-Length: 105

<html><head><title>Current IP Check</title></head><body>Current IP Address: xxx.xxx.xxx.xxx</body></html>
Connection closed by foreign host.
```

请求行： `<method><uri><version> `

👉 `GET / HTTP/1.1`

请求报头： `<header name>: <header data>`

👉 `host: checkip.dyndns.org`



### HTTP 响应


响应行: `<version> <status code> <status message>`

👉 `HTTP/1.1 200 OK`


此外还有很多附加信息，但最重要的是：

👉 `Content-Type: text/html`

👉 `Content-Length: 105`

### 从 socket 读数据：

调用 recv() 从 socket 读取数据：

```
int recv(int sockfd, void *buf, int len, unsigned int flags);

// sockfd 是要读的套接字描述符。
// buf 是要读的信息的缓冲。
// len 是缓冲的最大长度。
// flags 可以设置为 0。(请参考 recv() 的 man page。)

<读了几个字节> = recv(<描述符>， <缓冲区>，<要读取几个字符>， 0)；
```

要点：

- 字符串不以 \0 结尾
- 当用户在telnet输入文本时，字符串以 \r\n 结尾
- recv() 将返回字符个数，如果发生错误就返回 -1, 如果客户端关闭了连接，就返回0
- recv() 调用不一定能一次接收到所有字符

### DEBUG:

`$ lsof -i tcp:4000`

// ls open files internet address

看看 4000 端口的进程是否正在运行。

`$ curl localhost:4000`

看返回结果



### 两种方式提供内容



- 取一个磁盘文件，并将它的内容返回给客户端。磁盘文件称为静态内容，而返回文件给客户端的过程称为服务静态内容。
- 运行一个可执行文件，并将它的输出返回给客户端。运行时可执行文件产生的输出称为动态内容，而运行程序并返回它的输出到客户端的过程称为服务动态内容。

举例

- 静态内容： http://www.aol.com:80/index.html
- 可执行文件：http://kittyhawk.cmcl.cs.cmu.edu:8000/cgi-bin/adder?15000&213  ? 用来分隔文件名和参数



### 服务静态内容



在 `get_example` 中 只实现了 get 相关的函数，用来服务静态内容，其实关于 cgi 的部分都可以删掉。

运行 `gcc httpd_clone1.c -o http_clone1` 既可测试 。



### 服务动态内容



通用网关接口（Common Gateway Interface/CGI）是一种重要的互联网技术，可以让一个客户端，从网页浏览器向执行在网络服务器上的程序请求数据。CGI描述了服务器和请求处理程序之间传输数据的一种标准。

实际上 CGI 就是为了解决服务动态内容而产生的。


动态的 cgi 调用花了我一点时间，因为发现这个 cgi 脚本第一行是：

`#!/usr/local/bin/perl -Tw`

需要把它换成对应的 *nix 系统 perl 所在位置:

``#!/usr/bin/perl -Tw`

然后还需要让 cgi 文件可读可写可执行。



**Test cgi - works on my Raspbian.**



