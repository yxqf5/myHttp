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

#define CHECK_CALL(func, ...) check_error(#func,func(__VA_ARGS__))


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

std::vector<std::thread> pool;


int main(){

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
        size_t n=CHECK_CALL(read,connid,buf,sizeof(buf));

        auto req=std::string(buf,n);
        cout<<"收到的请求: "<<req<<endl;
        std::string res="HTTP/1.1 200 OK\r\nServer: co_http\r\nConnection: close\r\nContent-length: 9\r\n\r\nHelloword";
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