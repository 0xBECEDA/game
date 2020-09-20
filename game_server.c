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
#define MAXLINE  1220

/* объявляем промежуточный буфер */
char buffer[MAXLINE];

/* sockaddr_in сервера */
struct sockaddr_in servaddr;

/* sockaddr_in клиента(ов) */
struct sockaddr_in cliaddr;

int sockfd;

/* объявление структуры и массива пикселей */
struct pixel {
    int alive;
    int x;
    int y;
} pixels[MAX_PIXELS];


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

/* процедура генерации одного нового пикселя еды */
int PixelArray (void *p_pixels) {
    /* счетчик цикла, объявляется вне цикла, чтобы проанализировать
       пройден ли весь массив */
    int i;

    /* printf("\n"); */
    for (i = 0; i < MAX_PIXELS; i++) {
        if (0 == pixels[i].alive) {
            /*сгенерируем пиксели */
            int a;
            int b;
        generate_new_pixel:

            a = rand() % MAX_SIZE_X;
            b = rand() % MAX_SIZE_Y;

            if ( ( pixels[i].x == a) && (pixels[i].y == b) ) {
                goto generate_new_pixel;
            }

            pixels[i].x = a;
            pixels[i].y = b;
            pixels[i].alive = 1;
            /* printf("pixel x %d y %d\n", pixels[i].x, pixels[i].y); */
        }
    }

    /* Если после окончания цикла i равен максимальному значению
       переменной цикла - значит весь массив перебрали,
       но не нашли свободной структуры */
    if ( MAX_PIXELS >= i ) {
        return -1;

    } else {
        return 0;
    }
}

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

    /* printf("x_side %d y_side %d\n", *x_side, *y_side); */

    /*перезаписываем данные координат и сторон */
    memcpy(pnt, x, sizeof(int));
    pnt += sizeof(int);
    memcpy(pnt, y, sizeof(int));
    pnt += sizeof(int);

    memcpy(pnt, x_side, sizeof(int));
    pnt += sizeof(int);
    memcpy(pnt, y_side, sizeof(int));
    pnt += sizeof(int);

    // дополняем данными пикселей
    for (int i = 0; i <=99; i++) {

        *(int*)pnt = pixels[i].alive;
        pnt += sizeof(int);
        *(int*)pnt = pixels[i].x;
        pnt += sizeof(int);
        *(int*)pnt = pixels[i].y;
        pnt += sizeof(int);
    }
    return pointer;
}

void * counter (char * input, int* numpix) {

    int x = 0;
    int y = 0;
    int x_side = 0;
    int y_side = 0;

    void  * buffer = (void *)input;
    char *p = input;
    int eatten_pix = *numpix;

    /*десериализуем данные*/
    deserialization(buffer, &x, &y, &x_side, &y_side);

    /* printf("\n before: x %d\n y %d\n x_side %d\n y_side %d\n", */
    /*        x, y, x_side, y_side); */
    /* fflush(stdout); */

    for (int i = 0; i < MAX_PIXELS; i++) {
        /*если пиксель находится внутри квадрата*/
        if( ( pixels[i].alive == 1 )             &&
            ( pixels[i].x <= x + (x_side - 1) ) &&
            ( pixels[i].x > x )                 &&
            ( pixels[i].y <= y + (y_side - 1) ) &&
            ( pixels[i].y > y ) ) {
            /*то мы объявляем его как съеденный*/
            pixels[i].alive = 0;

            printf("\n x %d y %d x_side %d y_side %d\n",
                   x, y, x_side, y_side);
            printf("\n pixel x %d pixel y %d\n",
                   pixels[i].x, pixels[i].y);
            printf("\n");

            /*увеличиваем счетчик съеденных пикселей*/
            *numpix = eatten_pix + 1;

            printf("съеден\n");
            printf("numpix %d\n", *numpix);
            /*на каждом третьем пикселе квадрат увеличивается
              Пора увеличить?*/
            int result =  eatten_pix % 3;

            if (result == 0) {
                /* printf("увеличить\n"); */
                /* горизонталь и диагональ увеличиваются на 1*/
                x_side++;
                y_side++;

                printf("\n x_side %d y_side %d\n",
                       x, y, x_side, y_side);

                printf("\n");
            }
        }
    }

    /* printf("\n after: x %d\n y %d\n x_side %d\n y_side %d\n", */
    /*        x, y, x_side, y_side); */

    /*сериализуем обратно*/
    void * pnt = serialization(p, &x, &y, &x_side, &y_side);

    return pnt;
}

