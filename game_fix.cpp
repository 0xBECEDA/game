#include <SDL2/SDL.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>


SDL_Window* gWindow = NULL;
SDL_Surface* surface = NULL;
SDL_Event event;
SDL_Keysym keysym;
int SCREEN_WIDTH = 480;
int SCREEN_HEIGHT = 520;
#define MAX_PIXELS 100

bool init();
bool create();
bool surface_create();

void* serialization(int* my_x, int* my_y, int* pix_y, int* pix_x,
                    int* ident, int* data_n);

void deserialization(void * input, int* enemy_x, int* enemy_y,
                     int* enemy_pix_y, int* enemy_pix_x);

void move_box(int* x, int* y, int* x_side, int* y_side, int event);

void show_pixels();
/* объявление мьютекса */
pthread_mutex_t mutex;

/* объявление мьютекса */
pthread_mutex_t show_pixels_mutex;

struct sockaddr_in servaddr;

int sockfd;

struct pixel {
    int alive = 0;
    int x;
    int y;
} pixels[100];

bool init() {
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        return false;
    }
    return true;
}

bool create() {
    if ( !( gWindow = SDL_CreateWindow("SDL Tutorial",
                                       SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED,
                                       SCREEN_WIDTH,
                                       SCREEN_HEIGHT,
                                       SDL_WINDOW_SHOWN)) ) {
        printf( "SDL_CreateWindow() failed! SDL_Error: %s\n", SDL_GetError() );
        return false;
    }
    return true;
}

