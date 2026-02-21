#include "httpserver.h"

/*
void send_file(http_request_t *http_request, char *filename, char *mime)
{
    char sendBuff[HTTPSERVER_MAX_HTTP_LINE_LENGTH];
    int length = snprintf(NULL, 0, "HTTP/1.0 200 OK\r\nContent-type: %s\r\n\r\n", mime);
    snprintf(sendBuff, length + 1, "HTTP/1.0 200 OK\r\nContent-type: %s\r\n\r\n", mime);
    send(http_request->incoming_sock, sendBuff, length, 0);
}
*/
void http_close_connection(struct tcp_pcb *tpcb, http_transaction_t *trans)
{
     printf("closing connection..\n");
    tcp_arg(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_recv(tpcb, NULL);
    tcp_err(tpcb, NULL);

    if (trans != NULL)
    {
        mem_free(trans);
    }

    tcp_close(tpcb);
}
err_t http_serve(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    printf("processing recieve request..\n");
    http_transaction_t *trans = (http_transaction_t *)arg;

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    if (p == NULL || trans == NULL)
    {
        printf("ERROR \n");
        if (p != NULL)
            tcp_recved(tpcb, p->tot_len);

        http_close_connection(tpcb, trans);
        return ERR_OK;
    }

    for (struct pbuf *q = p; q != NULL; q = q->next)
    {
        if (trans->state == HTTP_REQ_STATE_DONE)
            break;
        for (int i = 0; i < q->len; i++)
        {
            if (trans->state == HTTP_REQ_STATE_DONE)
                break;

            trans->request_data[trans->request_length] = *(uint8_t *)(p->payload + i);
            //printf("yehii\n");
            //trans->server->callback(NULL);
            // putchar(trans->request_data[trans->request_length]);
            trans->request_length++;

            if (HTTPSERVER_MAX_REQUEST_LENGTH < trans->request_length)
            {
                printf("BIG REQUEST!!\n");
                pbuf_free(p);
                // http_close_connection(tpcb,trans);
                tcp_abort(tpcb);
                return ERR_OK;
            }

            switch (trans->state)
            {
            case HTTP_REQ_STATE_HEADER:

                // Check for line end
                
                if (trans->request_length >= 2 && trans->request_data[trans->request_length - 1] == '\n' && trans->request_data[trans->request_length - 2] == '\r')
                {
                    //for (uint16_t j = trans->last_line; j < trans->request_length; j++)
                    //    putchar(trans->request_data[j]);
                    char tmp[HTTPSERVER_MAX_HTTP_LINE_LENGTH];
                    uint8_t line_length = trans->request_length - trans->last_line;
                    memcpy(tmp, trans->request_data + trans->last_line, line_length);
                    tmp[line_length] = '\0';
                    /*if (strnstr(tmp, "Content-Length", line_length) != NULL)
                    {
                        sscanf(tmp, "Content-Length: %d\r\n", &trans->request.content_length);
                        printf("wow %s\n", tmp);
                        printf("wew %d\n", trans->request.content_length);
                    }*/


                    if (strncmp("GET", tmp, 3) == 0)
                    {
                       sscanf(tmp, "GET %s ", trans->request.target);
                       printf("wow %s\n", trans->request.target);
                    }

                    trans->last_line = trans->request_length;
                }
                
                // Check for header end
                if (trans->request_length >= 4 && trans->request_data[trans->request_length - 1] == '\n' && trans->request_data[trans->request_length - 2] == '\r' && trans->request_data[trans->request_length - 3] == '\n' && trans->request_data[trans->request_length - 4] == '\r')
                {

                    if (trans->request.content_length == 0)
                        trans->state = HTTP_REQ_STATE_DONE;
                    else
                        trans->state = HTTP_REQ_STATE_BODY;
                    trans->request.header_length = trans->request_length;
                    trans->request.content = trans->request_data + trans->request_length;
                }
                break;
            case HTTP_REQ_STATE_BODY:
                printf("filling content \n");
                if (trans->request_length - trans->request.header_length == trans->request.content_length)
                {
                    trans->state = HTTP_REQ_STATE_DONE;
                }
                break;
            default:
                printf("shouldnt be here \n");
                break;
            }
        }
    }
    
    printf("Recieved so far for %d : %d \n", trans->id, trans->request_length);
    printf("State so far of %d : %d \n", trans->id, trans->state);

    if (trans->state == HTTP_REQ_STATE_DONE)
    {
        printf("transaction recieve Done %d\n",trans->server);
       
        http_response_t resp = (*trans->server->callback)(&trans->request);
        
        switch (resp.type)
        {
        case HTTP_RESPONSE_TYPE_BLOB:
            const unsigned int chunkSize = 6400;
            unsigned int total = sizeof(resp.data_ptr);
            unsigned int sent = 0;
            unsigned int toSend = 0;
            while (sent < total)
            {
                toSend = total - sent; // min(total-sent,chunkSize);
                tcp_write(tpcb, resp.data_ptr + sent, toSend, TCP_WRITE_FLAG_COPY);
                tcp_output(tpcb);
                sent += toSend;
                printf("sending shitt\n");
            }
            break;
        case HTTP_RESPONSE_TYPE_STRING:
            tcp_write(tpcb, resp.data_ptr, strlen(resp.data_ptr), TCP_WRITE_FLAG_COPY);
            tcp_output(tpcb);
            break;
        case HTTP_RESPONSE_TYPE_NONE:
            const char* ok_buddy = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!";
            err_t err  = tcp_write(tpcb, ok_buddy, strlen(ok_buddy), TCP_WRITE_FLAG_COPY);
            if(err != ERR_OK)
            {
                printf("Smth fucked up\n");
            }
            err = tcp_output(tpcb);
            if(err != ERR_OK)
            {
                printf("Smth fucked up\n");
            }
            printf("Probably not found\n");
            break;
        default:
            break;
        }

    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    tcp_recved(tpcb, p->tot_len);

    pbuf_free(p);

    return ERR_OK;
}
static int reqID = 0;

err_t http_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    http_transaction_t *trans = (http_transaction_t *)arg;
    //tcp_close(tpcb);//THIS HAS TO BE HERE
    printf("Smth fucked up\n");
    http_close_connection(tpcb,trans);
}

err_t http_err_cb(void *arg, err_t err)
{
    printf("error in transaction\n");
    http_transaction_t *trans = (http_transaction_t *)arg;
    if (trans != NULL)
        printf("free 2\n");
    mem_free(trans);
    trans = NULL;
}
err_t http_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    printf(" accepting client \n");
    http_transaction_t *trans = mem_malloc(sizeof(http_transaction_t));
    trans->state = HTTP_REQ_STATE_HEADER;
    trans->last_line = 0;
    trans->request_length = 0;
    trans->id = reqID++;
    trans->server = (http_server_t *)arg;
    
    trans->request.header = trans->request_data;
    trans->request.header_length = 0;
    trans->request.content_length = 0;

    tcp_arg(newpcb, trans);
    tcp_recv(newpcb, http_serve);
    tcp_err(newpcb, http_err_cb);
    tcp_sent(newpcb, http_sent_cb);

    printf("client accepted \n");

    return ERR_OK;
}
void http_init(http_server_t *server, HTTP_callback_function_t callback, unsigned short port)
{

    server->callback = callback;
    

    struct tcp_pcb *pcb = tcp_new();

    tcp_bind(pcb, IP_ADDR_ANY, port);

    pcb = tcp_listen(pcb);

    tcp_arg(pcb, server);
    tcp_accept(pcb, http_accept);

    return;
}