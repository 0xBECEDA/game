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

bool init();
bool create();
bool surface_create();



void move_box( int &X, int &Y, int event);


int X = 0;
int Y = 0;

int pix_y = 10;
int pix_x = 10;




/* объявление мьютекса */
pthread_mutex_t mutex;



/*идентификатор игрока-клиента*/
int identificator;



int X_enemy = 0;
int Y_enemy = 0;

/* размер сторон врага */
int pix_y_enemy = 10;
int pix_x_enemy = 10;



void* serialization();

void deserialization (void * input);



int sockfd;
struct sockaddr_in servaddr;



/* максимальное кол-во пикселей еды на поле */
#define MAX_PIXELS 100

struct pixel {
    char alive = 0;
    int c;
    int d;
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
    SDL_LockSurface(surface);


    int cnt = 0;
    for ( int j = box_y; j < max_y; j++) {
        for ( int i = box_x; i < max_x; i++) {
            DrawPixel(surface, i, j, red, green, blue);
        }
    }
    SDL_UnlockSurface(surface);
    SDL_UpdateWindowSurface(gWindow);
}



void move_box(int &X, int &Y, int event) {

    int event_type = event;

    switch(event_type) {

    case 3:
        show_box(X, Y, pix_x, pix_y, 0, 0, 0);
        X++;
        show_box(X, Y, pix_x, pix_y, 255, 255, 255);
        break;

    case 4:
        show_box(X, Y, pix_x, pix_y, 0, 0, 0);
        X--;
        show_box(X, Y, pix_x, pix_y, 255, 255, 255);
        break;

    case 5:
        show_box(X, Y, pix_x, pix_y, 0, 0, 0);
        Y++;
        show_box(X, Y, pix_x, pix_y, 255, 255, 255);

    case 6:
        show_box(X, Y, pix_x, pix_y, 0, 0, 0);
        Y--;
        show_box(X, Y, pix_x, pix_y, 255, 255, 255);
        break;
    default:
        printf("move_box ERR: Unknoun type of event \n");
    }
}


void* udp_socket(void* pointer) {
    while (true) {

        usleep(10000); // sleep for 0.01 sec

        /* сериализуем данные*/

        void *buffer = serialization();

        socklen_t len = sizeof(servaddr);

        ssize_t sended = sendto( sockfd, buffer, 1212, MSG_CONFIRM,
                                 (const struct sockaddr *) &servaddr,
                                 sizeof(servaddr) );
        if(-1 == sended) {
            printf("::udp_socket():: Error: Send datagramm\n");
            exit(EXIT_FAILURE);

        } else {
            /* получаем данные */
            ssize_t received = recvfrom( sockfd, buffer, 1212, MSG_WAITALL,
                                         (struct sockaddr *) &servaddr,
                                         (socklen_t *)&len );

            /* если пакеты получены */
            if ( received != -1 ) {
                /*копируем старые данные врага*/
                int check_X = X_enemy;
                int check_Y = Y_enemy;

                /*десериализуем новые*/
                deserialization(buffer);
                /*проверяем, не изменились ли координаты*/
                if ( check_X != X_enemy || check_Y != Y_enemy) {
                    /*если координаты изменились,
                      то отрисовываем старые координаты фоном*/
                    show_box(check_X, check_Y, pix_x_enemy, pix_y_enemy, 0, 0, 0);

                } else {
                    /*затем отрисовываем */
                    show_box(X_enemy, Y_enemy, pix_x_enemy, pix_y_enemy, 255, 0, 0);
                }
            }
        }
    }
}


pthread_t udp_init() {
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
    
    if( 0 != pthread_create( &udp_thread, NULL, thread_func, NULL ) ) {
        perror("thread_create failed");
        exit(EXIT_FAILURE);
    }

    return udp_thread;
}



