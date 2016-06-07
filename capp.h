//
//  capp.h
//  cframe
//
//  Created by LiJie on 16/4/18.
//  Copyright (c) 2016 LiJie. All rights reserved.
//

#ifndef __cframe__capp__
#define __cframe__capp__

#define CAPP_VERSION "1.0.1"

#include <stdio.h>
// 同一个CFRAME中支持最多128个APP
#define MAX_APPS    128
struct capp;
struct cframe;
#define CONFIG_SUPPORT_SOCKET  1


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
    /**
     * 文件操作符
     * 如果是socket句柄，则需要在收到事件CAPP_EVT_OPEN后将socket的读写句柄赋值给该成员
     * 例如:
     * CAPP_RESULT capp_main_proc(struct capp *self, CAPP_EVT evt) {
     *      switch(evt) {
     *          case CAPP_EVT_OPEN:
     *              ....
     *              self->fds = self->socket_fds;  // 必须手动添加这一行
     *              ....
     *          break;
     *          default:
     *          break;
     *      }
     * }
     */
    int fds;
    // 文件操作模式
    CAPP_MODE mode;
    // 使能标识
    CAPP_DISABLE disabled;
    /**
     * 应用响应函数
     * 对于socket服务端来说是固定的:
     *    CAPP_RESULT capp_socket_server_main(struct capp *self, CAPP_EVT evt)
     * 对于socket客户端来说该值是通过创建socket服务端时传入的新连接处理接口
     * 对于创建的socket客户端来说，该值是注册APP时提供的回调函数
     * 对于文件来说该值是文件的回调函数
     */
    CAPP_MAIN capp_main;
    void *some_param;
#if (CONFIG_SUPPORT_SOCKET > 0)
    /**
     * 服务端的socket句柄，或者新的连接socket句柄
     * 服务端来源如下：
     * .....
     * socket_fds = socket();
     * .....
     * 客户端来源如下：
     * ....
     * server_socket_fds = socket()
     * ....
     * socket_fds = server_socket_fds.accept()
     * ....
     */
    int socket_fds;

    /**
     * 用于TCP服务端启动的新应用参数
     * 告知新的应用当前新的连接是从哪个socket连入的
     * 也可用于服务端主动退出时搜索该服务端下存在的应用。
     *
     * ....
     * socket_server_fds = socket()
     * ....
     * new_client_socket_fds = socket_server_fds.accept();
     * ....
     *
     * 在为new_client_socket_fds生成APP后将socket_server_fds传给该APP
     */
    int socket_server_fds;

    /**
     * 用于服务端的新链接APP
     * 该回调用于执行新链接的可写，可读，异常，打开，关闭等事件处理
     * 告知服务端APP当有新的连接后使用哪个接口来处理该新的连接
     */
    CAPP_MAIN new_client_proc;

    /**
     * 用于服务端的新链接APP
     * 是CFRAME框架传递给新连接APP的参数
     */
    void *new_client_param;

    void *socket_param[4];
#endif
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
void cframe_unregiste_capp(struct cframe *root, struct capp *app);

// 将框架中的文件扫描一遍
int cframe_run(struct cframe *root, int ms);

#if (CONFIG_SUPPORT_SOCKET > 0)
// 创建一个基于TCP的服务器
struct capp *capp_start_socket_server(struct cframe *root, CAPP_MAIN new_client_proc, void *server_param, void *client_param, int port, int maxclients);
// 销毁TCP服务器
void capp_destroy_socket_server(struct cframe *root, struct capp *);
// 创建一个基于TCP的客户端
struct capp *capp_start_socket_client(struct cframe *root, CAPP_MAIN session_proc, void *client_param, const char *host, int port);
// 销毁TCP客户端
void capp_destroy_socket_client(struct cframe *root, struct capp *);
#endif

#endif /* defined(__cframe__capp__) */
