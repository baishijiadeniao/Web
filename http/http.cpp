#include "http.h"
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string,string> users;

int setnonblocking(int fd){
    //获取文件的flags，即open函数的第二个参数
    int old_option=fcntl(fd,F_GETFL);
    //增加文件的某个flags，比如文件是阻塞的，想设置成非阻塞
    int new_option=old_option | O_NONBLOCK;
    //设置文件的flags
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void http_conn::initmysql_result(connection_pool *connPool){
    MYSQL *mysql=NULL;
    connectionRAII mysqlcon(&mysql,connPool);

    if(mysql_query(mysql,"SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT error:%s",mysql_error(mysql));
    }

    MYSQL_RES* result=mysql_store_result(mysql);

    int num_fields= mysql_num_fields(result);
    MYSQL_FIELD* fields=mysql_fetch_fields(result);
    while(MYSQL_ROW row=mysql_fetch_row(result)){
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1]=temp2;
    }

}

void addfd(int epollfd,int fd,bool one_shot,int TRIGmode){
    struct epoll_event event;
    event.data.fd=fd;
    if(TRIGmode==1){
        event.events= EPOLLIN | EPOLLET | EPOLLHUP;
    }else{
        event.events= EPOLLIN | EPOLLHUP;
    }
    if(one_shot){
        event.events |=EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd,int fd,int ev,int TRIGMode){
    struct epoll_event events;
    events.data.fd=fd;
    if(TRIGMode==1)
        events.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        events.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&events);
}

//初始化
void http_conn::init(int sockfd,const sockaddr_in &addr,char* root,int TRIGmode,int close_log,string user,string passwd,string sqlname){
    m_sockfd=sockfd;
    m_address=addr;
    addfd(m_epollfd,m_sockfd,true,m_TRIGMode);
    m_user_count++;
    doc_root=root;
    m_close_log=close_log;
    m_TRIGMode=TRIGmode;
    strcpy(sql_user,user.c_str());
    strcpy(sql_passwd,passwd.c_str());
    strcpy(sql_name,sqlname.c_str());
    init();
}

//接受新的连接
void http_conn::init(){
    mysql=NULL;
    m_linger=false;
    m_method=GET;
    m_write_idx=0;
    m_read_idx=0;
    m_start_line=0;
    m_content_length=0;
    
    cgi=0;
    bytes_have_send=0;
    bytes_to_send=0;
    m_state=0;
    improv=0;
    timer_flag=0;
    m_check_state=CHECK_STATE_REQUESTLINE;
    m_version=0;       //字符串初始化为‘0’，char *a=0是指给a所指的位置赋值'\0'，与char *a='\0'是一个意思
    m_url=0;
    m_host=0;
    m_checked_idx = 0;
    memset(m_read_buf,'\0',READ_BUFFER_SIZE);
    memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
    memset(m_real_file,'\0',FILENAME_LEN);
}

