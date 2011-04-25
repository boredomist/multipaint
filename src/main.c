#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <netdb.h>

#include "SDL/SDL.h"

#include <pthread.h>

#define CONNECT 1
#define DISCONNECT 2
#define MESSAGE 3
#define DRAW 4
#define ERASE 5

#define WIDTH 800
#define HEIGHT 600

#define WHITE -1

typedef enum COLORS {
  BLACK,
  GRAY,
  RED,
  GREEN,
  BLUE,
  YELLOW,
  FUCHSIA,
  CYAN,
  NUM_COLORS
} COLORS;

SDL_Surface *screen;

int sockfd;
struct sockaddr_in servaddr;
bool running;
char nick[256];

int currentColor = BLACK;
bool mouseLeftDown = false;
bool mouseRightDown = false;

void Handle(char*, int);
void SetPixel(int, int, int);
void SendDraw(short, short, short);
void SendErase(short, short);

void InitSDL() {
  SDL_Init(SDL_INIT_VIDEO);
  screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, SDL_SWSURFACE );
  SDL_WM_SetCaption("MULTIPAINTC", NULL );

  int x, y;
  for(x = 0; x < WIDTH; ++x) {
    for(y = 0; y < HEIGHT; ++y) {
      SetPixel(x, y, WHITE);
    }
  }
}

void UpdateSDL() {
  SDL_Event e;
  while(SDL_PollEvent(&e)) {
    if(e.type == SDL_QUIT ) {
      running = false;
      break;
    } else if(e.type == SDL_MOUSEMOTION && (mouseLeftDown || mouseRightDown)) {
      int x = e.motion.x;
      int y = e.motion.y;

      if(mouseLeftDown) {
        SetPixel(x, y, currentColor);
        SendDraw(x, y, currentColor);
      } else if(mouseRightDown) {
        SendErase(x, y);
      }

    } else if(e.type == SDL_MOUSEBUTTONDOWN) {
      if(e.button.button == SDL_BUTTON_WHEELDOWN) {
        currentColor = (currentColor + 1) % NUM_COLORS;
      } else if(e.button.button == SDL_BUTTON_WHEELUP) {
        currentColor = (--currentColor < 0) ? 0 : currentColor;
      } else {
        if(e.button.button == SDL_BUTTON_LEFT) {
          mouseLeftDown = true;
        } else if(e.button.button == SDL_BUTTON_RIGHT) {
          mouseRightDown = true;
        }
      }
    }  else if(e.type == SDL_MOUSEBUTTONUP) {
      if(e.button.button == SDL_BUTTON_LEFT) {
        mouseLeftDown = false;
      } else if(e.button.button == SDL_BUTTON_RIGHT) {
        mouseRightDown = false;
      }
    }
  }
}

void DrawSDL() {
  SDL_UpdateRect(screen, 0, 0, WIDTH, HEIGHT);
  SDL_Flip(screen);
}

void DeinitSDL() {
  SDL_FreeSurface(screen);
  SDL_Quit();
}

void UpdateConnection() {
  char buf[7000];
  int n;
  unsigned len = sizeof(servaddr);
  
  n = recvfrom(sockfd, buf, 7000, 0, (struct sockaddr *)&servaddr, &len);

  if(n <= 0) {
    return;
  }

  Handle(buf, n);
}

void SetPixel_(int x, int y, unsigned color) {
  int bpp = screen->format->BytesPerPixel;
  unsigned char *p = (unsigned char*)screen->pixels + y * screen->pitch + x * bpp; 
  
  *(unsigned*)p = color;
}

void SetPixel(int x, int y, int c) {

  if(x >= WIDTH || x < 0) {
    return;
  }
  if(y >= HEIGHT || y < 0) {
    return;
  }

  unsigned color;
  switch (c) {
  case WHITE: 
    color = SDL_MapRGB(screen->format, 255, 255, 255);
    break;
  case BLACK:
    color = SDL_MapRGB(screen->format, 0, 0, 0);
    break;
  case GRAY:
    color = SDL_MapRGB(screen->format, 128, 128, 128);
    break;
  case RED:
    color = SDL_MapRGB(screen->format, 255, 0, 0);
    break;
  case GREEN:
    color = SDL_MapRGB(screen->format, 0, 255, 0);
    break;
  case BLUE:
    color = SDL_MapRGB(screen->format, 0, 0, 255);
    break;
  case YELLOW:
    color = SDL_MapRGB(screen->format, 255, 255, 0);
    break;
  case FUCHSIA:
    color = SDL_MapRGB(screen->format, 255, 0, 255);
    break;
  case CYAN:
    color = SDL_MapRGB(screen->format, 0, 255, 255);
    break;
  }

  if(SDL_MUSTLOCK(screen)) {
    SDL_LockSurface(screen);
  }
  
  int x1 = x == WIDTH - 1 ? x : x + 1;
  int y1 = y == HEIGHT - 1 ? y : y + 1;

  SetPixel_(x, y, color);
  SetPixel_(x1, y, color);
  SetPixel_(x, y1, color);
  SetPixel_(x1, y1, color);

  if(SDL_MUSTLOCK(screen)) {
    SDL_UnlockSurface(screen);
  }
}

