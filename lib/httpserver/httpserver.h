
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"


#include "pico/cyw43_arch.h"
#include "pico/cyw43_driver.h"

#include "lwip/init.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/sockets.h"

#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"


#include "task.h"

#define HTTPSERVER_MAX_HTTP_LINE_LENGTH 100
#define HTTPSERVER_MAX_TARGET_LENGTH 50
#define HTTPSERVER_MAX_CONTENT_LENGTH 1024


typedef struct _http_request_t
{
    unsigned int content_length;
    char content[HTTPSERVER_MAX_CONTENT_LENGTH];
    int incoming_sock;
    unsigned int target_size;
    char taget[HTTPSERVER_MAX_TARGET_LENGTH];
} http_request_t;

typedef void (* CallbackFunction_t)( http_request_t * req );

typedef struct _http_server_t
{
    CallbackFunction_t callback;
    int server_sock;
    TaskHandle_t server_task;
} http_server_t;



void http_init(http_server_t * server, CallbackFunction_t callback,unsigned short port);