http_conn::LINE_STATE http_conn::parse_line(){
    // 将要分析的字节
    char temp;
    // m_read_idx：读缓冲区中数据的最后一个字节的下一个位置
    // m_checked_idx：指向从状态机当前正在分析的字节，最终指向读缓冲区下一行的开头
    for(;m_checked_idx<m_read_idx;m_checked_idx++){
        temp=m_read_buf[m_checked_idx];
        //http报文每行最后两个字符是\r\n
        if(temp == '\r'){
            // 完整读取一行
            if(m_read_buf[m_checked_idx+1] == '\n'){
                // 将'\r\n'改为'\0\0'
                m_read_buf[m_checked_idx++]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            // 下一个字符的位置是读缓冲区末尾，报文不完整
            else if(m_checked_idx+1== m_read_idx){
                return LINE_OPEN;
            }
            //语法有误
            else{
                return LINE_BAD;
            }
        }
        //当前字符是'\n'，则有可能读取到完整行
        // 上次读取到'\r'就到读缓冲区末尾了，没有接收完整，再次接收时会出现这种情况
        else if(temp=='\n'){
            // 前一个字符是'\r'
            if(m_checked_idx>1 && m_read_buf[m_checked_idx-1]=='\r'){
                // 将'\r\n'改为'\0\0'
                m_read_buf[m_checked_idx-1]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            //语法有误
            else{
                return LINE_BAD;
            }
        }
    }
    LOG_INFO("debug: %s","line_open2");
    // 没有找到'\r\n'，读取的行不完整
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char* text){
    LOG_INFO("%s",text);
    // 返回请求行中最先含有 空格 或 '\t'的位置
    m_url = strpbrk(text," \t");       //  \t前面要加空格
    //没有空格或\t
    if(!m_url){
        return BAD_REQUEST;
    }
    //将该类型改为\0
    *m_url++='\0';
    //strcasecmp可以忽略大小写
    if(strcasecmp(text,"GET")==0){
        m_method=GET;
    }else if(strcasecmp(text,"POST")==0){
        m_method=POST;
        cgi=1;            //
    }else{
        return BAD_REQUEST;
    }
    m_url+=strspn(m_url," \t");
    m_version=strpbrk(m_url," \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++='\0';
    m_version+=strspn(m_version," \t");
    if(strcasecmp(m_version,"HTTP/1.1")!=0){
        return BAD_REQUEST;
    }
    //只比较前7个字符
    if(strncasecmp(m_url,"http://",7)==0){
        m_url+=7;
        // 返回首次出现'/'的位置的指针
        m_url= strchr(m_url,'/');          //作用和strpbrk一样，只是第二个参数是int
    }else if(strncasecmp(m_url,"https://",8)==0){
        m_url+=8;
        m_url=strchr(m_url,'/');
    }
    if(!m_url || m_url[0] !='/'){
        return BAD_REQUEST;
    }
    if(strlen(m_url)==1){
        strcat(m_url,"judge.html");
    }
    //请求行处理完，将主机状态机转为处理请求头
    m_check_state=CHECK_STATE_HEADER;
    return NO_REQUEST;
}


//解析请求头
http_conn::HTTP_CODE http_conn::parse_header(char* text){
    LOG_INFO("debug: header:%s",text);
    //处理空行
    if(text[0]== '\0'){
        if(m_content_length !=0){
            //POST请求，将主机状态转为处理消息体
            m_check_state=CHECK_STAET_CONTENT;
            //请求不完整
            return NO_REQUEST;
        }else{
            return GET_REQUEST;
        }
    }else if(strncasecmp(text,"Connection:",11)==0){
        text+=11;
        //跳过/t与空格
        text+=strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0){
            m_linger=1;
        }
    }else if(strncasecmp(text,"Content-length:",15)==0){
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text);
    }else if(strncasecmp(text,"Host:",5)==0){
        text+=5;
        text+=strspn(text," \t");
        m_host=text;
    }else{
        LOG_INFO("Unknown Request Field: %s",text);
    }
    return NO_REQUEST;
}

//解析请求体
http_conn::HTTP_CODE http_conn::parse_content(char* text){
    if(m_read_idx>=m_content_length+m_checked_idx){
        // *text+m_content_length='\0';                 //不懂
        text[m_content_length]='\0';
        //账户名和密码
        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//解析读缓冲区中的请求体
http_conn::HTTP_CODE http_conn::process_read(){
    //初始化从状态机状态：完整读取一行
    LINE_STATE line_stat=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;         //可以这样初始化的
    //不懂为啥读取内容这里的line_stat要为LINE_OK
    LOG_INFO("debug m_check_state=%d line_stat=%d");
    //(主状态机状态 == 解析消息体 && 从状态机状态 == 完整读取一行) || (返回的从状态机状态 == 完整读取一行)
    //请求体可能很长，一次读取不完
    while((m_check_state==CHECK_STAET_CONTENT && line_stat==LINE_OK) || ((line_stat= parse_line())==LINE_OK)){
        //将指针向后偏移，指向未处理的字符
        text=get_line();
        // m_checked_idx：指向从状态机当前正在分析的字节 -> 最终指向下一行的开头
        m_start_line = m_checked_idx;

        switch (m_check_state){
        //解析请求行
        case CHECK_STATE_REQUESTLINE:
        {
            ret=parse_request_line(text);
            if(ret==BAD_REQUEST){
                return BAD_REQUEST;
            }
            break; //这里的break是退出switch而不是退出while
        }
        //解析请求头                    
        case CHECK_STATE_HEADER:
        {
            ret=parse_header(text);
            if(ret==BAD_REQUEST){
                return BAD_REQUEST;
            }
            //完整解析get请求
            if(ret==GET_REQUEST){
                LOG_INFO("debug: %s","do_request");
                return do_request();
            }
            break;
        }
        //解析请求体
        case CHECK_STAET_CONTENT:
        {
            ret=parse_content(text);
            if(ret==BAD_REQUEST){
                return BAD_REQUEST;
            }
            //响应函数
            if(ret==GET_REQUEST){
                LOG_INFO("debug: %s","CONTENT");
                return do_request();
            }
            line_stat=LINE_OPEN;       
            break;
        }
        //服务器内部错误
        default:
            return INTERNAL_REQUEST;
        }
    }
    LOG_INFO("debug:line_stat=%d",line_stat)
    LOG_INFO("debug: %s","no_request none");
    return NO_REQUEST;
}


bool http_conn::add_response(const char* format,...){
    // 写缓冲区中数据的最后一个字节的下一个位置 >= 写缓冲区大小
    if(m_write_idx>=WRITE_BUFFER_SIZE){
        return false;
    }
    //定义可变参数列表
    va_list arg_list;
    va_start(arg_list,format);
    //执行成功，返回最终生成字符串的长度，若生成字符串的长度大于size，
    //则将字符串的前size个字符复制到str，同时将原串的长度返回（不包含终止符）;注意是原串长度
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    if(len>=WRITE_BUFFER_SIZE-1-m_write_idx){
        va_end(arg_list);
        return false;
    }
    m_write_idx+=len;
    va_end(arg_list);
    LOG_INFO("Response: %s",m_write_buf);
    return true;
}


bool http_conn::add_status_line(int status,const char* title){
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool http_conn::add_headers(int content_len){
    return add_content_len(content_len) && add_linger() && add_blank_line();
}

bool http_conn::add_content_len(int content_len){
    return add_response("Content-Length:%d\r\n",content_len);
}

bool http_conn::add_linger(){
    return add_response("Connection:%s\r\n",m_linger==true?"keep-alive":"close");
}

bool http_conn::add_blank_line(){
    return add_response("\r\n");
}
bool http_conn::add_content_type(){
    return add_response("Content-Type:%s\r\n","text/html");
}

bool http_conn::add_content(const char* content){
    return add_response("%s",content);
}

bool http_conn::process_write(http_conn::HTTP_CODE ret){
    switch (ret)
    {
    //服务器内部错误
    case INTERNAL_REQUEST:
    {
        add_status_line(500,error_500_title);
        add_headers(strlen(error_500_form));
        if(!add_content(error_500_form)){                //不懂为什么这里就要判断异常上面却不用
            return false;
        }
        break;
    }
    //http请求报文有语法错误
    case BAD_REQUEST:
    {
        add_status_line(404,error_404_title);
        add_headers(strlen(error_404_form));
        if(!add_content(error_404_form)){
            return false;
        }
        break;
    }
    // 请求资源禁止访问
    case FORBIDDEN_REQUEST:
    {
        add_status_line(400,error_400_title);
        add_headers(strlen(error_400_form));
        if(!add_content(error_400_form)){
            return false;
        }
        break;
    }
    // 请求资源可以正常访问
    case FILE_REQUEST:
    {
        LOG_INFO("%s","DEBUG: FILEREQUEST");
        add_status_line(200,ok_200_title);
        if(m_file_stat.st_size !=0){
            add_headers(m_file_stat.st_size);
            //使用高级io writev，可用于读写多个非联系的缓冲区
            // 第一个iovec指针指向响应报文缓冲区，长度为m_write_idx
            m_iv[0].iov_base=m_write_buf;       //聚焦写
            m_iv[0].iov_len=m_write_idx;
            m_iv_count=2;
            // 第二个iovec指针指向mmap返回的文件指针，长度为文件大小
            m_iv[1].iov_len=m_file_stat.st_size;
            m_iv[1].iov_base=m_file_address;
            bytes_to_send=m_write_idx+m_file_stat.st_size;
            LOG_INFO("%s","DEBUG: true");
            return true;
        }
        //返回空白html文件
        else{
            const char* ok_form="<html><body></body></html>";
            add_headers(strlen(ok_form));
            if(!add_response(ok_form)){
                return false;
            }
        }
        break;
    }
    default:
        return false;
    }
    // 除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv[0].iov_base=m_write_buf;
    m_iv[0].iov_len=m_write_idx;
    m_iv_count=1;
    bytes_to_send=m_write_idx;
}

//将响应报文从写缓冲区中写出
bool http_conn::write(){
    int temp=0;
    //一般不会出现这种情况
    if(bytes_to_send==0){
        //注册新的读事件
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
        //初始化接受新的连接
        init();
        return true;
    }
    //解决大文件的传输问题，每次传输完成后都要更新下次传输的起始位置和长度
    while(1){
        // 将响应报文的响应行、响应头、空行和响应体发送给浏览器端
        temp=writev(m_sockfd,m_iv,m_iv_count);
        LOG_INFO("debug: temp2=%d",temp);
        if(temp<0){
            //缓冲区满了,只有在套接字是非阻塞的情况下会出现该错误
            //使用write发送数据时采取的方式是循环发送大量的数据；由于是异步的，write\send将要发送的数据提交到发送缓冲区后是立即返回的，
            //并不需要对端确认数据已接收。在这种情况下是很有可能出现发送缓冲区被填满，导致write\send无法再向缓冲区提交要发送的数据。
            if(errno==EAGAIN){
                LOG_INFO("debug: temp=%d",temp);
                // 注册并监听写事件,在此期间无法立即接收到同一用户的下一请求，但可以保证连接的完整性。
                modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);
                return true;
            }
            //释放内存映射区
            unmap();
            return false;
        }
        //更新已发送字节数
        bytes_have_send +=temp;
        //更新要发送字节数
        bytes_to_send -=temp;
        //第一个m_iv的数据已经发送完，发送第二个
        if(bytes_have_send>=m_iv[0].iov_len){
            m_iv[0].iov_len=0;
            m_iv[1].iov_base=m_file_address+bytes_have_send-m_write_idx;
            m_iv[1].iov_len=bytes_to_send;
        }
        //继续发送第一个m_iv的数据
        else{
            m_iv[0].iov_len=m_iv[0].iov_len-bytes_have_send;
            m_iv[0].iov_base=m_write_buf+bytes_have_send;
        }
        //全部数据已发送完
        if(bytes_to_send<=0){
            //释放内存缓冲区
            unmap();
            //注册并监听读事件
            modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
            //浏览器的请求是否为长连接
            if(m_linger){
                LOG_INFO("debug: temp=%s","this way");
                //短连接，初始化
                init();
                return true;
            }else{
                return false;       //不懂，为啥成功发送了也要返回false
            }
        }
    }
}

//解除内存映射
void http_conn::unmap(){
    if(m_file_address){
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    }
}

//响应函数
http_conn::HTTP_CODE http_conn::do_request(){
    strcpy(m_real_file,doc_root);
    int len=strlen(doc_root);
    const char* p=strrchr(m_url,'/');
    if(cgi==1 && *(p+1)== '2' || *(p+1) == '3'){
        //根据标志判断是登录检测还是注册检测
        char flag=m_url[1];
        char* m_url_real=(char*)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/");
        strcat(m_url_real,m_url+2);
        strncpy(m_real_file+len,m_url_real,FILENAME_LEN-len-1);
        free(m_url_real);
        char username[100],passwd[100];
        int i=0;

        //保存用户名和密码
        //user=123&password=123
        for(i=5;m_string[i] !='&';++i){
            username[i-5]=m_string[i];
        }
        username[i-5]='\0';
        int j=0;
        for(i=i+10;m_string[i] !='\0';++i,++j){
            passwd[j]=m_string[i];
        }
        passwd[i-5]='\0';
        
        //注册检验
        if(*(p+1)=='3'){
            char *sql_insert=(char*)malloc(sizeof(char)*200);
            strcpy(sql_insert,"INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert,"'");
            strcat(sql_insert,username);
            strcat(sql_insert,"','");
            strcat(sql_insert,passwd);
            strcat(sql_insert,"')");
            //查找users中有没有该用户
            if(users.find(username) == users.end()){
                m_lock.lock();
                int res = mysql_query(mysql,sql_insert);
                //创建用户到mysql
                users.insert(pair<string,string>(username,passwd));
                m_lock.unlock();
                if(!res)                        //为什么这里不用上锁，run函数上锁了这里就不用了
                    strcpy(m_url,"/log.html");
                else
                    strcpy(m_url,"/registerError.html");
            }else{
                strcpy(m_url,"/registerError.html");
            }
            free(sql_insert);
        }
        //登录检验
        else if(*(p+1)=='2'){
            LOG_INFO("%s","debug: 登录");
            //查找是否存在该用户
            if(users.find(username) != users.end() && users[username]==passwd){
                LOG_INFO("m_url: %s","欢迎界面");
                strcpy(m_url,"/welcome.html");
            }else{
                strcpy(m_url,"/logError.html");
            }
        }
    }
    //注册界面
    if(*(p+1)=='0'){
        char* m_url_real=(char*)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/register.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        free(m_url_real);
    }
    //登陆界面
    else if(*(p+1)=='1'){
        char* m_url_real=(char*)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/log.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        free(m_url_real);
    }
    //图片界面
    else if(*(p+1)=='5'){
        char* m_url_real=(char*)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/picture.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        free(m_url_real);
    }
    //视频界面
    else if(*(p+1)=='6'){
        char* m_url_real=(char*)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/video.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        free(m_url_real);
    }
    //fans界面
    else if(*(p+1)=='7'){
        char* m_url_real=(char*)malloc(sizeof(char)*200);
        strcpy(m_url_real,"/fans.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        free(m_url_real);
    }else{
        strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);
    }
    if(stat(m_real_file,&m_file_stat)<0)   //用来获取指定路径的文件或者文件夹的信息
        return NO_RESOURCE;
    if(!(m_file_stat.st_mode & S_IROTH))       
        return FORBIDDEN_REQUEST;
    if(S_ISDIR(m_file_stat.st_mode))       //S_ISDIR为宏
        return BAD_REQUEST;
    int fd=open(m_real_file,O_RDONLY);
    //内存映射，减小对硬盘的访问，减少系统调用
    m_file_address=(char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}


//业务逻辑处理
void http_conn::process(){
    HTTP_CODE read_ret =process_read();
    LOG_INFO("debug HTTP_CODE:%d",read_ret);
    if(read_ret==NO_REQUEST){
        LOG_INFO("%s","enter");
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
        return;
    }
    bool write_ret=process_write(read_ret);
    LOG_INFO("debug: write_ret:%s","write_ret");
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);
}


//删除监听事件
void removefd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close){
    if(real_close && m_sockfd !=-1){
        LOG_INFO("close: %d\n",m_sockfd);
        removefd(m_epollfd,m_sockfd);
        m_sockfd=-1;
        m_user_count--;
    }
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once(){
    if(m_read_idx>READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read;
    //LT模式
    if(0==m_TRIGMode){
        bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx-1,0);
        LOG_INFO("debug: m_read_buf:%s",m_read_buf);
        m_read_idx+=bytes_read;
        if(bytes_read<=0){
            return false;
        }
        return true;
    }
    //ET模式，要一次性把数据读完，所以要循环读取直到EAGAIN
    if(1==m_TRIGMode){
        while (true)
        {
            bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx-1,0);
            if(bytes_read==-1){
                //没有数据可读了，EAGAIN和EWOULDBLOCK的值其实是一样的
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            if(bytes_read==0){
                return false;
            }
            m_read_idx+=bytes_read;
        }
        return true;
    }
}