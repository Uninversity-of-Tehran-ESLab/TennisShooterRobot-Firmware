
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
#define HTTPSERVER_MAX_REQUEST_LENGTH 10240


enum transaction_state
{
    HTTP_REQ_STATE_HEADER = 0,
    HTTP_REQ_STATE_BODY = 1,
    HTTP_REQ_STATE_DONE = 2
};

typedef struct _http_request_t
{

    size_t header_length;
    uint8_t* header;

    unsigned int content_length;
    uint8_t* content;

    unsigned int target_size;
    uint8_t* target;
} http_request_t;



typedef void (*HTTP_callback_function_t)(http_request_t *req);

typedef struct _http_server_t
{
    HTTP_callback_function_t callback;

} http_server_t;

void http_init(http_server_t *server, HTTP_callback_function_t callback, unsigned short port);

typedef struct 
{
    uint8_t request_data[HTTPSERVER_MAX_REQUEST_LENGTH];
    uint16_t request_length;
    int id;
    enum transaction_state state;
    size_t last_line;

    http_request_t request;

    http_server_t* server;
} http_transaction_t;
