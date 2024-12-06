// #include<iostream>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>
#include <fmt/format.h>
#include <string.h>
#include <thread>
#include <vector>
#include <map>
#include <cassert>
#include "bytes_const_view.hpp"
#include "expected.hpp"
#include "callback.hpp"

#include <functional>
#include <deque>

using std::cout;
using std::endl;

template <int Except = 0, class T>
T check_error(const char *msg, T res)
{

    if (res == -1)
    {
        if constexpr (Except != 0)
        {
            if (errno == Except)
            {
                return -1;
            }
        }
        cout << msg << ": " << strerror(errno) << endl;
        throw;
    }
    return res;
}

#define SOURCE_INFO_IMPL(file, line) "In " file ":" #line ": "
#define SOURCE_INFO() SOURCE_INFO_IMPL(__FILE__, __LINE__)
#define CHECK_CALL_EXPECT(except, func, ...) check_error<except>(SOURCE_INFO() #func, func(__VA_ARGS__))
#define CHECK_CALL(func, ...) check_error(#func, func(__VA_ARGS__)) // this? how to word?

struct socket_address_fatptr
{
    struct sockaddr *m_addr;
    socklen_t maddrlen;
};
struct socket_address_storage
{
    union
    {
        struct sockaddr m_addr;
        struct sockaddr_storage m_addr_storage; //?
    };
    socklen_t m_addrlen = sizeof(struct sockaddr_storage); // why?

    operator socket_address_fatptr()
    { // what is this? the function is?
        return {&m_addr, m_addrlen};
    }
};

struct address_resolved_entry
{
    struct addrinfo *m_curr = nullptr;

    socket_address_fatptr get_address() const
    {
        return {m_curr->ai_addr, m_curr->ai_addrlen};
    }

    int create_socket() const
    {
        // int sockfd=check_error("socket",socket(m_curr->ai_family, m_curr->ai_socktype, m_curr->ai_protocol));
        int sockfd = CHECK_CALL(socket, m_curr->ai_family, m_curr->ai_socktype, m_curr->ai_protocol);
        return sockfd;
    }

    int create_socket_and_bind() const
    {
        int sockfd = create_socket();
        socket_address_fatptr server_addr = get_address();
        // socket_address_fatptr addr=entry.get_address();//为什么可以让entry.get_address赋值给addr;
        // check_error("bind",bind(sockfd,addr.m_addr,addr.maddrlen));
        CHECK_CALL(bind, sockfd, server_addr.m_addr, server_addr.maddrlen);
        return sockfd;
    }

    [[nodiscard]] bool next_entry()
    {

        m_curr = m_curr->ai_next;
        if (m_curr == nullptr)
        {
            return false;
        }
        else
            return true;
    }
};

struct address_resolver
{
    struct addrinfo *m_head = nullptr;

    address_resolved_entry resolve(std::string const &name, std::string const &service)
    {
        int err = getaddrinfo(name.c_str(), service.c_str(), NULL, &m_head);
        if (err != 0)
        {
            cout << "err :" << gai_strerror(err) << endl;
            // perror("err");
            throw;
        }
        return {m_head};
    }

    address_resolved_entry get_first_entry()
    {
        return {m_head}; // why the m_head can be a address_resolved_entry,and this entry  don`t have the Constructor function
    }

    address_resolver() = default;
    address_resolver(address_resolver &&that) : m_head(that.m_head)
    { // what is that?
        that.m_head = nullptr;
    }

    ~address_resolver()
    {
        if (m_head)
        {
            freeaddrinfo(m_head);
        }
    }
};

using StringMap = std::map<std::string, std::string>;

struct http11_header_parser
{

    std::string m_header;
    std::string m_headerline;

    StringMap m_header_keys;

    std::string m_body;
    bool m_header_finished = false;

    void reset_state()
    {
        m_header.clear();
        m_headerline.clear();
        m_header_keys.clear();
        m_body.clear();
        m_header_finished = false;
    }

    [[nodiscard]] bool header_finished()
    {
        return m_header_finished;
    }

