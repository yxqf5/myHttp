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
#include<vector>
#include<map>
#include <cassert>
#include "bytes_const_view"


using std::cout;
using std::endl;

int check_error(const char* msg,int res){

    if (res==-1)
    {
        cout<<msg<<": "<<strerror(errno)<<endl;
        throw;
    }
    return res;
    
}

#define CHECK_CALL(func, ...) check_error(#func,func(__VA_ARGS__))// this? how to word?


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
        struct sockaddr_storage m_addr_storage;//?
        
    };
    socklen_t m_addrlen=sizeof(struct sockaddr_storage);//why?

    operator socket_address_fatptr(){//what is this? the function is?
        return  {&m_addr,m_addrlen};
    }    
};


struct address_resolved_entry
{
    struct addrinfo *m_curr = nullptr;

    socket_address_fatptr get_address() const
    {
        return {m_curr->ai_addr, m_curr->ai_addrlen};
    }

    int create_socket() const{
        // int sockfd=check_error("socket",socket(m_curr->ai_family, m_curr->ai_socktype, m_curr->ai_protocol));
        int sockfd=CHECK_CALL(socket,m_curr->ai_family,m_curr->ai_socktype,m_curr->ai_protocol);
        return  sockfd;
        
    }

    int create_socket_and_bind() const{
        int sockfd=create_socket();
        socket_address_fatptr server_addr= get_address();
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
    struct addrinfo *m_head =nullptr;
    
    address_resolved_entry resolve(std::string const &name, std::string const &service){
        int err=getaddrinfo(name.c_str(),service.c_str(),NULL,&m_head);
        if (err!=0)
        {
            cout<<"err :"<<gai_strerror(err)<<endl;
            //perror("err");
            throw;
        }
        return {m_head};
    }

    address_resolved_entry get_first_entry(){
        return {m_head};//why the m_head can be a address_resolved_entry,and this entry  don`t have the Constructor function
    }


    address_resolver() =default;
    address_resolver(address_resolver && that) : m_head(that.m_head){//what is that?
      that.m_head=nullptr;
    }

    ~address_resolver(){
        if (m_head)
        {
            freeaddrinfo(m_head);
        }
    }

};


using StringMap =std::map<std::string,std::string>;

struct http11_header_parser
{

    std::string m_header;
    std::string m_headerline;

    StringMap m_header_keys;

    std::string m_body;
    bool m_header_finished=false;

    void reset_state(){
        m_header.clear();
        m_headerline.clear();
        m_header_keys.clear();
        m_body.clear();
        m_header_finished=false;
    }

    [[nodiscard]] bool header_finished(){
        return m_header_finished;
    }

    void _extract_headers()
    {
        size_t pos = m_header.find("\r\n");
        while (pos != std::string::npos)
        {
            pos+=2;
            size_t next_pos =m_header.find("\r\n",pos);
            size_t line_len =std::string::npos;
            if (next_pos!=std::string::npos)
            {
                line_len=next_pos-pos;
            }

            std::string_view line=std::string_view(m_header).substr(pos,line_len);
            size_t colon=line.find(": ");
            if (colon!=std::string::npos)
            {
                std::string key=std::string(line.substr(0,colon));
                std::string_view value=line.substr(colon+2);

                for (char &c: key) {
                    if ('A' <= c && c <= 'Z') {
                        c += 'a' - 'A';
                    }
                }

            m_header_keys.insert_or_assign(std::move(key),value);
            }
            pos=next_pos;
            
            // size_t posOfPoint = m_header.find(":", pos);
            // std::string tmp = m_header.substr(pos + 4, posOfPoint);
            // size_t pos = m_header.find("\r\n", pos);
            // std::string the_value = m_header.substr(posOfPoint + 1, pos);
            // m_header_keys[tmp] = the_value;
        }
    }

    void push_chunk(std::string chunk) {
        assert(!m_header_finished);
        size_t old_size = m_header.size();
        m_header.append(chunk);
        std::string_view header = m_header;
        // 如果还在解析头部的话，尝试判断头部是否结束
        if (old_size < 4) {
            old_size = 4;
        }
        old_size -= 4;
        size_t header_len = header.find("\r\n\r\n", old_size, 4);
        if (header_len != std::string::npos) {
            // 头部已经结束
            m_header_finished = true;
            // 把不小心多读取的正文留下
            m_body = header.substr(header_len + 4);
            m_header.resize(header_len);
            // 开始分析头部，尝试提取 Content-length 字段
            _extract_headers();
        }
    }


    StringMap& headers(){
        return m_header_keys;
    }

    std::string &headline(){
        return m_headerline;
    }
    

    std::string &extra_body(){

        return m_body;
    }

    std::string &header_raw(){
        return m_header;
    }


};




template <class HeaderParser = http11_header_parser>
struct http_request_parser
{
    HeaderParser m_header_parser;
    //std::string m_header;
    //std::string m_body;
    //bool m_header_finished =false;
    size_t m_content_length=0;
    bool m_body_finished=false;
        size_t body_accumulated_size = 0;//?

    
    void reset_state() {
        m_header_parser.reset_state();
        m_content_length = 0;
        body_accumulated_size = 0;
        m_body_finished = false;
    }

    // [[nodiscard]] bool need_more_chunks() const{
    //     return !m_body_finished;
    // }

    [[nodiscard]] bool request_finished(){
        return m_body_finished;
    }
   [[nodiscard]] bool header_finished(){
        return m_header_parser.header_finished();
    }

    std::string& header_raw(){
        return m_header_parser.header_raw();//weishen me temple zhong de hanshu dou shibie bushang?
     }

