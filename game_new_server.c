#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <linux/unistd.h>


/* Размер поля по горизонтали */
#define MAX_SIZE_X 500

/* Размер поля по вертикали */
#define MAX_SIZE_Y 500

/* максимальное кол-во пикселей еды на поле */
#define MAX_PIXELS 100

/* порт сервера */
#define PORT     8080

/* максимальное кол-во подключенных клиентов */
#define MAX_CLIENTS 2

/* максимальный размер датаграммы */
#define MAXLINE  1500

/* sockaddr_in сервера */
struct sockaddr_in servaddr;

/* sockaddr_in клиента(ов) */
struct sockaddr_in cliaddr;

int sockfd;

/* объявление структуры и массива клиентов */
struct connection {
    int thread;
    int ident;
    int data_n;
    char *buf;
    struct sockaddr_in *sockaddr_p;

} clients[MAX_CLIENTS];


void * serialization(char * input, int* x, int* y, int* x_side,
                     int* y_side);
void deserialization (void * input, int* x, int* y, int* x_side,
                      int* y_side);


void deserialization(void * input, int* x, int* y, int* x_side,
                     int* y_side) {

    void * buffer = input;
    /*пропускаем идентификатор и номер датаграммы*/
    buffer += sizeof(int);
    buffer += sizeof(int);

    /*десериаизуем координаты*/
    *x = *(int *)buffer;
    buffer += sizeof(int);
    *y =  *(int *)buffer;
    buffer += sizeof(int);

    /*десериализуем размер сторон*/
    *x_side = *(int *)buffer;
    buffer += sizeof(int);
    *y_side = *(int *)buffer;
    buffer += sizeof(int);
}

void * serialization(char * input, int* x, int* y, int* x_side, int* y_side) {

    /* сохраняем неизмененный указатель на буфер */
    char *pointer = input;
    /*пропускаем идентификатор и номер датаграммы*/
    void *pnt;

    pnt = (void*)input + sizeof(int);
    pnt = pnt + sizeof(int);

    /*перезаписываем данные координат и сторон */
    memcpy(pnt, x, sizeof(int));
    pnt += sizeof(int);
    memcpy(pnt, y, sizeof(int));
    pnt += sizeof(int);

    memcpy(pnt, x_side, sizeof(int));
    pnt += sizeof(int);
    memcpy(pnt, y_side, sizeof(int));
    pnt += sizeof(int);

    return pointer;
}

void* send_data(void* buf_pointer) {
    printf("Thread is going\n");
    while(1) {

        /* получаем идентификатор клиента */
        void *ident_pnt = buf_pointer;
        int ident = *(int *)ident_pnt;

        /*заводим структуру, чтоб позже скопировать в нее данные соединения клиента*/
        struct connection client;

        for (int i = 0; i < MAX_CLIENTS; i++) {

            /* если идентификатор из буфера совпадает
               с идентификатором  клиента */
            if ( ident == clients[i].ident ) {
                /* то получаем указатель на его буфер */
                char *cur_client_buf = clients[i].buf;

                /* и ищем не совпадающий идентификатор */
                for (int i = 0; i < MAX_CLIENTS; i++) {

                    /* если идентификаторы разные - мы нашли, кому отправить данные*/
                    if (ident != clients[i].ident &&
                        clients[i].ident != 0 ) {

                        int n =  sendto(sockfd, cur_client_buf, MAXLINE,
                                        MSG_CONFIRM,
                                        (struct sockaddr *)clients[i].sockaddr_p,
                                        sizeof(cliaddr));
                    }
                }
            }
        }
    }
}

int init_socket() {

    /* Создаем сокет. Должны в случае успеха получить его дескриптор */

    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* заполняем данные о сервере */
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    /* привязываем сокет к адресу */
    if ( bind( sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr) ) < 0 ) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

}

void register_new_client(char* input, int data_n, int ident,
                         int client_array_cnt) {

    struct connection new_client_connect = clients[client_array_cnt];
    /* выделяем память под буфер и перезаписываем туда данные */
    char *new_client_buf = malloc(MAXLINE);
    memcpy(new_client_buf, input, MAXLINE);

    /* записали идентификатор, адрес буфера и номер датаграммы*/
    new_client_connect.buf = new_client_buf;
    new_client_connect.ident = ident;
    new_client_connect.data_n = data_n;

    /* создаем поток и записываем его идентификатор в стуктуру*/
    new_client_connect.sockaddr_p = (struct sockaddr_in *)malloc(sizeof
                                                                 (cliaddr));
    memcpy(new_client_connect.sockaddr_p, &cliaddr, (sizeof(cliaddr)));

    pthread_t udp_thread;
    pthread_create(&udp_thread, NULL,
                   send_data, (void*)input);

    new_client_connect.thread = udp_thread;

    /* копируем данные структуру клиента в массив */
    clients[client_array_cnt] = new_client_connect;

}

void update_client_data(char* input, int client_array_cnt, int* cnt_clients) {

    char *buf_pointer = clients[client_array_cnt].buf;
    memcpy(buf_pointer, input, MAXLINE);
    clients[client_array_cnt].buf = buf_pointer;
    memcpy(clients[client_array_cnt].sockaddr_p, &cliaddr, (sizeof(cliaddr)));
    int value = *cnt_clients;
    value++;
    *cnt_clients = value;
}

void get_data() {
    char buffer[MAXLINE];
    while (1) {

        int* cnt_clients = malloc(sizeof(int));
        *cnt_clients = 0;
        /* Читаем датаграмму */
        int len = sizeof(cliaddr);
        int n = recvfrom( sockfd, buffer, MAXLINE,
                          MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                          &len );

        /* Разбираем датаграмму и пересылаем изменения остальным клиентам */
        if (n != -1) {

            /* вытаскиваем идентификатор и номер датаграммы */
            char* input = buffer;

            int ident_client = *(int *)input;
            input += sizeof(int);
            int data_n = *(int *)input;

            for(int i = 0; i < MAX_CLIENTS; i++) {
                /*если идентификатор совпадает - клиент уже зарегестрирован*/
                /* просто обновляем его данные  */
                if ( clients[i].ident == ident_client ) {
                    if ( clients[i].data_n <= data_n ) {
                        /* printf("update \n"); */
                        update_client_data(buffer, i,  cnt_clients);
                    }
                    /* иначе регистрируем нового */
                } else if ( ( clients[i].ident == 0 ) &&
                            ( *cnt_clients == 0 ) ) {
                    register_new_client(buffer, data_n, ident_client, i);
                    break;
                }
            }
        }
        free( cnt_clients );
    }
}

int main(){
    init_socket();
    memset(clients, 0, sizeof(clients));
    get_data();
}