    void _extract_headers()
    {
        size_t pos = m_header.find("\r\n");
        while (pos != std::string::npos)
        {
            pos += 2;
            size_t next_pos = m_header.find("\r\n", pos);
            size_t line_len = std::string::npos;
            if (next_pos != std::string::npos)
            {
                line_len = next_pos - pos;
            }

            std::string_view line = std::string_view(m_header).substr(pos, line_len);
            size_t colon = line.find(": ");
            if (colon != std::string::npos)
            {
                std::string key = std::string(line.substr(0, colon));
                std::string_view value = line.substr(colon + 2);

                for (char &c : key)
                {
                    if ('A' <= c && c <= 'Z')
                    {
                        c += 'a' - 'A';
                    }
                }

                m_header_keys.insert_or_assign(std::move(key), value);
            }
            pos = next_pos;

            // size_t posOfPoint = m_header.find(":", pos);
            // std::string tmp = m_header.substr(pos + 4, posOfPoint);
            // size_t pos = m_header.find("\r\n", pos);
            // std::string the_value = m_header.substr(posOfPoint + 1, pos);
            // m_header_keys[tmp] = the_value;
        }
    }

    void push_chunk(std::string chunk)
    {
        assert(!m_header_finished);
        size_t old_size = m_header.size();
        m_header.append(chunk);
        std::string_view header = m_header;
        // 如果还在解析头部的话，尝试判断头部是否结束
        if (old_size < 4)
        {
            old_size = 4;
        }
        old_size -= 4;
        size_t header_len = header.find("\r\n\r\n", old_size, 4);
        if (header_len != std::string::npos)
        {
            // 头部已经结束
            m_header_finished = true;
            // 把不小心多读取的正文留下
            m_body = header.substr(header_len + 4);
            m_header.resize(header_len);
            // 开始分析头部，尝试提取 Content-length 字段
            _extract_headers();
        }
    }

    StringMap &headers()
    {
        return m_header_keys;
    }

    std::string &headline()
    {
        return m_headerline;
    }

    std::string &extra_body()
    {

        return m_body;
    }

    std::string &header_raw()
    {
        return m_header;
    }
};

template <class HeaderParser = http11_header_parser>
struct http_request_parser
{
    HeaderParser m_header_parser;
    // std::string m_header;
    // std::string m_body;
    // bool m_header_finished =false;
    size_t m_content_length = 0;
    bool m_body_finished = false;
    size_t body_accumulated_size = 0; //?

    void reset_state()
    {
        m_header_parser.reset_state();
        m_content_length = 0;
        body_accumulated_size = 0;
        m_body_finished = false;
    }

    // [[nodiscard]] bool need_more_chunks() const{
    //     return !m_body_finished;
    // }

    [[nodiscard]] bool request_finished()
    {
        return m_body_finished;
    }
    [[nodiscard]] bool header_finished()
    {
        return m_header_parser.header_finished();
    }

    std::string &header_raw()
    {
        return m_header_parser.header_raw(); // weishen me temple zhong de hanshu dou shibie bushang?
    }

    std::string &headline()
    {
        return m_header_parser.headline();
    }

    StringMap &headers()
    {
        return m_header_parser.headers();
    }

    std::string _headline_first()
    { ///////////////////////////cv
        // "GET / HTTP/1.1" request
        // "HTTP/1.1 200 OK" response
        auto &line = headline();
        size_t space = line.find(' ');
        if (space == std::string::npos)
        {
            return "";
        }
        return line.substr(0, space);
    }

    std::string _headline_second()
    { ////////////////////////////////cv
        // "GET / HTTP/1.1"
        auto &line = headline();
        size_t space1 = line.find(' ');
        if (space1 == std::string::npos)
        {
            return "";
        }
        size_t space2 = line.find(' ', space1 + 1);
        if (space2 == std::string::npos)
        {
            return "";
        }
        return line.substr(space1 + 1, space2 - (space1 + 1));
    }

