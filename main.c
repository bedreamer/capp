//
//  main.c
//  cframe
//
//  Created by 李杰 on 16/4/18.
//  Copyright (c) 2016年 李杰. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "capp.h"

struct capp_ca {
    double last_tsp;
    int ttl;
    int count;
    double tsp_diff_avg;
};

CAPP_RESULT http_svr_main(struct capp *self, CAPP_EVT evt)
{
    char buff[128] = {0};
    int len  = 0;
    struct timeval tv;
    struct capp_ca *ca;
    double this_tsp;
    
    switch(evt)
    {
            // capp initlize event
        case CAPP_EVT_INIT:
            return CAPP_RESULT_OK;
            // capp open file event
        case CAPP_EVT_OPEN:
            self->fds = (int)self->some_param;
            self->some_param = malloc(sizeof(struct capp_ca));
            memset(self->some_param, 0, sizeof(struct capp_ca));
            ca = (struct capp_ca*)self->some_param;
            ca->ttl = 10000;
            self->mode = CAPP_MODE_RW;
            return CAPP_RESULT_OK;
            // capp file is writable event
        case CAPP_EVT_WRITABLE:
            gettimeofday(&tv, NULL);
            ca = self->some_param;
            this_tsp = (tv.tv_sec-1462464803) + tv.tv_usec/1000000.0f;
            if ( ca->count == 0 ) {
            } else if ( ca->count == 1 ) {
                ca->tsp_diff_avg = this_tsp - ca->last_tsp;
            } else {
                ca->tsp_diff_avg = (ca->tsp_diff_avg * ca->count + (this_tsp - ca->last_tsp) ) / (ca->count+1);
            }

            len = sprintf(buff, "[%.6f] %d: %.6f,%.6f\n", this_tsp, self->fds, ca->tsp_diff_avg, this_tsp - ca->last_tsp);
            write(self->fds, buff, len);

            ca->last_tsp = this_tsp;
            ca->count ++;

            if ( -- ca->ttl != 0 )
                return CAPP_RESULT_OK;
            else return CAPP_RESULT_EXIT;
        case CAPP_EVT_READABLE:
            gettimeofday(&tv, NULL);
            printf("{read} %lu.%d \n", tv.tv_sec, tv.tv_usec);
            return CAPP_RESULT_OK;
            // capp file is exception event
        case CAPP_EVT_EXCEPT:
            return CAPP_RESULT_EXIT;
            // capp close file event
        case CAPP_EVT_CLOSE:
            close(self->fds);
            printf("{close}\n");
            return CAPP_RESULT_EXIT;
        default:
            return CAPP_RESULT_EXIT;
    }
    return CAPP_RESULT_EXIT;
}

int main()
{
    struct cframe *root = NULL;
    struct capp *capp = NULL;
    
    root = create_cframe();
    if ( ! root ) {
        return 1;
    }
    capp =  capp_start_socket_server(root, http_svr_main, NULL, 9999, 10);

    if ( !capp ) printf("capp failed!\n");
    while (1) {
        cframe_run(root, 50);
    }
    
    return 0;
}