bool surface_create() {
    if ( !(surface = SDL_GetWindowSurface(gWindow)) ) {
        printf ("Didn't create surface! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    return true;
}



void DrawPixel(SDL_Surface *screen, int x, int y,
               Uint8 R, Uint8 G, Uint8 B) {
    Uint32 color = SDL_MapRGB(surface->format, R, G, B);
    int bpp =  surface->format->BytesPerPixel;
    Uint32 ppr = surface->pitch/bpp;

    switch (bpp) {
    case 1:
        {
            Uint8 *p = (Uint8 *)surface->pixels + (y * ppr + x )* bpp;
            *p = color;
        }
        break;
    case 2:
        {
            Uint16 *p = (Uint16 *)surface->pixels + (y * ppr + x );
            *p = color;
        }
        break;
    case 3:
        {
            Uint8 *p = (Uint8 *)surface->pixels +
                y*surface->pitch + x * 3;
            if(SDL_BYTEORDER == SDL_LIL_ENDIAN)
                {
                    p[0] = color;
                    p[1] = color >> 8;
                    p[2] = color >> 16;
                } else {
                p[2] = color;
                p[1] = color >> 8;
                p[0] = color >> 16;
            }
        }
        break;
    case 4:
        {
            Uint32 *p = (Uint32 *)surface->pixels + (y * ppr + x );
            *p = color;
        }
        break;
    default:
        printf("DrawPixel ERR: Unknown type of pixel \n");
        exit(0);
    }
}



void show_box(int box_x, int box_y, int side_a, int side_b,
              int red, int green, int blue) {
    int side_x = side_a;
    int side_y = side_b;
    int max_y = box_y + side_y;
    int max_x = box_x + side_x;

    // printf("\n show_box: box_x %d box_y %d side_x %d side_y %d\n ", box_x, box_y,
    //        side_x, side_y);

    SDL_LockSurface(surface);

    int cnt = 0;
    for ( int j = box_y; j < max_y; j++ ) {
        for ( int i = box_x; i < max_x; i++ ) {
            DrawPixel(surface, i, j, red, green, blue);
        }
    }
    SDL_UnlockSurface(surface);
    SDL_UpdateWindowSurface(gWindow);
}


void move_box(int* x, int* y, int* x_side, int* y_side, int event) {
    int event_type = event;
    int x_point = *x;
    int y_point = *y;
    int pix_x = *x_side;
    int pix_y = *y_side;

    switch(event_type) {

    case 3:
        show_box(x_point, y_point, pix_x, pix_y, 0, 0, 0);
        x_point++;
        *x = x_point;
        show_box(x_point, y_point, pix_x, pix_y, 255, 255, 255);
        break;

    case 4:
        show_box(x_point, y_point, pix_x, pix_y, 0, 0, 0);
        x_point--;
        *x = x_point;
        show_box(x_point, y_point, pix_x, pix_y, 255, 255, 255);
        break;

    case 5:
        show_box(x_point, y_point, pix_x, pix_y, 0, 0, 0);
        y_point++;
        *y = y_point;
        show_box(x_point, y_point, pix_x, pix_y, 255, 255, 255);
        break;
    case 6:
        show_box(x_point, y_point, pix_x, pix_y, 0, 0, 0);
        y_point--;
        *y = y_point;
        show_box(x_point, y_point, pix_x, pix_y, 255, 255, 255);
        break;
    default:
        printf("move_box ERR: Unknoun type of event \n");
    }
}

void* udp_socket(void* data_array) {

    int** data_array_begin = (int**)data_array;

    // printf("udp_socket data_array_begin %p\n", data_array_begin);

    int* my_x = *data_array_begin;
    data_array_begin += sizeof(int);
    // printf("my_x %p\n", my_x);

    int* my_y = *data_array_begin;
    data_array_begin += sizeof(int);

    int* my_pix_y = *data_array_begin;
    data_array_begin += sizeof(int);

    int* my_pix_x = *data_array_begin;
    data_array_begin += sizeof(int);

    int* x_enemy = *data_array_begin;
    data_array_begin += sizeof(int);

    int* y_enemy = *data_array_begin;
    data_array_begin += sizeof(int);

    int* pix_x_enemy = *data_array_begin;
    data_array_begin += sizeof(int);

    int* pix_y_enemy = *data_array_begin;
    data_array_begin += sizeof(int);

    int* my_ident = *data_array_begin;

    int* data_n = (int*)malloc(sizeof(int));
    *data_n = 0;

    int* check_x_enemy = (int*)malloc(sizeof(int));
    int* check_y_enemy = (int*)malloc(sizeof(int));
    int* check_x_my = (int*)malloc(sizeof(int));
    int* check_y_my = (int*)malloc(sizeof(int));
    int* check_my_pix_x = (int*)malloc(sizeof(int));
    int* check_my_pix_y = (int*)malloc(sizeof(int));;
    int* new_ident = (int*)malloc(sizeof(int));

    while (true) {

        usleep(10000); // sleep for 0.01 sec
        // sleep( 2 );

        void *send_buffer = serialization(my_x, my_y, my_pix_y, my_pix_x,
                                          my_ident, data_n);

        printf("данные сериализованны:  \n");
        printf("my_x %d my_y %d my_pix_x %d my_pix_y %d\n",
               *my_x, *my_y, *my_pix_x, *my_pix_y);

        socklen_t len = sizeof(servaddr);

        ssize_t sended = sendto( sockfd, send_buffer, 1212, MSG_CONFIRM,
                                 (const struct sockaddr *) &servaddr,
                                 sizeof(servaddr) );
        // очищаем буфер с данными сериализации
        free(send_buffer);

        printf("пакеты отправлены\n");
        printf("\n");
        fflush(stdout);

        int dub_data_n = *data_n;
        *data_n = dub_data_n++;;

        if(-1 == sended) {
            printf("::udp_socket():: Error: Send datagramm\n");
            exit(EXIT_FAILURE);

        } else {

            void*  received_buffer = malloc(sizeof(pixel) * 100 + sizeof(int) * 6);
            printf("received_buffer size %d\n",
                   sizeof(pixel) * 100 + sizeof(int) * 6);
            /* получаем данные */
            ssize_t received = recvfrom( sockfd, received_buffer, 1212, MSG_WAITALL,
                                         (struct sockaddr *) &servaddr,
                                         (socklen_t *)&len );

            /* если пакеты получены */
            if ( received != -1 ) {

                printf("пакеты получены\n");

                check_x_enemy = x_enemy;
                check_y_enemy = y_enemy;
                check_my_pix_x = my_pix_x;
                check_my_pix_y = my_pix_y;
                new_ident = (int *)received_buffer;

                // printf("\n my_x %d my_y %d\n", *my_x, *my_y);
                // printf("\n my_pix_x %d  my_pix_y %d\n", *my_pix_x,
                //        *my_pix_y);
                // printf("\n");

                /*десериализуем новые*/

                if (*new_ident == *my_ident) {

                    deserialization(received_buffer, check_x_my, check_y_my,
                                    my_pix_x, my_pix_y);

                    show_pixels();

                    printf("данные десериализованны:  \n");

                    printf("my_x %d my_y %d my_pix_x %d my_pix_y %d\n",
                           *my_x, *my_y, *my_pix_x, *my_pix_y);
                    printf("check_x_my %d check_y_my %d\n",
                           *check_x_my, *check_y_my);
                    printf("\n");
                    fflush(stdout);

                    if ( (*check_my_pix_x != *my_pix_x ) ||
                         (*check_my_pix_y != *my_pix_y ) ) {

                        printf("стороны изменились: \n");
                        show_box(*my_x, *my_y,
                                 *check_my_pix_x, *check_my_pix_y, 0, 0, 0);

                        show_box(*my_x, *my_y,
                                 *my_pix_x, *my_pix_y, 255, 255, 255);

                        // printf("after my_x %d my_y %d my_pix_x %d my_pix_y %d\n",
                        //        *my_x, *my_y, *my_pix_x, *my_pix_y);
                        printf("\n");
                        fflush(stdout);

                        printf("пиксели отрисованы\n");
                        fflush(stdout);
                    }
                } else {
                    deserialization(received_buffer, x_enemy,
                                    y_enemy, pix_x_enemy, pix_y_enemy);

                    printf("данные десериализованны: \n");

                    printf("x_enemy %d y_enemy %d pix_x_enemy %d pix_y_enemy %d\n",
                           *x_enemy, *y_enemy, *pix_x_enemy, *pix_y_enemy);
                    printf("\n");
                    fflush(stdout);

                    show_pixels();

                    printf("пиксели отрисованы\n");
                    fflush(stdout);
                    /*проверяем, не изменились ли координаты*/
                    if ( ( *check_x_enemy != *x_enemy ) ||
                         ( *check_y_enemy != *y_enemy ) ) {
                        /*если координаты изменились,
                          то отрисовываем старые координаты фоном*/
                        show_box(*check_x_enemy, *check_y_enemy,
                                 *pix_x_enemy, *pix_y_enemy, 0, 0, 0);
                        /*затем отрисовываем */
                        show_box(*x_enemy, *y_enemy, *pix_x_enemy,
                                 *pix_y_enemy, 255, 0, 0);
                    }
                }
            }
        }
    }
}

pthread_t udp_init(int** data_array) {
    // Создаем сокет.
    // Должны в случае успеха получить его дескриптор
    // в глобальную переменную sockfd
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0) ) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Переводим сокет в неблокирующий режим
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    // заполняем данные о сервере
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    /*создаем новый поток*/

    /* создаем новый поток */
    pthread_t udp_thread;

    void *(*thread_func)(void *) = udp_socket;

    if( 0 != pthread_create( &udp_thread, NULL, thread_func,
                             (void*)data_array) ) {
        perror("thread_create failed");
        exit(EXIT_FAILURE);
    }
    // printf("udp_thread %d\n", udp_thread);

    return udp_thread;
}