    std::string _headline_third()
    { //////////////////////////////cv
        // "GET / HTTP/1.1"
        auto &line = headline();
        size_t space1 = line.find(' ');
        if (space1 == std::string::npos)
        {
            return "";
        }
        size_t space2 = line.find(' ', space1 + 1);
        if (space2 == std::string::npos)
        {
            return "";
        }
        return line.substr(space2);
    }

    // std::string method(){
    //     auto &headline =m_header_parser.header()///////////have posblem
    //     size_t space =headerline.find(' ');
    //     if (space==std::string::npos)
    //     {

    //         return "GET";
    //     }
    //      return headline.substr(0,space);
    // }

    // std::string url(){
    //     auto  & headline=m_header_parser.headerline();
    //     size_t space1 =headerline.find(' ');
    //     if(space1!=std::string::npos)//why?
    //     {
    //         return "";
    //     }
    //     size_t space2=headerline.find(' ',space1);
    //     if(space2!=std::string::npos)
    //     {
    //         return "";
    //     }
    //     return headerline.substr(space1,space2);
    // }

    std::string &body()
    {
        return m_header_parser.extra_body();
    }

    size_t _extract_content_length()
    {
        auto &headers = m_header_parser.headers();
        auto it = headers.find("contect-length");
        if (it == headers.end())
        {
            return 0;
        }
        try
        {
            return std::stoi(it->second);
        }
        catch (std::invalid_argument const &)
        {
            return 0;
        }
    }

    void push_chunk(std::string chunk)
    {

        // if (!m_header_finished)
        // {
        //     m_header.append(chunk);
        //     size_t header_len=m_header.find("\r\n\r\n");
        //     if (header_len!=std::string::npos)
        //     {
        //         m_header_finished=true;
        //         m_body.append(m_header.substr(header_len+4));
        //         m_header.resize(header_len);
        //         size_t body_length=0;
        //         if (m_body.size()>=body_length)
        //         {
        //             m_body_finished=true;
        //         }
        //     }
        // }
        // else{
        //     m_body.append(chunk);
        // }

        if (!m_header_parser.header_finished())
        {

            m_header_parser.push_chunk(chunk);
            if (m_header_parser.header_finished())
            {
                body_accumulated_size = body().size();
                m_content_length = _extract_content_length();

                if (body_accumulated_size >= m_content_length)
                {
                    m_body_finished = true;
                    // body().resize(m_content_length);
                }
            }
        }
        else
        {
            body().append(chunk);
            if (body_accumulated_size >= m_content_length)
            {
                m_body_finished = true;
                // body().resize(m_content_length);
            }
        }
    }

    std::string read_some_body()
    { //
        return std::move(body());
    }
};

struct http11_header_writer
{

    std::string m_buffer;

    void reset_state()
    {
        m_buffer.clear();
    }

    std::string &buffer()
    {
        return m_buffer;
    }

    void begin_header(std::string_view first, std::string_view second, std::string_view third)
    {
        m_buffer.append(first);
        m_buffer.append(" ");
        m_buffer.append(second);
        m_buffer.append(" ");
        m_buffer.append(third);
    }

    void write_header(std::string_view key, std::string_view value)
    {
        m_buffer.append("\r\n");
        m_buffer.append(key);
        m_buffer.append(": ");
        m_buffer.append(value);
    }

    void end_header()
    {
        m_buffer.append("\r\n\r\n");
    }
};

template <class HeaderWriter = http11_header_writer>
struct http_response_writer
{
    HeaderWriter m_header_writer;

    void begin_header(std::string_view first, std::string_view second, std::string_view third)
    {
        m_header_writer.begin_header(first, second, third);
    }

    void reset_state()
    {
        m_header_writer.reset_state();
    }

    std::string &buffer()
    {
        return m_header_writer.buffer();
    }

    void write_header(std::string_view key, std::string_view value)
    {
        m_header_writer.write_header(key, value);
    }

    void end_header()
    {
        m_header_writer.end_header();
    }

    void write_body(std::string_view body)
    {
        m_header_writer.buffer().append(body);
    }
};

//------------------async-------------------

// std::vector<std::thread> pool;

// template <class  ...Args>
// using callback = std::function<void(Args...)>;

int epollfd;

std::map<int, callback<>> to_be_called_later;

