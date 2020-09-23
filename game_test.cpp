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

struct sockaddr_in servaddr;

int sockfd;

struct pixel {
    int alive = 0;
    int x;
    int y;
} pixels[MAX_PIXELS];


// struct pixel {
//     int alive = 0;
//     int x;
//     int y;
// };

/* объявление мьютекса */
pthread_mutex_t show_pixels_mutex;

pthread_mutex_t mutex;

// предекларации
bool init();

bool create();

bool surface_create();

void show_box(int box_x, int box_y, int side_a, int side_b,
              int red, int green, int blue);

void DrawPixel(SDL_Surface *screen, int x, int y,
               Uint8 R, Uint8 G, Uint8 B);

int** pack_data_into_array(int* my_x, int* my_y, int* my_pix_y, int* my_pix_x,
                           int* x_enemy, int* y_enemy, int* pix_x_enemy,
                           int* pix_y_enemy,  int* ident);
int** init_values_and_box();

void driver_loop(int **data_array);

void Handle_Keydown(SDL_Keysym* keysym, int* x, int* y, int* pix_x, int* pix_y);

void move_box(int* x, int* y, int* x_side, int* y_side, int event);

void update_data(void* received_buffer, int* my_x, int* my_y, int* pix_y, int* pix_x,
                 int* my_ident, int* x_enemy, int* y_enemy,
                 int* pix_x_enemy, int* pix_y_enemy);

void show_pixels();

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

void* serialization(int* my_x, int* my_y, int* pix_y, int* pix_x,
                    int* ident, int* data_n) {

    /*выделяем память под буфер*/
    void * udp_buffer = malloc((sizeof(int) * 6) + sizeof(pixels));

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

    // printf("десериализация\n");
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
    printf("\n x_enemy %d y_enemy %d\n", *enemy_x, *enemy_y);
    printf("\n pix_x_enemy %d  pix_y_enemy %d\n", *enemy_pix_x,  *enemy_pix_y);
    // printf("\n pixel x %d\n ", *(int *)buffer);
    fflush(stdout);
    printf("\n");

    int j = 0;
    // десериализуем пиксели

    while (j < MAX_PIXELS) {
        pixels[j].alive = *(int *)buffer;
        buffer += sizeof(int);
        pixels[j].x = *(int *)buffer;
        buffer += sizeof(int);
        pixels[j].y = *(int *)buffer;
        buffer += sizeof(int);
        printf("j %d: pixel x %d pixel y %d pisel alive %d\n", j, pixels[j].x,
               pixels[j].y, pixels[j].alive );
        fflush(stdout);
        j++;
    }

    printf("\n");
    fflush(stdout);

    /* откываем мьютекс после выхода из цикла*/
    pthread_mutex_unlock(&mutex);
    /* освобождаем место в памяти */
    free(buffer_begin);
}

int** pack_data_into_array(int* my_x, int* my_y, int* my_pix_y, int* my_pix_x,
                           int* x_enemy, int* y_enemy, int* pix_x_enemy,
                           int* pix_y_enemy,  int* ident) {

    int** data_array_pnt = (int**)malloc(sizeof(int**) * 9);
    int** data_array = data_array_pnt;

    *data_array = my_x;
    data_array++;

    *data_array =  my_y;
    data_array++;

    *data_array = my_pix_y;
    data_array++;

    *data_array = my_pix_x;
    data_array++;

    *data_array = x_enemy;
    data_array++;

    *data_array = y_enemy;
    data_array++;

    *data_array = pix_x_enemy;
    data_array++;

    *data_array = pix_y_enemy;
    data_array++;

    *data_array = ident;
    printf("pack  data_array: %p\n", data_array_pnt);
    return data_array_pnt;
}

int** init_values_and_box() {
    srand(time(NULL));

    int* my_x = (int*)malloc(sizeof(int));
    *my_x = rand() % ( SCREEN_WIDTH - 11 );

    int* my_y = (int*)malloc(sizeof(int));
    *my_y = rand() % ( SCREEN_HEIGHT - 11 );

    int *my_pix_y = (int*)malloc(sizeof(int));
    *my_pix_y = 10;

    int *my_pix_x = (int*)malloc(sizeof(int));
    *my_pix_x = 10;

    int* x_enemy = (int*)malloc(sizeof(int));
    *x_enemy = 0;

    int* y_enemy = (int*)malloc(sizeof(int));
    *y_enemy = 0;

    int* pix_y_enemy = (int*)malloc(sizeof(int));
    *pix_y_enemy = 10;

    int* pix_x_enemy = (int*)malloc(sizeof(int));
    *pix_x_enemy = 10;

    /*идентификатор игрока-клиента*/
    int* ident = (int*)malloc(sizeof(int));
    *ident = rand() % 500;

    printf("my_ident %d \n", *ident);
    // printf("init_values: x_point %d y_point %d pix_x %d pix_y %d\n",
    //        *my_x, *my_y, *my_pix_x, *my_pix_y);

    // отрисовка квадратика
    show_box(*my_x, *my_y, *my_pix_x, *my_pix_y, 255, 255, 255);

    // упаковка значений в массив
    int** data_array  = pack_data_into_array(my_x, my_y, my_pix_y, my_pix_x,
                                             x_enemy, y_enemy, pix_x_enemy,
                                             pix_y_enemy, ident);

    // printf("init_values data_array: %p\n", data_array);

    return data_array;
}

