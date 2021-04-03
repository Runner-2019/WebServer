#include "lst_timer.h"
#include "../http/http_conn.h"

// 初始化
sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

//链表被销毁时，删除其中所有的定时器
sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

//将目标定时器timer添加到链表中
void sort_timer_lst::add_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    if (!head)
    {
        head = tail = timer;
        return;
    }
    //如果目标定时器的超时时间小于当前链表中的所有定时器的超时时间，
    //则把该定时器插入链表头部，作为新的链表节点
    //否则就用重载函数add_timer(util_timer* timer,util_timer* lst_head)
    //把它插入链表中的合适位置，以保证链表的升序特性
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

//  当某个定时任务发生变化时，调整对应的定时器在链表中的位置
// 这个函数只考虑被调整的定时器的超时时间延长的情况
// 即该定时器需要往链表的尾部移动
void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    util_timer *tmp = timer->next;
    /*如果被调整的定时器处在链表尾部，或者该定时器的超时值仍然小于下一个定时器的超时值，则不需要调整*/
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }
    // 如果目标定时器是链表的头节点，则将该定时器从链表中取出并重新插入链表
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    //如果目标定时器不是链表的头节点，则将该定时器从链表中取出，然后插入
    //其原来所在位置之后的部分链表中
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

//将目标定时器timer从链表中删除
void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    //链表中只有一个定时器，即目标定时器
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    //链表中至少有两个定时器，且目标定时器是链表的头节点
    //则将链表的头节点重置为原头节点的下一个节点
    //然后删除目标定时器
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    //链表中至少有两个定时器，且目标定时器是链表的尾节点
    //则将链表的头节点重置为原头节点的前一个节点
    //然后删除目标定时器
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    //如果目标定时器位于链表的中间，则把它前后的定时器串联起来，然后删除目标定时器
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

//SIGALRM 信号每次触发，都在其信号处理函数，统一事件源就是主函数中执行一次tick函数
//以处理链表上到期的任务
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    //当前时间
    time_t cur = time(NULL);
    util_timer *tmp = head;
    //定时器核心逻辑
    //从头到尾依次处理每个定时器节点，直到遇到第一个尚未到期的定时器
    while (tmp)
    {
        //当前时间小于到期时间，未超时
        //注意时间是绝对时间
        if (cur < tmp->expire)
        {
            break;
        }
        //调用回调，可以向客户发送FIN之类的断联信息
        tmp->cb_func(tmp->user_data);
        //执行完回调后，删除，重置头节点
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}
//一个重载的辅助函数
//将timer添加到lst_head之后的部分链表中
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    //超时时间最大，放到链表尾部
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    //定时处理任务，实际上是调用trick函数
    m_timer_lst.tick();
    //一次alarm调用只会触发一次SIGALRM信号，所以重新定时
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