int topid;

struct async_file
{
    int m_fd;

    static async_file async_wrap(int fd)
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags < 0)
        {
            throw std::runtime_error("fcntl failed");
        }
        flags |= O_NONBLOCK; // 修改文件句柄的类似为非阻塞
        CHECK_CALL(fcntl, fd, F_SETFL, flags);

        // async
        epollfd = epoll_create1(0);
        if (epollfd < 0)
        {
            perror("epoll_create1");
            exit(EXIT_FAILURE);
        }

        struct epoll_event event;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

        return async_file{fd};
    }

    ssize_t sync_read(char buf[])
    {
        return CHECK_CALL(read, m_fd, buf, sizeof(*buf));
        // return CHECK_CALL_EXPECT(EAGAIN, read, m_fd, buf, sizeof(*buf));
    }

    void async_read(char buf[], callback<ssize_t> cb)
    {
        ssize_t ret;
        std::string buf_str(buf);

        ret = CHECK_CALL_EXPECT(EAGAIN, read, m_fd, buf, sizeof(buf_str.data()));
        if (ret != -1)
        {
            cb(ret);
            return;
        }

        // async
        // epollfd = epoll_create1(0);
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.u64 = m_fd;

        epoll_ctl(epollfd, EPOLL_CTL_MOD, m_fd, &event);

        // to_be_called_later[m_fd] = ([this, buf, cb = std::move(cb)]() mutable { // ret值不正常，所以，回调函数参数为空，不返回ret；
        //     async_read(buf, std::move(cb));                                     // mutable???  1：51
        // });

        to_be_called_later[m_fd] = [this, buf, cb = std::move(cb)]() mutable {
        async_read(buf, std::move(cb)); // 当事件触发时再次调用 async_read
    };

        // topid++;
        // cb(ret);
    }

    ssize_t sync_write(std::string &buf)
    {
        return CHECK_CALL(write, m_fd, buf.data(), buf.size());
    }

    void async_write(std::string buf, callback<ssize_t> cb)
    {
        // ssize_t ret= CHECK_CALL(write,m_fd,buf.data(),buf.size());
        size_t ret;
        do
        {
            ret = CHECK_CALL_EXPECT(EAGAIN, write, m_fd, buf.data(), buf.size());
        } while (ret == -1);
        cb(ret);
    }

    int sync_accept(socket_address_storage &addr)
    {
        return CHECK_CALL(accept, m_fd, &addr.m_addr, &addr.m_addrlen);
    }

    void async_accept(socket_address_storage &addr, callback<int> cb)
    {
        ssize_t ret;
        ret = CHECK_CALL_EXPECT(EAGAIN, accept, m_fd, &addr.m_addr, &addr.m_addrlen);
        if (ret != -1)
        {
            cb(ret);
            return;
        }

        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        event.data.u64 = m_fd;

        epoll_ctl(epollfd, EPOLL_CTL_MOD, m_fd, &event);

        // to_be_called_later[m_fd] = ([this, &addr, cb = std::move(cb)]() mutable { // ret值不正常，所以，回调函数参数为空，不返回ret；
        //     async_accept(addr, std::move(cb));                                    // mutable???  1：51
        // });

        // 将回调函数存储到 to_be_called_later 中
        to_be_called_later[m_fd] = [this, &addr, cb = std::move(cb)]() mutable
        {
            async_accept(addr, std::move(cb)); // 当事件触发时再次调用 async_accept
        };
    }

    void close_file()
    {

        epoll_ctl(epollfd, EPOLL_CTL_DEL, m_fd, nullptr);
        close(m_fd);
    }
};

struct http_connection_handler
{
    async_file m_conn;
    http_request_parser<> m_req_parser;
    char m_buf[1024];

    void do_start(int connfd)
    {
        m_conn = async_file::async_wrap(connfd);
        // m_req_parser.reset_state();

        do_read();
    }