        std::string &headline() {
        return m_header_parser.headline();
    }

    StringMap &headers(){
        return m_header_parser.headers();


    }


    std::string _headline_first() {///////////////////////////cv
        // "GET / HTTP/1.1" request
        // "HTTP/1.1 200 OK" response
        auto &line = headline();
        size_t space = line.find(' ');
        if (space == std::string::npos) {
            return "";
        }
        return line.substr(0, space);
    }

    std::string _headline_second() {////////////////////////////////cv
        // "GET / HTTP/1.1"
        auto &line = headline();
        size_t space1 = line.find(' ');
        if (space1 == std::string::npos) {
            return "";
        }
        size_t space2 = line.find(' ', space1 + 1);
        if (space2 == std::string::npos) {
            return "";
        }
        return line.substr(space1 + 1, space2 - (space1 + 1));
    }

    std::string _headline_third() {//////////////////////////////cv
        // "GET / HTTP/1.1"
        auto &line = headline();
        size_t space1 = line.find(' ');
        if (space1 == std::string::npos) {
            return "";
        }
        size_t space2 = line.find(' ', space1 + 1);
        if (space2 == std::string::npos) {
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


    std::string &body(){
        return m_header_parser.extra_body();
    }



    size_t _extract_content_length(){
        auto &headers=m_header_parser.headers();
        auto it=headers.find("contect-length");
        if(it==headers.end())
        {
            return 0;
        }
        try
        {
            return std::stoi(it->second);
        }
        catch(std::invalid_argument const &)
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
                    //body().resize(m_content_length);
           
                }
            }
        }
        else
        {
            body().append(chunk);
            if (body_accumulated_size >= m_content_length)
            {
                m_body_finished = true;
                //body().resize(m_content_length);
                
            }
         
        }
    }

    std::string read_some_body() {
        return std::move(body());
    }


};



std::vector<std::thread> pool;


int main(){

std::string str="sadfasdf";
fmt::println("{}",str);
    setlocale(LC_ALL,"zh_CN.UTF-8");
    address_resolver resolver;
    cout<<"正在监听: 127.0.0.1:8090"<<endl;
    auto entry=resolver.resolve("127.0.0.1","8090");
    int listenfd=entry.create_socket_and_bind();
    // check_error("listen",listen(sockfd,SOMAXCONN));
    CHECK_CALL(listen,listenfd,SOMAXCONN);







    while (true)
    {

    socket_address_storage addr;
    int connid= CHECK_CALL(accept,listenfd,&addr.m_addr,&addr.m_addrlen);
    pool.push_back(std::thread([connid]{//? 3
        char buf[1024];

        http_request_parser req_parser;

        do
        {
        size_t n=CHECK_CALL(read,connid,buf,sizeof(buf));
        req_parser.push_chunk(std::string(buf,n));
        } while (!req_parser.header_finished());
        std::string body=req_parser.body();
        fmt::println("the request header: {}",req_parser.header_raw());
        fmt::println("the request body: {}",req_parser.body());

        std::cout<<"---------------------------------------------------------------------------------"<<endl;

        // cout<<"收到的请求: "<<req_parser.m_header<<'\n';
        // cout<<"the request body: "<<req_parser.m_body<<'\n';
        // std::string body=req_parser.m_body;


        // size_t n = CHECK_CALL(read, connid, buf, sizeof(buf));
        // std::string req(buf, n);
        // cout << "the req : " << req << "\n";

         std::string body1="nihao,yxqf!";
        std::string res="HTTP/1.1 200 OK\r\nServer: co_http\r\nConnection: close\r\nContent-length: "+std::to_string((size_t)9+body1.size())+"\r\n\r\nHelloword"+body1;
         cout<<"我的回答: "<<res<<'\n';
         CHECK_CALL(write,connid,res.data(),res.size());
        close(connid);
    }));
    }

    for(auto&t: pool){
        t.join();
    }
cout<<"success"<<endl;


    return 0;
}

int main1()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    struct addrinfo *addrinfo;
    int err = getaddrinfo("127.0.0.1", "80", NULL, &addrinfo);
    if (err != 0)
    {
        // perror("getaddrinfo: ");
        cout << "getaddrinfo: " << gai_strerror(err) << err << endl;
        return 1;
    }

    for (struct addrinfo *current = addrinfo; current != NULL; current = current->ai_next)
    {

        cout << "ai_family: " << current->ai_family << endl;
        cout << "ai_socktype: " << current->ai_socktype << endl;
        cout << "ai_protocol: " << current->ai_protocol << endl;
    }

    //    cout<<"ai_family: "<<addrinfo->ai_family<<endl;
    //    cout<<"ai_socktype: "<<addrinfo->ai_socktype<<endl;
    //    cout<<"ai_protocol: "<<addrinfo->ai_protocol<<endl;

    int sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (sockfd == -1)
    {
        // perror("socket");
        EINVAL;
        EOWNERDEAD;
        perror("socket: ");
        // cout<<"errno: "<<strerror(errno)<<endl;
        return 1;
    }

    //    int flags=fcntl(sockfd,F_GETFL);
    //    if(flags==-1){
    //         perror("fcntl");
    //         return 1;
    //    }
    //     flags |=O_NONBLOCK;
    //     res=fcntl(sockfd,F_SETFL,flags);
    //     if (res==-1)
    //     {
    //         perror("fcntl");
    //         return 1;
    //     }

    //     int epollfd =epoll_create1(0);
    //     if (epollfd==-1)
    //     {
    //         perror("epoll_create1");
    //         return 1;
    //     }

    std::cout << "the_hwerttp" << std::endl;

    return 0;
}