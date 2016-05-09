//
//  capp.h
//  cframe
//
//  Created by LiJie on 16/4/18.
//  Copyright (c) 2016 LiJie. All rights reserved.
//

#ifndef __cframe__capp__
#define __cframe__capp__

#include <stdio.h>
// 同一个CFRAME中支持最多128个APP
#define MAX_APPS    128
struct capp;
struct cframe;
typedef unsigned int id_t;
#define ID_INVALID  0

// CAPP 框架事件
typedef enum {
    CAPP_EVT_INVALID    = 0x00,
    // 初始化
    CAPP_EVT_INIT       = 0x01,
    // 打开文件事件
    CAPP_EVT_OPEN       = 0x02,
    // 文件关闭事件
    CAPP_EVT_CLOSE      = 0x03,
    // 文件可读事件
    CAPP_EVT_READABLE   = 0x80,
    // 文件可写事件
    CAPP_EVT_WRITABLE   = 0x40,
    // 文件异常事件
    CAPP_EVT_EXCEPT     = 0x20
}CAPP_EVT;

// CAPP 框架返回结果
typedef enum {
    CAPP_RESULT_OK = 0,
    // APP 要求关闭文件
    CAPP_RESULT_CLOSE = 1,
    // APP 要求退出
    CAPP_RESULT_EXIT = 2
}CAPP_RESULT;

// CAPP 框架APP文件读写方式
typedef enum {
    // 无效模式
    CAPP_MODE_INVALID = 0x00,
    // 只读
    CAPP_MODE_RO = 0x01,
    // 只写
    CAPP_MODE_WO = 0x02,
    // 读写
    CAPP_MODE_RW = 0x01|0x02
}CAPP_MODE;

// CAPP 禁用模式
typedef enum{
    // 已禁用
    CAPP_DISABLED,
    // 已使能
    CAPP_ENABLED
}CAPP_DISABLE;

// CAPP 框架响应接口函数
typedef CAPP_RESULT (*CAPP_MAIN)(struct capp *self, CAPP_EVT evt);

// 框架内的应用程序描述
struct capp {
    // 文件操作符
    int fds;
    // 文件操作模式
    CAPP_MODE mode;
    // 使能标识
    CAPP_DISABLE disabled;
    // 应用响应函数
    CAPP_MAIN capp_main;
    void *some_param;
    // 框架根
    struct cframe *root;

    struct capp *next;
};

// 框架结构
struct cframe {
    // 框架下正常使用的应用程序
    struct capp *normal_childs;
    // 框架下禁用的应用程序
    struct capp *disabled_childs;
    // 框架下未注册的应用程序
    struct capp *unregisted_childs;
    // 框架下新注册的应用
    struct capp *new_childs;
};

// 初始化文件APP框架
struct cframe* create_cframe();
// 终止一个文件APP框架
int destroy_cframe(struct cframe* root);

// 使能capp
int cframe_enable_capp(struct cframe *root, struct capp *app);
// 禁用capp
int cframe_disable_capp(struct cframe *root, struct capp *app);
// 注册capp
struct capp *cframe_registe_capp(struct cframe* root, CAPP_MAIN capp_main, void *some_param);
// 注销capp
int cframe_unregiste_capp(struct cframe *root, struct capp *app);

// 将框架中的文件扫描一遍
int cframe_run(struct cframe *root, int ms);

// socket 服务器参数
struct socket_server {
    CAPP_MAIN  new_client_proc;
    void *server_param;
    int server_fds;
};
// 创建一个基于TCP的服务器
struct capp *capp_start_socket_server(struct cframe* root, CAPP_MAIN capp_main, void *server_param, int port, int maxclients);
// 创建一个基于TCP的客户端
struct capp *capp_start_socket_client(struct cframe* root, CAPP_MAIN capp_main, void *client_param, const char *host, int port);

#endif /* defined(__cframe__capp__) */