void* send_data(void* buf_pointer) {
    printf("Thread is going\n");
    int* numpix = (int*)malloc(sizeof(int));
    *numpix = 0;
    while(1) {

        /* sleep (2); */

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
                /* printf("cur_client_buf %p \n", cur_client_buf); */
                cur_client_buf = (char*)counter(cur_client_buf, numpix);
                /* printf("cur_client_buf %p \n", cur_client_buf); */
                /* printf("отправляю пакет от клиента %d к клиенту %d\n", */
                /*        ident, clients[i].ident); */

                /* printf("\n"); */
                int num =  sendto(sockfd, cur_client_buf, MAXLINE,
                                  MSG_CONFIRM,
                                  (struct sockaddr *)clients[i].sockaddr_p,
                                  sizeof(cliaddr));

                /* и ищем не совпадающий идентификатор */
                for (int i = 0; i < MAX_CLIENTS; i++) {

                    /* если идентификаторы разные - мы нашли, кому отправить данные*/
                    if (ident != clients[i].ident &&
                        clients[i].ident != 0 ) {

                        /*дополняем буфер данными*/

                        /* printf("отправляю пакет от клиента %d к клиенту %d\n", */
                        /*        ident, clients[i].ident); */
                        /* printf("\n"); */
                        /* отправляем пакет */
                        /* printf("client.sockaddr_p %p\n", clients[i].sockaddr_p); */
                        /* printf("\n"); */
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

void generate_pixels() {

    void* pixels = pixels;
    PixelArray(pixels);

}

void get_data_and_register_client() {

    int cnt = 0;

    while (1) {

        /* Создаем новые пиксели еды если есть возможность */
        generate_pixels();

        /* Читаем датаграмму */
        int len = sizeof(cliaddr);
        int n = recvfrom( sockfd, buffer, MAXLINE,
                          MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                          &len );

        /* Разбираем датаграмму и пересылаем изменения остальным клиентам */

        /* вытаскиваем идентификатор и номер датаграммы */
        char* input = buffer;

        int ident_client = *(int *)input;
        input += sizeof(int);
        int data_n = *(int *)input;

        for(int i = 0; i < MAX_CLIENTS; i++) {

            /*если идентификатор совпадает - клиент уже зарегестрирован*/
            /* просто обновляем его данные  */
            if( ( clients[i].ident == ident_client ) &&
                ( clients[i].data_n < data_n ) ) {

                char *buf_pointer = clients[i].buf;
                memcpy(buf_pointer, buffer, MAXLINE);
                clients[i].buf = buf_pointer;
                break;

            } else if ( clients[i].ident == 0 ) {

                struct connection new_client_connect = clients[i];
                /* выделяем память под буфер и перезаписываем туда данные */
                char *new_client_buf = malloc(MAXLINE);
                memcpy(new_client_buf, buffer, MAXLINE);

                /* записали идентификатор, адрес буфера и номер датаграммы*/
                new_client_connect.buf = new_client_buf;
                new_client_connect.ident = ident_client;
                new_client_connect.data_n = data_n;

                /* создаем поток и записываем его идентификатор в стуктуру*/
                /* переменная для хранения идентификатора потока */
                /* копируем данные структуру клиента в массив */
                new_client_connect.sockaddr_p = (struct sockaddr_in *)malloc(sizeof
                                                                             (cliaddr)
                                                                             );
                memcpy(new_client_connect.sockaddr_p, &cliaddr, (sizeof(cliaddr)));

                /* printf("new_client_connect.sockaddr_p %p\n", */
                /*        new_client_connect.sockaddr_p); */
                /* printf("new_client_connect.ident %d\n", new_client_connect.ident); */
                /* printf("\n"); */
                /* fflush(stdout); */

                pthread_t udp_thread;
                pthread_create(&udp_thread, NULL,
                               send_data, (void*)buffer);

                new_client_connect.thread = udp_thread;
                clients[i] = new_client_connect;

                /* printf("clients[i].sockaddr_p is %p\n", clients[i].sockaddr_p); */
                /* printf("clients[i].ident is %d\n", clients[i].ident); */
                /* printf("\n"); */
                /* fflush(stdout); */
                break;
            }
        }
    }
}

int main(){
    init_socket();
    memset(clients, 0, sizeof(clients));
    get_data_and_register_client();
}