void show_pixels() {
    pthread_mutex_lock(&show_pixels_mutex);

    SDL_LockSurface(surface);
    printf("пиксели начали отрисовываться :\n");
    printf("\n");
    fflush(stdout);


    int x;
    int y;
    // printf("\n");
    for(int i = 0; i < MAX_PIXELS; i++) {
        if ( pixels[i].alive == 1 ) {
            x = pixels[i].x;
            y = pixels[i].y;
            printf("i %d: pixel x %d y %d\n", i,  pixels[i].x, pixels[i].y);
            fflush(stdout);
            DrawPixel(surface, x, y, 0, 255, 0);
        } else {
            printf("i %d: pixel x %d y %d\n", i,  pixels[i].x, pixels[i].y);
            fflush(stdout);
            DrawPixel(surface, x, y, 0, 0, 0);
        }
    }

    SDL_UnlockSurface(surface);
    SDL_UpdateWindowSurface(gWindow);
    pthread_mutex_unlock(&show_pixels_mutex);

}

void* serialization(int* my_x, int* my_y, int* pix_y, int* pix_x,
                    int* ident, int* data_n) {

    /*выделяем память под буфер*/
    void * udp_buffer = malloc((sizeof(int) * 6) + sizeof(pixels));

    // void * udp_buffer = malloc(sizeof(int) * 5);

    // printf(" Size is %d\n", sizeof(int) * 5 + sizeof(pixels));
    /* сохраняем неизмененный указатель на буфер */

    void *begin_udp_buffer = udp_buffer;

    /* сериализуем идентификатор квадратика */
    memcpy(udp_buffer, ident, sizeof(int));
    udp_buffer += sizeof(int);

    // сериализуем номер датаграммы
    memcpy(udp_buffer, data_n, sizeof(int));
    udp_buffer += sizeof(int);

    /*сериализуем координаты квадратика и его размер*/
    memcpy(udp_buffer, my_x, sizeof(int));
    udp_buffer += sizeof(int);

    memcpy(udp_buffer, my_y, sizeof(int));
    udp_buffer += sizeof(int);

    memcpy(udp_buffer, pix_y, sizeof(int));
    udp_buffer += sizeof(int);

    memcpy(udp_buffer, pix_x, sizeof(int));
    udp_buffer += sizeof(int);

    return begin_udp_buffer;
}