void* serialization() {
    /*выделяем память под буфер*/
    void * udp_buffer = malloc((sizeof(int) * 5) + sizeof(pixels));
    printf(" Size is %d\n", sizeof(int) * 5 + sizeof(pixels));
    /* сохраняем неизмененный указатель на буфер */
    void *pnt = udp_buffer;

    /* сериализуем идентификатор квадратика */
    memcpy(udp_buffer, &identificator, sizeof(int));
    udp_buffer += sizeof(identificator);

    /*сериализуем координаты квадратика и его размер*/
    memcpy(udp_buffer, &X, sizeof(X));
    udp_buffer += sizeof(X);
    memcpy(udp_buffer, &Y, sizeof(Y));
    udp_buffer += sizeof(Y);

    memcpy(udp_buffer, &pix_y, sizeof(pix_y));
    udp_buffer += sizeof(pix_y);
    memcpy(udp_buffer, &pix_x, sizeof(pix_x));
    udp_buffer += sizeof(pix_x);

    return pnt;
}



void deserialization (void * input) {
    void * buffer = input;
    /*сохраняем неизмененный указатель*/
    void * pnt = input;
    int i = 0;

    /*пропускаем идентификатор, он нам не нужен*/
    int ident = *(int *)buffer;
    // printf("ident is %d\n", ident);
    buffer += sizeof(int);

    /*десериализуем данные врага*/
    /* закрываем мьютекс здесь,
       т.к. это критическая секция кода*/
    pthread_mutex_lock(&mutex);
    X_enemy = *(int *)buffer;
    buffer += sizeof(int);
    Y_enemy  = *(int *)buffer;
    buffer += sizeof(int);

    pix_y_enemy = *(int *)buffer;
    buffer += sizeof(int);
    pix_x_enemy = *(int *)buffer;
    buffer += sizeof(int);
    //printf("X_enemy %d Y_enemy %d\n", X_enemy, Y_enemy);
    int j = 0;
    /* десериализуем пиксели */

    while (j <=99) {
        //printf("..........\n");
        pixels[j].alive = *(char *)buffer;
        buffer += sizeof(char);
        //printf("buffer in %d iteration is %X\n", j, buffer);
        pixels[j].c = *(int *)buffer;
        buffer += sizeof(int);
        //printf("buffer in %d iteration is %X\n", j, buffer);
        pixels[j].d = *(int *)buffer;
        buffer += sizeof(int);
        //printf("buffer in %d iteration is %X\n", j, buffer);
        j++;
    }

    /* откываем мьютекс после выхода из цикла*/
    pthread_mutex_unlock(&mutex);
    /* освобождаем место в памяти */
    free(pnt);
}


void Handle_Keydown(SDL_Keysym* keysym) {

    SDL_Event event;
    switch(keysym->sym) {

    case SDLK_3:
        printf("3 is pressed\n");
        if (X != SCREEN_WIDTH - pix_x) {
            move_box(X,Y,3);
        }
        break;

    case SDLK_4:
        printf("4 is pressed\n");

        if (X != 0) {
            move_box(X,Y,4);
        }
        break;

    case SDLK_5:
        printf("5 is pressed\n");
        if (Y != SCREEN_HEIGHT - pix_y) {
            move_box(X,Y,5);
        }
        break;

    case SDLK_6:
        printf("6 is pressed\n");
        if (Y != 0) {
            move_box(X,Y,6);
        }
        break;

    default:
        printf("Can't find this key\n");
        break;
    }
}

int main() {

    if( !init() ) {
        printf( "Failed to initialize SDL!\n" );

    } else if( !create() ) {
        printf( "Failed to initialize window!\n" );

    } else if( !surface_create() ) {
        printf( "Failed to initialize surface!\n" );

    } else {

        SDL_LockSurface(surface);
        srand(time(NULL));
        X = rand() % 500;

        show_box(X, Y, pix_x, pix_y, 255, 255, 255);
        SDL_UnlockSurface(surface);
        SDL_UpdateWindowSurface(gWindow);

        /* создаем идентификатор */
        srand(time(NULL));
        identificator = rand() % 500;

        /* создаем мьютекс */
        mutex = PTHREAD_MUTEX_INITIALIZER;

        /* создаем сокет */
        udp_init();
        printf("инициализация udp прошла успешно\n");

        
          while (256 != event.type) {
              SDL_WaitEventTimeout(& event, 100);
        
              switch (event.type) {
              case SDL_MOUSEMOTION:
                  break;
        
              case SDL_KEYDOWN:
                  Handle_Keydown(&event.key.keysym);
                  break;
        
              case SDL_WINDOWEVENT:
                  break;
        
              default:
                  printf("Handle_Keydown ERR: Unknown type of keysym \n");
              }
        }
        
    }
}
