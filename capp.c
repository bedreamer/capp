//
//  capp.c
//  cframe
//
//  Created by 李杰 on 16/4/18.
//  Copyright (c) 2016年 李杰. All rights reserved.
//
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "capp.h"

// 初始化文件APP框架
struct cframe* create_cframe()
{
    struct cframe* root = (struct cframe*)malloc(sizeof(struct cframe));
    memset(root, 0, sizeof(struct cframe));
    return root;
}

// 终止一个文件APP框架
int destroy_cframe(struct cframe* root)
{
    return 0;
}

// 创建一个基于文件的APP对象
struct capp *cframe_registe_capp(struct cframe* root, CAPP_MAIN capp_main, void *some_param)
{
    struct capp *app = NULL;
    struct capp **thiz = &root->new_childs;

    if ( ! root ) return NULL;
    app = (struct capp*)malloc(sizeof(struct capp));
    if ( ! app ) return NULL;

    memset(app, 0, sizeof(struct capp));
    app->capp_main = capp_main;
    app->disabled = CAPP_DISABLED;
    app->some_param = some_param;
    app->fds = -1;
    app->mode = CAPP_MODE_INVALID;

    // 将应用对象添加到列表中
    while ( *thiz ) thiz = &(*thiz)->next;
    *thiz = app;

    return app;
}

// 创建一个基于SOCKET的APP对象
int new_socket_capp()
{
    return 0;
}

// 处理可读文件
void cframe_process_files(struct capp **apps, struct fd_set *rd_fds, struct fd_set *wr_fds, struct fd_set *exp_fds)
{
    struct capp *thiz = NULL;
    CAPP_RESULT capp_result = CAPP_RESULT_OK;

    while ( *apps ) {
        thiz = *apps;
        if ( FD_ISSET(thiz->fds, rd_fds) ) {
            capp_result = thiz->capp_main(thiz, CAPP_EVT_READABLE);
            if ( capp_result == CAPP_RESULT_EXIT ) {
                FD_CLR(thiz->fds, wr_fds);
                FD_CLR(thiz->fds, exp_fds);
                capp_result = thiz->capp_main(thiz, CAPP_EVT_CLOSE);
                // delete
                *apps = thiz->next;
                free(thiz);
                continue;
            }
        }
        if ( FD_ISSET(thiz->fds, wr_fds) ) {
            capp_result = thiz->capp_main(thiz, CAPP_EVT_WRITABLE);
            if ( capp_result == CAPP_RESULT_EXIT ) {
                FD_CLR(thiz->fds, exp_fds);
                capp_result = thiz->capp_main(thiz, CAPP_EVT_CLOSE);
                // delete
                *apps = thiz->next;
                free(thiz);
                continue;
            }
        }
        if ( FD_ISSET(thiz->fds, exp_fds) ) {
            capp_result = thiz->capp_main(thiz, CAPP_EVT_EXCEPT);
            if ( capp_result == CAPP_RESULT_EXIT ) {
                FD_CLR(thiz->fds, exp_fds);
                capp_result = thiz->capp_main(thiz, CAPP_EVT_CLOSE);
                // delete
                *apps = thiz->next;
                free(thiz);
                continue;
            }
        }
        apps = &((*apps)->next);
    }
}