void move_box(int* x, int* y, int* x_side, int* y_side, int event) {
    int event_type = event;
    int x_point = *x;
    int y_point = *y;
    int pix_x = *x_side;
    int pix_y = *y_side;

    // printf("move_box x_point %d y_point %d pix_x %d pix_y %d\n", x_point, y_point,
    //        pix_x, pix_y);
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

void send_data(int* my_x, int* my_y,
               int* my_pix_y, int* my_pix_x,
               int* my_ident, int* data_n) {

    void *send_buffer = serialization(my_x, my_y, my_pix_y, my_pix_x,
                                      my_ident, data_n);

    socklen_t len = sizeof(servaddr);

    ssize_t sended = sendto( sockfd, send_buffer, 1212, MSG_CONFIRM,
                             (const struct sockaddr *) &servaddr,
                             sizeof(servaddr) );

    int dub_data_n = *data_n;
    *data_n = dub_data_n + 1;

    printf(" send_data: data_n %d \n", *data_n);

    if ( sended == -1 ) {

        printf("::send_data:: Error: Send datagramm\n");
        exit(EXIT_FAILURE);
    }

    printf("пакет отправлен\n");
    fflush(stdout);
    // очищаем буфер с данными сериализации
    free(send_buffer);
}

void receive_data(int* my_x, int* my_y, int* my_pix_y, int* my_pix_x,
                  int* my_ident, int* x_enemy, int* y_enemy,
                  int* pix_x_enemy, int* pix_y_enemy) {

 begin:
    int buf_size = (sizeof(pixel) * 100 + sizeof(int) * 6);
    void* received_buffer = malloc( buf_size );

    socklen_t len = sizeof(servaddr);

    /* получаем данные */
    ssize_t received = recvfrom( sockfd, received_buffer, buf_size, MSG_WAITALL,
                                 (struct sockaddr *) &servaddr,
                                 (socklen_t *)&len );

    void* test_buffer = received_buffer;
    test_buffer += sizeof(int) * 6;
    int first_p_alive = *(int*)test_buffer;
    test_buffer += sizeof(int);
    int first_p_x = *(int*)test_buffer;
    test_buffer += sizeof(int);
    int first_p_y = *(int*)test_buffer;
    test_buffer += sizeof(int);

    printf("данные первого пикселя1 x %d, y %d, alive %d\n",
           first_p_x,  first_p_y,  first_p_alive);

    if ( received == -1 ) {
        // ждем пакеты
        free(received_buffer);
        goto begin;

    } else {
        printf("пакет принят\n");
        fflush(stdout);

        // void* test_buffer = received_buffer;
        // test_buffer += sizeof(int) * 6;
        // int first_p_alive = *(int*)test_buffer;
        // test_buffer += sizeof(int);
        // int first_p_x = *(int*)test_buffer;
        // test_buffer += sizeof(int);
        // int first_p_y = *(int*)test_buffer;
        // test_buffer += sizeof(int);

        // printf("данные первого пикселя2 x %d, y %d, alive %d\n",
        //        first_p_x,  first_p_y,  first_p_alive);

        // если все хорошо, обновляем данные
        update_data( received_buffer, my_x, my_y, my_pix_y, my_pix_x, my_ident,
                     x_enemy, y_enemy,
                     pix_x_enemy, pix_y_enemy );

    }
}

void show_pixels() {
    pthread_mutex_lock(&show_pixels_mutex);

    SDL_LockSurface(surface);
    printf("пиксели начали отрисовываться :\n");
    printf("\n");
    fflush(stdout);

    int x;
    int y;

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

void update_data(void* received_buffer, int* my_x, int* my_y, int* my_pix_y,
                 int* my_pix_x, int* my_ident, int* x_enemy, int* y_enemy,
                 int* pix_x_enemy, int* pix_y_enemy) {

    printf("обновление данных\n");
    fflush(stdout);

    // заводим переменные под дубликаты данных
    int* double_x_enemy = (int*)malloc(sizeof(int));
    int* double_y_enemy = (int*)malloc(sizeof(int));
    int* double_pix_x_enemy = (int*)malloc(sizeof(int));
    int* double_pix_y_enemy = (int*)malloc(sizeof(int));;

    int* double_my_x = (int*)malloc(sizeof(int));
    int* double_my_y = (int*)malloc(sizeof(int));
    int* double_my_pix_x = (int*)malloc(sizeof(int));
    int* double_my_pix_y = (int*)malloc(sizeof(int));;

    int* new_ident = (int*)malloc(sizeof(int));

    *double_x_enemy = *x_enemy;
    *double_y_enemy = *y_enemy;
    *double_pix_x_enemy = *pix_x_enemy;
    *double_pix_y_enemy = *pix_y_enemy;

    *double_my_pix_x = *my_pix_x;
    *double_my_pix_y = *my_pix_y;

    *new_ident = *(int *)received_buffer;

    // пришел пакет с нашими же данными?
    if ( *new_ident == *my_ident ) {

        printf("пришли собственные данные\n");
        fflush(stdout);

        deserialization( received_buffer, double_my_x, double_my_y,
                         double_my_pix_x, double_my_pix_y );

        // длина сторон персонажа увеличилась?
        if ( ( *double_my_pix_x != *my_pix_x ) ||
             ( *double_my_pix_y != *my_pix_y ) ) {

            // отрисовать фоном
            show_box( *my_x, *my_y,
                      *double_my_pix_x, *double_my_pix_y, 0, 0, 0 );

            // отрисовать цветом с новыми сторонами
            show_box( *my_x, *my_y,
                      *my_pix_x, *my_pix_y, 255, 255, 255 );
        }

        show_pixels();
        // пришли данные от другого персонажа - врага
    } else {

        deserialization( received_buffer, x_enemy,
                         y_enemy, pix_x_enemy, pix_y_enemy );

        // местоположение врага изменилось?
        if ( ( *double_x_enemy != *x_enemy ) ||
             ( *double_y_enemy != *y_enemy ) ) {

            // отрисовать фоном прежнее местоположении
            show_box( *double_x_enemy, *double_y_enemy,
                      *double_pix_x_enemy, *double_pix_y_enemy, 0, 0, 0 );

            // отрисовать цветом
            show_box( *x_enemy, *y_enemy, *pix_x_enemy,
                      *pix_y_enemy, 255, 0, 0 );
        }
        show_pixels();
    }
}

void* udp_socket(void* data_array) {

    // распаковка данных
    int** data_array_begin = (int**)data_array;

    int* my_x = *data_array_begin;
    data_array_begin++;

    int* my_y = *data_array_begin;
    data_array_begin++;

    int* my_pix_y = *data_array_begin;
    data_array_begin++;

    int* my_pix_x = *data_array_begin;
    data_array_begin++;

    int* x_enemy = *data_array_begin;
    data_array_begin++;

    int* y_enemy = *data_array_begin;
    data_array_begin++;

    int* pix_x_enemy = *data_array_begin;
    data_array_begin++;

    int* pix_y_enemy = *data_array_begin;
    data_array_begin++;

    int* my_ident = *data_array_begin;

    // создаем идентификатор датаграммы
    int* data_n = (int*)malloc(sizeof(int));
    *data_n = 0;

    while (true) {

        usleep(10000); // sleep for 0.01 sec

        // отправка сообщения
        send_data( my_x, my_y, my_pix_y, my_pix_x, my_ident, data_n );

        printf("data_n %d \n", *data_n);
        printf("цикл: пакет отправлен\n");

        // usleep(50000);

        // приняли сообщение
        receive_data( my_x, my_y, my_pix_y, my_pix_x, my_ident,
                      x_enemy, y_enemy, pix_x_enemy, pix_y_enemy );

        printf("цикл: пакет принят\n");

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

void driver_loop(int **data_array) {

    int** data_array_begin = data_array;

    int* my_x = *data_array_begin;
    data_array_begin++;

    int* my_y = *data_array_begin;
    data_array_begin++;

    int* my_pix_y = *data_array_begin;
    data_array_begin++;

    int* my_pix_x = *data_array_begin;
    data_array_begin++;

    int* x_enemy = *data_array_begin;
    data_array_begin++;

    int* y_enemy = *data_array_begin;
    data_array_begin++;

    int* pix_x_enemy = *data_array_begin;
    data_array_begin++;

    int* pix_y_enemy = *data_array_begin;
    data_array_begin++;

    int* my_ident = *data_array_begin;

    printf("driver_loop: my_x %d my_y %d my_pix_x %d my_pix_y %d\n",
           *my_x, *my_y, *my_pix_x, *my_pix_y);

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
        default:
            break;

        }
    }
}

void init_mutex() {

    mutex = PTHREAD_MUTEX_INITIALIZER;
    show_pixels_mutex = PTHREAD_MUTEX_INITIALIZER;

}
// struct pixel* make_pixel_array() {
//     int size =  sizeof(pixel) * MAX_PIXELS;
//     struct pixel* array = (struct pixel*)malloc(size);
//     memset(array, pixel, size);
//     return array;
// }

int main() {

    // printf("game main\n");

    if( !init() ) {
        printf( "Failed to initialize SDL!\n" );

    } else if( !create() ) {
        printf( "Failed to initialize window!\n" );

    } else if( !surface_create() ) {
        printf( "Failed to initialize surface!\n" );

    } else {

        int** data_array = init_values_and_box();
        memset(pixels, 0, sizeof(pixels));
        udp_init( data_array );
        init_mutex();
        driver_loop( data_array );
        return 0;
    }
}
