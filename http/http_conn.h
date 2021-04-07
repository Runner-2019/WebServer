#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn {
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,            // 400
        NO_RESOURCE,            // 404
        FORBIDDEN_REQUEST,      // 403
        FILE_REQUEST,
        INTERNAL_ERROR,         // 500
        CLOSED_CONNECTION
    };
    enum LINE_STATUS {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    // 初始化
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    void initmysql_result(connection_pool *connPool);               // 从数据库中取出现存的账号和密码


    // 读取数据
    bool read_once();

    // 先读后写
    void process();

    // 发送响应
    bool write();

    // 关闭连接
    void close_conn(bool real_close = true);

    sockaddr_in *get_address() {
        return &m_address;
    }


    int timer_flag;         //
    int improv;



private:

    void init();            // 初始化
    void unmap();           // 发送完响应体后，unmmap文件地址

    // 解析读
    HTTP_CODE process_read();
    char *get_line() { return m_read_buf + m_start_line; };
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    LINE_STATUS parse_line();

    // 生成请求
    HTTP_CODE do_request();

    // 写响应
    bool process_write(HTTP_CODE ret);
    bool add_response(const char *format, ...); ////生成每行响应体
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;                       // epollfd
    static int m_user_count;                    // 当前连接的用户数
    MYSQL *mysql;                               // 一个数据库连接，用于写日志
    int m_state;                                // 读为0, 写为1

private:
    int m_sockfd;                               //  套接字
    sockaddr_in m_address;                      // 客户地址
    int m_content_length;
    int cgi;                                    // 是否启用的POST
    char *m_string;                             // 存储请求头数据

    // 读取数据
    int m_TRIGMode;                             // 初始化时传入，设置 listenfd（这里当然没有） or connfd 为 ET or LT


    // 有限状态机，解析数据
    char m_read_buf[READ_BUFFER_SIZE];          // 读缓冲区
    int m_read_idx;                             // 所有读到的数据的索引，在read_once()中生成
    int m_checked_idx;                          // 我们检查到哪里了
    int m_start_line;                           // 标志新行的开始位置，其值由m_checked_idx 提供
    CHECK_STATE m_check_state;                  // 主状态

    // 根据数据处理请求
    METHOD m_method;                            // 请求方法
    char m_real_file[FILENAME_LEN];             // 实际文件的地址
    char *m_url;                                // 请求的url
    char *m_version;                            // http版本
    char *m_host;                               // host
    bool m_linger;                              // 是否长连接，生成响应体时还会用到
    char *m_file_address;                       // 用户所请求的文件的mmap映射地址，生成响应体时还会用到
    struct stat m_file_stat;                    // 用户所请求文件的属性
    char *doc_root;                             // 用户所请求文件的根目录，即网站的根目录

    // 生成响应
    char m_write_buf[WRITE_BUFFER_SIZE];        // 写缓冲区，响应消息在这里暂存
    int m_write_idx;                            // 写到哪里了，写完时就是bytes_to_send
    struct iovec m_iv[2];                       // 最后在这里发送，因为有文件的存在，所以直接映射到m_write_buf里并不好
    int m_iv_count;                             // 缓冲区的长度
    int bytes_to_send;                          // 需要发送的响应体的总长度，在生成响应体时生成，用于write()函数实际发送

    // 发送响应
    int bytes_have_send;                        // 已经发送的字节数量


    map <string, string> m_users;

    int m_close_log;                            // 是否关闭日志

    char sql_user[100];                         // 登录sql的用户名
    char sql_passwd[100];                       // 登录sql的密码
    char sql_name[100];                         // 登录的sql的name
};

#endif
