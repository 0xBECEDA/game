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

/* объявление структуры и массива пикселей */
struct pixel {
    char alive;
    int c;
    int d;
} pixels[MAX_PIXELS];


/* объявление структуры и массива клиентов */
struct connection {
    int thread;
    int ident;
    struct sockaddr_in *p;
    char *buf;
} clients[MAX_CLIENTS];

  /* индекс следующего клиента */
int new_client_idx = 0;

void * serialization(char * input, int x, int y, int x_side,
                     int y_side);
void deserialization (void * input, int x, int y, int x_side,
                      int y_side);

int numpix = 0;

/* sockaddr_in сервера */
struct sockaddr_in servaddr;

/* sockaddr_in клиента */
struct sockaddr_in cliaddr;

int sockfd;

/* процедура генерации одного нового пикселя еды */
int PixelArray (void *p_pixels) {
    /* счетчик цикла, объявляется вне цикла, чтобы проанализировать
       пройден ли весь массив */
    int i;
    for (i=0; i<MAX_PIXELS; i++) {
        if (0 == pixels[i].alive) {
            /*сгенерируем пиксели */
            int a;
            int b;
              generate_new_pixel:

               a = rand() % MAX_SIZE_X;
               b = rand() % MAX_SIZE_Y;

              if ((a == pixels[i].c) && (b == pixels[i].d)) {
                  goto generate_new_pixel;
              }

              pixels[i].c = a;
              pixels[i].d = b;
              pixels[i].alive = 1;
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
void deserialization(void * input, int x, int y, int x_side,
                     int y_side) {

    void * buffer = input;
    /*пропускаем идентификатор*/
    buffer += sizeof(int);

    /*десериаизуем координаты*/
    x = *(int *)buffer;
    // printf("in deserial int c %d\n", c);
    buffer += sizeof(int);
    y =  *(int *)buffer;
    buffer += sizeof(int);

    /*десериализуем размер сторон*/
    x_side = *(int *)buffer;
    buffer += sizeof(int);
    y_side = *(int *)buffer;
    buffer += sizeof(int);

}

void * serialization(char * input, int x, int y, int x_side,
                     int y_side) {

        /* сохраняем неизмененный указатель на буфер */
        char *pointer = input;
        printf("pointer in serial %X\n", pointer);
        /*пропускаем идентификатор*/
        void *pnt =  (void*)input + sizeof(int);
        printf("pnt in serial %X\n", pnt);

        /*перезаписываем данные координат и сторон */
        memcpy(pnt, &x, sizeof(x));
        pnt += sizeof(x);
        memcpy(pnt, &y, sizeof(y));
        pnt += sizeof(y);

        memcpy(pnt, &x_side, sizeof(x_side));
        pnt += sizeof(x_side);
        memcpy(pnt, &y_side, sizeof(y_side));
        pnt += sizeof(y_side);

        /*дополняем данными пикселей*/
        for (int i = 0; i <=99; i++) {

            *(char*)pnt = pixels[i].alive;
            pnt += sizeof(char);
            *(char*)pnt = pixels[i].c;
            pnt += sizeof(char);
            *(char*)pnt = pixels[i].d;
            pnt += sizeof(char);
        }
        return pointer;
    }


void * counter (char * input) {

    int x = 0;
    int y = 0;
    int x_side = 0;
    int y_side = 0;

    void  * buffer = (void *)input;

    char *p = input;

    /*десериализуем данные*/
    deserialization(buffer, &x, &y, &x_side, &y_side);


    for (int i= 0; i <= 99; i++) {
            /*если пиксель находится внутри квадрата*/
            if(pixels[i].c <= x + (x_side - 1) &&
               pixels[i].c > x &&
               pixels[i].d <= y + (y_side - 1) &&
               pixels[i].d >= y) {
                /*то мы объявляем его как съеденный*/
                pixels[i].alive = 0;

                /*увеличиваем счетчик съеденных пикселей*/
                numpix++;

                /*на каждом третьем пикселе квадрат увеличивается
                  Пора увеличить?*/
                int result =  numpix % 3;
                if (result == 0) {

                    /* горизонталь и диагональ увеличиваются на 1*/
                    x_side++;
                    y_side++;

                }
            }
        }


    /*сериализуем обратно*/
    char * pnt;
    return pnt =  serialization(p, &x, &y, &x_side, &y_side);

}

void* udp_socket(void* pointer) {
    printf("Thread is going\n");
    while(1) {

        /* получаем идентификатор клиента */
        void *pnt = pointer;
        int ident = *(int *)pnt;

        /*заводим структуру, чтоб загрузить в нее сохраненные из cliaddr данные*/
        struct sockaddr_in dub_client;

        /*заводим структуру, чтоб позже скопировать в нее данные клиента*/
        struct connection client;

        for (int i = 0; i <=1; i++) {

            /* если идентификатор из буфера совпадает
               с идентификатором  клиента */

            if (ident == clients[i].ident) {
                /* то получаем указатель на его буфер */
                char *p = clients[i].buf;

                /* и ищем не совпадающий идентификатор */
                for (int i = 0; i <=1; i++) {
                    /* если идентификаторы разные */
                    if (ident != clients[i].ident &&
                        clients[i].ident != 0 ) {

                        /* то загружаем сохранненные из структуры cliaddr данные */
                        client = clients[i];
                        dub_client = *client.p;

                        /*дополняем буфер данными*/
                        counter(p);

                        /* отправляем пакет */
                        int n =  sendto(sockfd, p, MAXLINE,
                                        MSG_CONFIRM,
                                        (struct sockaddr *) &dub_client,
                                        sizeof(cliaddr));
                    }
                }
            }
        }
    }
}

int  main() {

    /* объявляем промежуточный буфер */
    char buffer[MAXLINE];

    /*массив для хранения данных структур cliaddr*/
    struct sockaddr_in dub_array[2];
    int cnt = 0;

      /* Создаем сокет. Должны в случае успеха получить его дескриптор */

      if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
          perror("socket creation failed");
          exit(EXIT_FAILURE);
      }

      /* заполняем данные о сервере */
      servaddr.sin_family = AF_INET;
      servaddr.sin_addr.s_addr = INADDR_ANY;
      servaddr.sin_port = htons(PORT);


      memset(dub_array, 0, sizeof(dub_array));
      memset(clients, 0, sizeof(clients));


      /* привязываем сокет к адресу */
      if ( bind( sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr) ) < 0 ) {
          perror("bind failed");
          exit(EXIT_FAILURE);
      }

    while (1) {
        /* Создаем новые пиксели еды если есть возможность */
        void * pixels = &pixels;
        PixelArray(&pixels);

        /* Читаем датаграмму */
        int len = sizeof(cliaddr);
        int n = recvfrom( sockfd, buffer, MAXLINE,
                         MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                         &len );

        /* передаем указатель на массив c данными структур cliaddr */
        struct sockaddr_in *pnt = dub_array;

        /* Разбираем датаграмму и пересылаем изменения остальным клиентам */


        /* вытаскиваем идентификатор */
        int ident_client = *(int *)buffer;


        for(int i = 0; i<=1; i++) {
            int counter = 0;

            /*если идентификатор совпадает*/
            if( clients[i].ident == ident_client) {
                char *point = clients[i].buf;
                //printf("char *p, если ident совпал  %X\n", point);
                memcpy(point, buffer, MAXLINE);
                clients[i].buf = point;
                counter++;
                break;
            }


            /*если структура пустая и счетчик нулевой*/
            if( ( clients[i].ident == 0 ) && ( counter == 0 ) ) {

                /* то записываем данные клиента в массив */
                clients[i].ident = ident_client;

                /* выделяем память по буфер и перезаписываем туда данные */
                char *p = malloc(MAXLINE);
                memcpy(p, buffer, MAXLINE);
                clients[i].buf = p;


                void* pointer = NULL;

                 /* переменная для хранения идентификатора потока */
                 pthread_t udp_thread;

                 /* создаем поток */
                 pthread_create(&udp_thread, NULL,
                                udp_socket, pointer);


                /* кладем идентификатор потока в структуру */
                clients[i].thread = udp_thread;

                /* копируем данные структуру клиента в массив */
                dub_array[cnt] = cliaddr;

                clients[i].p = pnt;
                printf("pnt of struct is %X\n", pnt);
                printf("clients[i].p is %X\n", clients[i].p);
                printf ("clients[i].ident is %d\n", clients[i].ident);
                fflush(stdout);

                pnt += 1;
                cnt++;
                break;
            }
        }

    }
}