// 添加新的文件到普通文件列表中
int cframe_process_new_capp(struct cframe *root, struct capp **normal_apps, struct capp **new_apps)
{
    int new_apps_nr = 0;
    struct capp *thiz_app = NULL;
    CAPP_RESULT capp_result = CAPP_RESULT_OK;
    
    if ( ! normal_apps ) return 0;
    if ( ! new_apps ) return 0;
    if ( ! *new_apps ) return 0;

    while ( *normal_apps ) normal_apps = &(*normal_apps)->next;
    while ( *new_apps ) {
        // 1. 先从新文件列表中摘除
        thiz_app = *new_apps;
        *new_apps = thiz_app->next;
        thiz_app->next = NULL;
        
        // 2. 执行初始化和打开文件操作
        // 2.1 没有有效的事件处理函数接口
        if ( ! thiz_app->capp_main ) continue;
        
        capp_result = thiz_app->capp_main(thiz_app, CAPP_EVT_INIT);
        // 2.2 初始化失败
        if ( capp_result != CAPP_RESULT_OK ) continue;

        capp_result = thiz_app->capp_main(thiz_app, CAPP_EVT_OPEN);
        // 2.3 打开文件失败
        if ( capp_result != CAPP_RESULT_OK ) continue;
        
        // 2.4 检测文件打开模式
        if ( thiz_app->mode == CAPP_MODE_INVALID ) continue;
        
        // 2.5 检测文件描述符有效性
        if ( thiz_app->fds == -1 ) continue;

        thiz_app->disabled = CAPP_ENABLED;
        thiz_app->root = root;

        // 添加至普通链表中
        new_apps_nr ++;
        *normal_apps = thiz_app;
        normal_apps = &(*normal_apps)->next;
    }

    return new_apps_nr;
}

// 设置指定模式的文件描述符
int cframe_set_fds(struct capp *apps, CAPP_MODE mode, struct fd_set *fds_set)
{
    int max_fds = -1;
    struct capp *thiz = NULL;

    while ( apps && apps->fds ) {
        thiz = apps;
        apps = apps->next;

        if ( ! (thiz->mode & mode) ) continue;
        if ( thiz->disabled == CAPP_DISABLED ) continue;
        if ( thiz->fds > max_fds ) max_fds = thiz->fds;

        FD_SET(thiz->fds, fds_set);
    }
    
    return max_fds;
}

// 将框架中的文件扫描一遍
int cframe_run(struct cframe *root, int ms)
{
    struct fd_set readable_set, writable_set, exp_set;
    int select_return = 0;
    int select_timeout_s = 0, select_timeout_ms = 50;
    struct timeval tv;
    int max_fds = -1, select_max_fds = -1;

    if ( ms >= 0 ) {
        select_timeout_ms = ms % 1000; // 50ms is the default TTL
        select_timeout_s = ms / 1000;
    }

    FD_ZERO(&readable_set);
    FD_ZERO(&writable_set);
    FD_ZERO(&exp_set);
    tv.tv_sec = select_timeout_s;
    tv.tv_usec = select_timeout_ms * 1000;

    // 处理所有新添加的文件
    cframe_process_new_capp(root, &root->normal_childs, &root->new_childs);

    max_fds = cframe_set_fds(root->normal_childs, CAPP_MODE_RO, &readable_set);
    select_max_fds = max_fds > select_max_fds ? max_fds : select_max_fds;

    max_fds = cframe_set_fds(root->normal_childs, CAPP_MODE_WO, &writable_set);
    select_max_fds = max_fds > select_max_fds ? max_fds : select_max_fds;

    max_fds = cframe_set_fds(root->normal_childs, CAPP_MODE_RW, &exp_set);
    select_max_fds = max_fds > select_max_fds ? max_fds : select_max_fds;
    
    // 没有需要筛选的文件描述符
    if ( select_max_fds <= -1 ) return 0;

    select_return = select(select_max_fds+1, &readable_set, &writable_set, &exp_set, &tv);

    // 没有准备完成读或写的文件， TIMEOUT
    if ( select_return == 0 ) {
        return 0;
    }
    // 出错, 被信号打断了, 可以继续执行
    if ( select_return == -1 && errno == EINTR ) {
        printf("%ld %ul", tv.tv_sec, tv.tv_usec);
        return 0;
    }
    // 出错，有无效的文件描述符，设法恢复现场
    if ( select_return == -1 && errno == EBADF ) {
        return EBADF;
    }
    // 出错，内核无法分配文件描述
    if ( select_return == -1 && errno == EAGAIN ) {
        return EAGAIN;
    }
    // 出错，无效的参数，设法恢复现场
    if ( select_return == -1 && errno == EINVAL ) {
        return EINVAL;
    }
    // 有其他未知错误
    if ( select_return == -1 ) {
        return -1;
    }
    // 处理就绪文件
    cframe_process_files(&root->normal_childs, &readable_set, &writable_set, &exp_set);

    return 0;
}