void deserialization (void * input, int* enemy_x, int* enemy_y,
                      int* enemy_pix_y, int* enemy_pix_x) {

    printf("десериализация\n");
    void * buffer_begin = input;

    /*сохраняем неизмененный указатель*/
    void * buffer = input;

    /*пропускаем идентификатор, он нам не нужен*/
    int ident = *(int *)buffer;
    // printf("ident is %d\n", ident);
    buffer += sizeof(int);

    // пропускаем номер датаграммы
    int data_n = *(int *)buffer;
    buffer += sizeof(int);

    /*десериализуем данные врага*/
    /* закрываем мьютекс здесь,
       т.к. это критическая секция кода*/
    pthread_mutex_lock(&mutex);

    *enemy_x = *(int *)buffer;
    buffer += sizeof(int);
    *enemy_y = *(int *)buffer;
    buffer += sizeof(int);

    *enemy_pix_y = *(int *)buffer;
    buffer += sizeof(int);
    *enemy_pix_x = *(int *)buffer;
    buffer += sizeof(int);
    // printf("\n x_enemy %d y_enemy %d\n", *enemy_x, *enemy_y);
    // printf("\n pix_x_enemy %d  pix_y_enemy %d\n", *enemy_pix_x,  *enemy_pix_y);
    // printf("\n");

    int j = 0;
    // десериализуем пиксели

    while (j < MAX_PIXELS) {
        //printf("..........\n");
        pixels[j].alive = *(int *)buffer;
        buffer += sizeof(int);
        //printf("buffer in %d iteration is %X\n", j, buffer);
        pixels[j].x = *(int *)buffer;
        buffer += sizeof(int);
        //printf("buffer in %d iteration is %X\n", j, buffer);
        pixels[j].y = *(int *)buffer;
        buffer += sizeof(int);
        //printf("buffer in %d iteration is %X\n", j, buffer);
        printf("j %d: pixel x %d pixel y %d pisel alive %d\n", j, pixels[j].x,
               pixels[j].y, pixels[j].alive );
        fflush(stdout);
        j++;
    }

    printf("\n");
    /* откываем мьютекс после выхода из цикла*/
    pthread_mutex_unlock(&mutex);
    /* освобождаем место в памяти */
    free(buffer_begin);
}

void Handle_Keydown(SDL_Keysym* keysym, int* x, int* y, int* pix_x, int* pix_y) {
    SDL_Event event;
    switch(keysym->sym) {

    case SDLK_RIGHT:
        if (*x != SCREEN_WIDTH - *pix_x) {
            move_box(x, y, pix_x, pix_y, 3);
        }
        break;

    case SDLK_LEFT:
        if (*x != 0) {
            move_box(x, y, pix_x, pix_y, 4);
        }
        break;

    case SDLK_DOWN:
        if (*y != SCREEN_HEIGHT - *pix_y) {
            move_box(x, y, pix_x, pix_y, 5);
        }
        break;

    case SDLK_UP:
        if (*y != 0) {
            move_box(x, y, pix_x, pix_y, 6);
        }
        break;

    default:
        printf("Can't find this key\n");
        break;
    }
}