    void do_read()
    {
        m_conn.async_read(m_buf, [this](ssize_t n)
                          {

        if (n == 0)
        {
            fmt::println("主动关闭了连接");
            do_close();
            return;
        }
        //fmt::println("读取到 {} 个字节",n);
        m_req_parser.push_chunk(std::string(m_buf, n));
        if (!m_req_parser.request_finished())
        {
            do_read();
        }else{
            do_write();
        } });
    }

    void do_write()
    {
        std::string body = std::move(m_req_parser.body());
        m_req_parser.reset_state();

        if (body.empty())
        {
            body = "the body is null123";
            /* code */
        }
        else
        {
            body = "hello! you request is: [ " + body + " ]";
        }

        fmt::println("正在响应请求");

        http_response_writer res_writer;
        res_writer.begin_header("HTTP/1.1", "200", "OK");
        res_writer.write_header("Server", "co_http");
        res_writer.write_header("Content-type", "text/html;charset=utf-8");
        res_writer.write_header("Connection", "keep-alive");
        res_writer.write_header("COntent-length", std::to_string(body.size()));
        res_writer.end_header();
        auto buffer = res_writer.buffer();

        m_conn.sync_write(buffer);
        m_conn.sync_write(body);
        fmt::println("正在响应");

        do_read();
    }

    void do_close()
    {
        fmt::println("close the connid");
        m_conn.close_file();

        delete this;
    }
};

struct http_connection_acceptor
{

    async_file m_listen;
    socket_address_storage m_addr;

    void do_start(std::string name, std::string port)
    {
        address_resolver resolver;

        fmt::println("正在监听: {},{}", name, port);
        auto entry = resolver.resolve(name, port);
        int listenfd = entry.create_socket_and_bind();
        CHECK_CALL(listen,listenfd,SOMAXCONN);
        //fmt::println("the end of create socket and bind ");
        m_listen = async_file::async_wrap(listenfd);
        do_accept();
    }

    void do_accept()
    {
        m_listen.async_accept(m_addr, [this](int connfd)
                              {
                                  fmt::println("接受了一个连接: {}", connfd);
                                  auto conn_handler = new http_connection_handler{};
                                  conn_handler->do_start(connfd);
                                  do_accept();
                              });
    }
};



void event_loop()
{
    struct epoll_event events[10];
    while (true)
    {
        int nfds = epoll_wait(epollfd, events, 10, -1);
        if (nfds == -1)
        {
            perror("epoll_wait failed");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.u64;
            if (to_be_called_later.count(fd))
            {
                // 获取回调并执行
                auto cb = std::move(to_be_called_later[fd]);
                to_be_called_later.erase(fd);
                cb(); // 执行回调
            }
        }
    }
}

void event_loop567()
{
    std::vector<struct epoll_event> events(10);
    while (true)
    {
        int num_events = epoll_wait(epollfd, events.data(), events.size(), -1); // 阻塞等待事件
        if (num_events == -1)
        {
            if (errno == EINTR)
                continue;  // 被信号中断时继续
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < num_events; ++i)
        {
            // 获取文件描述符
            int fd = events[i].data.u64;

            // 判断是否有事件触发了
            if (events[i].events )
            {
                if (to_be_called_later.find(fd) != to_be_called_later.end())
                {
                    // 如果有对应的回调，调用它
                    to_be_called_later[fd](); 
                    to_be_called_later.erase(fd);  // 调用后删除回调
                }
            }
            // 这里可以继续添加其他事件处理逻辑
        }
    }
}



void server()
{
    auto acceptor = new http_connection_acceptor;
    acceptor->do_start("127.0.0.1", "8090");

    struct epoll_event events[10];


    //event_loop();

    while (true)
    {

        int ret = epoll_wait(epollfd, events, 10, -1);
        if (ret < 0)
        {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (size_t i = 0; i < ret; i++)
        {
            auto id = events->data.u64;
            if (to_be_called_later.count(id))
            {
                auto cb = std::move(to_be_called_later[id]);
                to_be_called_later.erase(id);
                cb();
            }
        }
    }



    fmt::println("所有任务完成了");
    close(epollfd);
}

int main()
{

    setlocale(LC_ALL, "zh_CN.UTF-8");

    server();

    cout << "success" << endl;

    return 0;
}