void SendErase(short x, short y) {
  char buf[6];
  unsigned len = sizeof(servaddr);
  buf[0] = '\x00';
  buf[1] = ERASE;
  short xtmp = htons(x);
  short ytmp = htons(y);

  char* xpos = (char*)&xtmp;
  char* ypos = (char*)&ytmp;

  buf[2] = xpos[0];
  buf[3] = xpos[1];

  buf[4] = ypos[0];
  buf[5] = ypos[1];

  sendto(sockfd, buf, 6, 0,
         (struct sockaddr *)&servaddr, len);
}

void SendDraw(short x, short y, short c) {
  char buf[8];
  unsigned len = sizeof(servaddr);
  buf[0] = '\x00';
  buf[1] = DRAW;
  short xtmp = htons(x);
  short ytmp = htons(y);
  short ctmp = htons(c);

  char* xpos = (char*)&xtmp;
  char* ypos = (char*)&ytmp;
  char* col  = (char*)&ctmp;

  buf[2] = xpos[0];
  buf[3] = xpos[1];

  buf[4] = ypos[0];
  buf[5] = ypos[1];

  buf[6] = col[0];
  buf[7] = col[1];

  sendto(sockfd, buf, 8, 0,
         (struct sockaddr *)&servaddr, len);
}

void Connect(char* host, char* name) {
  char buf[256];
  sockfd = socket(AF_INET,SOCK_DGRAM,0);
  
  memset(&servaddr, '\0', sizeof(servaddr));
  
  struct hostent* h = gethostbyname(host);
  if(h == NULL) {
    printf("Error resolving host!\n");
    exit(1);
  }

  memcpy(&servaddr.sin_addr, h->h_addr_list[0], h->h_length);
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(5303);
  
  unsigned len = sizeof(servaddr);
  
  char* s = malloc(512);
  s[0] = '\x00';
  s[1] = '\x00001';
  strcpy(s + 2, name);
  
  if(connect(sockfd, (const struct sockaddr*)&servaddr, len) < 0) {
    printf("Connect failed!\n");
    exit(1);
  }

  sendto(sockfd, s, strlen(name) + 2, 0,
         (struct sockaddr *)&servaddr, len);
  
  int n = recvfrom(sockfd, buf, 256, 0, (struct sockaddr *)&servaddr, &len);
  
  if(n < 0) {
    printf("Connect failed\n");
    exit(1);
  }

  buf[n] = 0;
  
  strcpy(nick, buf + 2);
  printf("Nick is => %s\n", nick);
  free(s);
}

void HandleConnect(char* buf, int len) {
  printf("%s connected\n", buf);
}

void HandleDisconnect(char* buf, int len) {
  printf("%s disconnected\n", buf);
}

void HandleMessage(char* buf, int len) {
  char* user = buf;
  char* msg = user + strlen(user) + 1;
  printf("%s> %s\n", user, msg);
}

void draw_erase_stub(char* buf, int len, bool draw) {
  char* user = buf;
  char* rest = buf + strlen(user) + 1;
  len -= strlen(user) + 1;

  while(len > 0) {
    char x_[2];
    char y_[2];
    char c_[2];
    
    /* reversed because converting from big endian to little endian */
    x_[1] = rest[0];
    x_[0] = rest[1];
    
    rest += 2;
    
    y_[1] = rest[0];
    y_[0] = rest[1];
    
    rest += 2;
    
    c_[1] = rest[0];
    c_[0] = rest[1];
    
    rest += 2;

    unsigned short x, y, c;
    
    /* resorting to witchcraft */
    x = *(unsigned short*)&x_;
    y = *(unsigned short*)&y_;
    c = *(unsigned short*)&c_;
    
    SetPixel(x, y, draw ? c : WHITE);

    len -= 6;

  }
}

void HandleDraw(char* buf, int len) {
  draw_erase_stub(buf, len, true);
}

void HandleErase(char* buf, int len) {
  draw_erase_stub(buf, len, false);
}

void Handle(char* buf, int len) {
  if(len < 2) {
    return;
  }

  char c = buf[1];
  buf = buf + 2;
  len = len - 2;
  switch(c) {
  case CONNECT:
    HandleConnect(buf, len);
    break;
  case DISCONNECT:
    HandleDisconnect(buf, len);
    break;
  case MESSAGE:
    HandleMessage(buf, len);
    break;
  case DRAW:
    HandleDraw(buf, len);
    break;
  case ERASE:
    HandleErase(buf, len);
    break;
  }

}

void* net_thread(void* nothing) {
  while(running) {
    UpdateConnection();
  }
  
  return NULL;
}

int main(int argc, char**argv)
{
   if (argc != 3) {
     printf("usage:  multipaintc [IP] [NICK]");
     exit(1);
   }
   
   InitSDL();

   Connect(argv[1], argv[2]);

   running = true;

   pthread_t thread;
   pthread_create(&thread, NULL, net_thread, NULL);

   unsigned ticks = SDL_GetTicks();

   while(running) {
     ticks = SDL_GetTicks();
     
     UpdateSDL();
     DrawSDL();

     SDL_Delay(1);
   }

   close(sockfd);

   DeinitSDL();
}