int** pack_data_into_array(int* my_x, int* my_y, int* my_pix_y, int* my_pix_x,
                           int* x_enemy, int* y_enemy, int* pix_x_enemy,
                           int* pix_y_enemy,  int* ident) {

    int** data_array_pnt = (int**)malloc(sizeof(int**) * 9);
    int** data_array = data_array_pnt;

    *data_array = my_x;
    data_array += sizeof(my_x);

    *data_array =  my_y;
    data_array += sizeof(my_y);

    *data_array = my_pix_y;
    data_array += sizeof(my_pix_y);

    *data_array = my_pix_x;
    data_array += sizeof(my_pix_x);

    *data_array = x_enemy;
    data_array += sizeof(x_enemy);

    *data_array = y_enemy;
    data_array += sizeof(y_enemy);

    *data_array = pix_x_enemy;
    data_array += sizeof(pix_x_enemy);

    *data_array = pix_y_enemy;
    data_array += sizeof(pix_y_enemy);

    *data_array = ident;

    // printf("my_x *data_array end %p\n", *data_array_pnt);

    return data_array_pnt;
}

int main() {

    printf("game main\n");

    if( !init() ) {
        printf( "Failed to initialize SDL!\n" );

    } else if( !create() ) {
        printf( "Failed to initialize window!\n" );

    } else if( !surface_create() ) {
        printf( "Failed to initialize surface!\n" );

    } else {

        srand(time(NULL));

        int* my_x = (int*)malloc(sizeof(int));
        *my_x = rand() % 500;

        int* my_y = (int*)malloc(sizeof(int));
        *my_y = 0;

        int *my_pix_y = (int*)malloc(sizeof(int));
        *my_pix_y = 10;

        int *my_pix_x = (int*)malloc(sizeof(int));
        *my_pix_x = 10;

        int* x_enemy = (int*)malloc(sizeof(int));
        *x_enemy = 0;

        int* y_enemy = (int*)malloc(sizeof(int));
        *y_enemy = 0;

        /* размер сторон врага */
        int* pix_y_enemy = (int*)malloc(sizeof(int));
        *pix_y_enemy = 10;

        int* pix_x_enemy = (int*)malloc(sizeof(int));
        *pix_x_enemy = 10;

        /*идентификатор игрока-клиента*/
        int* ident = (int*)malloc(sizeof(int));
        *ident = rand() % 500;

        // printf("game: ident %d\n", *ident);

        // printf("my_x %p\n", my_x);
        // printf("my_y %p\n", my_y);
        // printf("my_pix_y %p\n", my_pix_y);
        // printf("my_pix_x %p\n", my_pix_x);
        // printf("x_enemy %p\n", x_enemy);
        // printf("y_enemy %p\n", y_enemy);
        // printf("pix_x_enemy %p\n", pix_x_enemy);
        // printf("pix_y_enemy %p\n", pix_y_enemy);
        // printf("ident %p\n", ident);
        // printf("\n");

        int** data_array  = pack_data_into_array(my_x, my_y, my_pix_y, my_pix_x,
                                                 x_enemy, y_enemy, pix_x_enemy,
                                                 pix_y_enemy, ident);
        // printf("game здесь\n");
        /* создаем сокет */
        SDL_LockSurface(surface);
        srand(time(NULL));

        show_box(*my_x, *my_y, *my_pix_x, *my_pix_y, 255, 255, 255);

        SDL_UnlockSurface(surface);
        SDL_UpdateWindowSurface(gWindow);

        /* создаем мьютекс */
        mutex = PTHREAD_MUTEX_INITIALIZER;
        show_pixels_mutex = PTHREAD_MUTEX_INITIALIZER;
        /* создаем сокет */

        udp_init( data_array );
        // printf("инициализация udp прошла успешно\n");

        while (256 != event.type) {
            SDL_WaitEventTimeout(&event, 100);

            switch (event.type) {
            case SDL_MOUSEMOTION:
                break;

            case SDL_KEYDOWN:
                Handle_Keydown(&event.key.keysym, my_x, my_y, my_pix_y, my_pix_x);
                break;

            case SDL_WINDOWEVENT:
                break;

                // default:
                //     printf("event loop ERR: Unknown type of event.type \n");
            }
        }
    }
}
