#include "httpserver.h"


unsigned int recv_line(int incoming_sock, char* buffer)
{
    unsigned int read = 0;
    while(read < HTTPSERVER_MAX_HTTP_LINE_LENGTH)
    {
        unsigned int  done = recv(incoming_sock, buffer+read,1, 0);

        if(done<=0)
            return -1;

        read++;
        if(read >= 2 && buffer[read-1] == '\n' && buffer[read-2] == '\r')
            return read;   
    }
    return -1;
}



void send_file(http_request_t* http_request,char* filename,char * mime)
{
    char sendBuff[HTTPSERVER_MAX_HTTP_LINE_LENGTH];
    int length = snprintf( NULL, 0, "HTTP/1.0 200 OK\r\nContent-type: %s\r\n\r\n", mime );
    snprintf( sendBuff, length + 1, "HTTP/1.0 200 OK\r\nContent-type: %s\r\n\r\n", mime);
    send(http_request->incoming_sock,sendBuff, length, 0);

}

static void http_serve(void *params)
{
    http_server_t * server = (http_server_t *) params;
    while (true)
    {
        struct sockaddr_storage remote_addr;
        http_request_t http_request;
        socklen_t len = sizeof(remote_addr);
        http_request.incoming_sock = accept(server->server_sock, (struct sockaddr *)&remote_addr, &len);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        if (http_request.incoming_sock < 0)
        {
            printf("Unable to accept incoming connection: error %d\n", errno);
            continue;
        }

        char buffer[HTTPSERVER_MAX_HTTP_LINE_LENGTH];
        unsigned int line_length = recv_line(http_request.incoming_sock,buffer);

        bool targ = false;
        http_request.target_size = 0;
        for(int i = 0;i<HTTPSERVER_MAX_HTTP_LINE_LENGTH;i++)
        {
            if(targ && buffer[i]==' ')
            {
                break;
            }
            if(targ)
            {
                http_request.taget[http_request.target_size] = buffer[i];
                http_request.target_size++;
            }
            if(!targ && buffer[i]==' ')
            {
                targ = true;
            }
        }
        http_request.taget[http_request.target_size]  = '\0';
        http_request.content_length = 0;
        while(!(buffer[0] == '\r' && buffer[1] == '\n'))
        {
             
            line_length = recv_line(http_request.incoming_sock,buffer);
            buffer[line_length] = '\0';
            if(strnstr(buffer, "Content-Length", line_length) != NULL){
                sscanf(buffer,"Content-Length: %d\r\n",&http_request.content_length);
                printf("wow %s\n", buffer);
                printf("wew %d\n", http_request.content_length);
            }
        }
       
        if(HTTPSERVER_MAX_CONTENT_LENGTH < http_request.content_length)
        {
            printf("wow %s\n", buffer);
            closesocket(http_request.incoming_sock);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            continue;
        }

        if(http_request.content_length > 0)
            recv(http_request.incoming_sock,http_request.content,http_request.content_length,0);
        http_request.content[http_request.content_length] = '\0';
        server->callback(&http_request);
        closesocket(http_request.incoming_sock);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    }
}

void http_init(http_server_t * server, CallbackFunction_t callback,unsigned short port)
{
    server->callback = callback;

    server->server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    struct sockaddr_in listen_addr =
        {
            .sin_len = sizeof(struct sockaddr_in),
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = 0,
        };
 
    if ( server->server_sock < 0)
    {
        printf("Unable to create socket: error");
        return;
    }
 
    if (bind( server->server_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
    {
        closesocket( server->server_sock);
        printf("Unable to bind socket: error");
        return;
    }
 
    if (listen( server->server_sock, 1) < 0)
    {
        closesocket( server->server_sock);
        printf("Unable to listen on socket: error\n");
        return;
    }
    
   
    xTaskCreate(http_serve,"httpServer",2048,server,tskIDLE_PRIORITY+5, &(server->server_task));
    return;
}