CAPP_RESULT capp_socket_server(struct capp *self, CAPP_EVT evt)
{
    int ret  = 0;
    struct socket_server *server = (struct socket_server *)self->some_param;

    switch(evt)
    {
            // capp initlize event
        case CAPP_EVT_INIT:
            return CAPP_RESULT_OK;
            // capp open file event
        case CAPP_EVT_OPEN:
            // 金蝉脱壳
            self->fds = server->server_fds;
            self->some_param = server->new_client_proc;
            self->mode = CAPP_MODE_RO;
            return CAPP_RESULT_OK;
            // capp file is writable event
        case CAPP_EVT_READABLE:{
            printf("new client");
            struct sockaddr_in client_addr;
            socklen_t length = sizeof(client_addr);
            
            ///成功返回非负描述字，出错返回-1
            int conn = accept(self->fds, (struct sockaddr*)&client_addr, &length);
            if(conn<0) {
                perror("accept connect failed");
                exit(1);
            }
            printf("start accept data from Client...\r\n");
            cframe_registe_capp(self->root, (CAPP_MAIN)self->some_param, (void*)(int *)conn);
            return CAPP_RESULT_OK;
        }
            // capp file is exception event
        case CAPP_EVT_EXCEPT:
            return CAPP_RESULT_EXIT;
            // capp close file event
        case CAPP_EVT_CLOSE:
            close(self->fds);
            printf("{close}\n");
            return CAPP_RESULT_EXIT;
        case CAPP_EVT_WRITABLE:
        default:
            return CAPP_RESULT_EXIT;
    }
    return CAPP_RESULT_EXIT;
}

// 创建一个基于TCP的服务器
struct capp *capp_start_socket_server(struct cframe* root, CAPP_MAIN capp_main, void *server_param, int port, int maxclients)
{
    struct sockaddr_in server_sockaddr;
    int ret;
    int server_sockfd;
    struct capp *self;
    
    if (root == NULL ) return NULL;
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( server_sockfd == -1 ) return NULL;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons (port);
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(server_sockfd, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr));
    if ( ret != 0 ) {
        close(server_sockfd);
        return NULL;
    }
    if ( maxclients <= 0 ) maxclients = 1;
    if ( 0 == listen(server_sockfd, maxclients) ) {
        struct socket_server *server = (struct socket_server *)malloc(sizeof(struct socket_server));
        server->new_client_proc = capp_main;
        server->server_fds = server_sockfd;
        server->server_param = server_param;
        self = cframe_registe_capp(root, capp_socket_server, server);
        if ( !self ) goto die;
        self->root = root;
        return self;
    }
die:
    close(server_sockfd);
    return NULL;
}

// 创建一个基于TCP的客户端
struct capp *capp_start_socket_client(struct cframe* root, CAPP_MAIN capp_main, void *client_param, const char *host, int port)
{
    struct sockaddr_in client_sockaddr;
    int ret;
    int client_sockfd;
    struct capp *self;
    
    if (root == NULL ) return NULL;
    client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( client_sockfd == -1 ) return NULL;
    client_sockaddr.sin_family = AF_INET;
    client_sockaddr.sin_port = htons (port);
    client_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    ret = bind(client_sockfd, (struct sockaddr *)&client_sockaddr, sizeof(client_sockaddr));
    if ( ret != 0 ) {
        close(client_sockfd);
        return NULL;
    }
    self = cframe_registe_capp(root, capp_main, (void*)client_sockfd);
    if ( !self ) goto die;
    self->root = root;
    return self;
die:
    close(client_sockfd);
    return NULL;
}

