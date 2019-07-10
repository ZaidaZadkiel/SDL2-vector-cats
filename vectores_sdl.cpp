/*
 * TODO: sep 2016 encontrar como implementar a listy5 en esta cosa
 * TODO: Jun 2019 increase play area through viewports with proper scrolling
 *
 * $ g++ vectores_sdl.cpp -lSDL2 -lSDL2_image -std=c++11  -o vectores.bin
*/


#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_render.h>
#include <math.h>
//#include <iostream>
//#include <vector>
//#include <string>
//#include <unistd.h>

#include "font.h"


using namespace std;

#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))

#define MAX_VECTOR   20  // maximum vectorz in screen
#define FLOAT_ROUND  0.5f // round threshold for float
#define SPEED        5.0f // maximum speed a vector can go
#define SPEED_DELTA  0.09f

#define V_FLAG_DEAD  4
#define V_FLAG_DYING 2  //explosion loop
#define V_FLAG_ALIVE 1  //normal loop

//these could be bit flags but whatever
#define CONTROL_UP    0
#define CONTROL_DOWN  1
#define CONTROL_LEFT  2
#define CONTROL_RIGHT 3
#define CONTROL_BUT1  4
#define CONTROL_BUT2  5

char controlkeys[8]; //joystick buttons on / off
int  focus_text = 1; //0 == focus is on txt area, 1 == focus is on graph area

/* la estructura vec mantiene los datos del vector
 * float x y son la posicion actual en el frame
 * float d_x d_y son la posicion *delta* es decir, la variacion entre frame y frame
     por cada frame delta se le agrega a la posicion del vector.
 * c es el color en RGBA (rojo verde azul alpha (transparencia) 1 byte cada 1 = 4 byte = 32 bit
 * dr, dg, db es un delta para el cambio del color, falta de implementar

 * for(int i =0; i!= (200); i++){
 * struct vec está iniciada a 200 elementos como arreglo simple, plano, sin chiste y aburrido. NADA de magia.
 */
struct vec{
    float  x,   y;
    float  d_x, d_y;
    Uint32 c;
    signed char dr, dg, db;
}        vectores  [MAX_VECTOR];
SDL_Rect collisions[MAX_VECTOR];
Uint8    flags     [MAX_VECTOR];

/* floor_coord es la posicion del "piso" de donde rebotan en Y osea, lo que se supone jamás deben pasar */
float floor_coord= 500;

struct wall{
  vec w;
  vec v;
}             walls[4];

/*** SDL Stuff ***/
font        *fontstyle = NULL;

Uint8        fullscreen = false; // toggle fullscreen at startup only
SDL_Window  *window;             // WM window data

Uint32       rmask,
             gmask,   //masks
             bmask,
             amask;

int          screenWidth;        // actual screen size
int          screenHeight;       // actual screen size

const int    texWidth  = 600;    // texture pixel size
const int    texHeight = 600;    // texture pixel size

SDL_Surface   *s;                // drawing pixel data
unsigned char *pixels;           // buffer pointer



float sqr(float n) { return n * n; }


//Warning: Witchcraft
// Returns 1 if the lines intersect, otherwise 0. In addition, if the lines
// intersect the intersection point may be stored in the floats i_x and i_y.
char get_line_intersection(float p0_x, float p0_y, float p1_x, float p1_y,
    float p2_x, float p2_y, float p3_x, float p3_y, float *i_x, float *i_y)
{
    float s02_x, s02_y, s10_x, s10_y, s32_x, s32_y, s_numer, t_numer, denom, t;
    s10_x = p1_x - p0_x;
    s10_y = p1_y - p0_y;
    s32_x = p3_x - p2_x;
    s32_y = p3_y - p2_y;

    denom = s10_x * s32_y - s32_x * s10_y;
    if (denom == 0)
        return 0; // Collinear
    bool denomPositive = denom > 0;

    s02_x = p0_x - p2_x;
    s02_y = p0_y - p2_y;
    s_numer = s10_x * s02_y - s10_y * s02_x;
    if ((s_numer < 0) == denomPositive)
        return 0; // No collision

    t_numer = s32_x * s02_y - s32_y * s02_x;
    if ((t_numer < 0) == denomPositive)
        return 0; // No collision

    if (((s_numer > denom) == denomPositive) || ((t_numer > denom) == denomPositive))
        return 0; // No collision
    // Collision detected
    t = t_numer / denom;
    if (i_x != NULL)
        *i_x = p0_x + (t * s10_x);
    if (i_y != NULL)
        *i_y = p0_y + (t * s10_y);

    return 1;
} /* get_line_intersection(...) */


char bounce_wall(vec *v, wall w){
  float ix, iy;
  char n = get_line_intersection(
            w.w.x,       w.w.y,
            w.v.x,       w.v.y,
            v->x,        v->y,
            v->x-v->d_x, v->y-v->d_y,
            (&ix),       (&iy)
            );

  if(n == 1) {
    v->d_x = -v->d_x;
    v->d_y = -v->d_y;
    v->x   = ix + v->d_x;
    v->y   = iy + v->d_y;
  } /* if (n == 1) */
} /* bounce_wall (vec, wall) */

//        unsigned char* lockedPixels;
//        int pitch;
//        SDL_LockTexture
//            (
//            texture,
//            NULL,
//            reinterpret_cast< void** >( &lockedPixels ),
//            &pitch
//            );
//        std::copy( pixels.begin(), pixels.end(), lockedPixels );
//        SDL_UnlockTexture( texture );
//
//        SDL_UpdateTexture(
//            texture,
//            NULL,
//            &pixels[0],
//            texWidth * 4
//        );

void putpixel(int x, int y, Uint32 c){
    //char* pixels = (char*)s->pixels;

    unsigned char *color = ((unsigned char *)(&c));
    const unsigned int offset = ( texWidth * 4 * y ) + x * 4;
    if(offset <=0) return;
    if(offset >= 4*(texWidth*texHeight)) return;
    pixels[ offset + 0 ] = color[0];    // b
    pixels[ offset + 1 ] = color[1];    // g
    pixels[ offset + 2 ] = color[2];    // r
    pixels[ offset + 3 ] = color[3];    // a
}


int updateScreenBounds(){
  if(fullscreen == true){
    SDL_Rect bounds;
    SDL_GetDisplayBounds(0, &bounds);
    screenHeight = bounds.h;
    screenWidth  = bounds.w;
  }else{
    SDL_GetWindowSize(window, &screenHeight, &screenWidth);
  }

  return 0;
}

void update_vec(vec *v){
  // trim deltas
  if (fabsf(v->d_x) > SPEED) v->d_x = SPEED * signbit(v->d_x);
  if (fabsf(v->d_y) > SPEED) v->d_y = SPEED * signbit(v->d_y);
  v->x += v->d_x;
  v->y += v->d_y;
}

int main( int argc, char** argv )
{

    /* iniciacion de la libreria */
    SDL_Init( SDL_INIT_EVERYTHING );
    atexit( SDL_Quit );

    SDL_version sdlversion;
    SDL_GetVersion(&sdlversion);

    printf("SDL_version: %d.%d.%d\n", sdlversion.major, sdlversion.minor, sdlversion.patch);
    /* ventana de 600 px * 600 px */
    window = SDL_CreateWindow(
        "SDL2",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        600, 600,
        SDL_WINDOW_SHOWN
    );

    if(fullscreen == true) SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);

    updateScreenBounds();
    /* la estructura del renderizado */
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE
    );

    SDL_Surface *file;
    file = IMG_Load("res/gato.png");
    SDL_Texture *gato = SDL_CreateTextureFromSurface(renderer, file);
    SDL_FreeSurface(file);

    file = IMG_Load("res/background.png");
    SDL_Texture *background = SDL_CreateTextureFromSurface(renderer, file);
    SDL_FreeSurface(file);

    file = IMG_Load("res/player.png");
    SDL_Texture *player = SDL_CreateTextureFromSurface(renderer, file);
    SDL_FreeSurface(file);

    file = IMG_Load("res/victory.png");
    SDL_Texture *victory = SDL_CreateTextureFromSurface(renderer, file);
    SDL_FreeSurface(file);
    SDL_Rect rvictory;
    rvictory.x = 0;
    rvictory.y = 220;
    rvictory.h = 160;
    rvictory.w = 600;

    /* la fuente de pixeles (lo que parecia tubería */
    fontstyle = new font(renderer);

    /* aqui se dibujan los vectores */
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET,
        texWidth, texHeight
    );
    SDL_Rect texRect;
    texRect.x = 0;
    texRect.y = 0;
    texRect.h = texHeight;
    texRect.w = texWidth;

    SDL_Rect gatorect;
    gatorect.x = 0;
    gatorect.y = 0;
    gatorect.w = 16;
    gatorect.h = 16;


    /* SDL interprets each pixel as a 32-bit number, so our masks must depend
       on the endianness (byte order) of the machine */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    s = SDL_CreateRGBSurface(0, 1024, 1024, 32, rmask, gmask, bmask, amask);
    if(s == NULL) {
        fprintf(stderr, "CreateRGBSurface failed: %s\n", SDL_GetError());
        exit(1);
    }

    pixels = (unsigned char*)malloc ( texWidth * texHeight * 4  );

    SDL_Event event;
    bool running = true;

    vectores[0].c = (Uint32)-1;
    vectores[0].x = 300;
    vectores[0].y = 300;
    flags   [0]   = V_FLAG_ALIVE;

/******************* INITALIZE LOOP *******************/
    for(int i = 1; i!= MAX_VECTOR; i++){
        vec *v = &vectores[i];

        v->x   = (float)(rand() % texWidth);
        v->y   = (float)(rand() % round(floor_coord));

        // delta -5.0f .. 0 .. +5.0f
        v->d_x = 0;//(SPEED) * ((float)rand() / (float)RAND_MAX) - (SPEED*0.5) ;
        v->d_y = 0;//(SPEED) * ((float)rand() / (float)RAND_MAX) - (SPEED*0.5) ;

        // bright colorz
        unsigned char *color = ((unsigned char*) (&v->c));
        color[0] = (0x30 + rand()) % 0xff;
        color[1] = (0x30 + rand()) % 0xff;
        color[2] = (0x30 + rand()) % 0xff;
        color[3] = SDL_ALPHA_OPAQUE;


        // 0...RAND_MAX >> 8 = 0xffffffff > 0x0fffffff % 3 = 011b - 1 = 3 - 1 = 2
        v->dr = (rand() % 3) - 1;
        v->dg = (rand() % 3) - 1;
        v->db = (rand() % 3) - 1;

        flags[i] = V_FLAG_ALIVE; //set all vectors as alive
    }

    walls[0].w.x = 100;
    walls[0].w.y = 100;
    walls[0].v.x = 200;
    walls[0].v.y = 400;

    walls[1].w.x = 500;
    walls[1].w.y = 150;
    walls[1].v.x = 550;
    walls[1].v.y = 350;


    SDL_Keycode k;

    long     framespersecond = 0;
    Uint64   seconds         = 0;

    int      killed      = 0;
    int      combo       = 0;
    int      combomax    = 0;
    Uint64   combo_start = 0;
    Uint64   combo_count = 0;
    Uint64   time        = SDL_GetTicks();

    SDL_Rect fat   = {0,0,16,16};  //the vector head, a fat pixel
    Uint64   start = 0;
    Uint64   hit   = 0;


    Uint16 winc=0x00;

/******************* MAIN LOOP *******************/
    while( running )
    {
        framespersecond++;
        time = SDL_GetTicks();

        //SDL_SetRenderDrawColor( renderer, 0, 0, 0, 128 );
        //SDL_RenderClear( renderer );


/******************* INPUT *******************/
        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        while( SDL_PollEvent( &event ) )
        {
/******************* SDL POLLING  *******************/
            switch(event.type){
              case SDL_QUIT:
                running = false;
                break;
              case SDL_KEYDOWN: {
                if(SDL_SCANCODE_ESCAPE == event.key.keysym.scancode ) {
                    running = false;
                    break;
                }

                k = event.key.keysym.sym;

                if(focus_text == 0){ // focus is in txt area
                  switch(k){
                      case SDLK_RETURN:
                      case SDLK_LEFT:
                      case SDLK_RIGHT:
                      case SDLK_BACKSPACE:
                          fontstyle->text_putch(event.key.keysym.sym);
                          break;
                      case SDLK_TAB:
                          focus_text = 1;
                          break;
                  }//switch keysym

                } else {             // focus is in the game area

                   //focus is on graph area
                  //vec *v = &vectores[0];
                  if(keys[SDL_SCANCODE_TAB])   focus_text                 = 0;
                  if(keys[SDL_SCANCODE_UP])    controlkeys[CONTROL_UP]    = 1;
                  if(keys[SDL_SCANCODE_DOWN])  controlkeys[CONTROL_DOWN]  = 1;
                  if(keys[SDL_SCANCODE_LEFT])  controlkeys[CONTROL_LEFT]  = 1;
                  if(keys[SDL_SCANCODE_RIGHT]) controlkeys[CONTROL_RIGHT] = 1;
                  if(keys[SDL_SCANCODE_SPACE]) controlkeys[CONTROL_BUT1]  = 1;
                } // else
                //printf("focus is %d\n", focus_text);
              }
              break; // case SDL_KEYDOWN
              case SDL_KEYUP:
                if(focus_text == 1){ //focus is on graph area
                  //if(keys[SDL_SCANCODE_TAB])   focus_text                 = 0;
                  if(keys[SDL_SCANCODE_UP]    == 0) controlkeys[CONTROL_UP]    = 0;
                  if(keys[SDL_SCANCODE_DOWN]  == 0) controlkeys[CONTROL_DOWN]  = 0;
                  if(keys[SDL_SCANCODE_LEFT]  == 0) controlkeys[CONTROL_LEFT]  = 0;
                  if(keys[SDL_SCANCODE_RIGHT] == 0) controlkeys[CONTROL_RIGHT] = 0;
                  if(keys[SDL_SCANCODE_SPACE] == 0) controlkeys[CONTROL_BUT1]  = 0;
                }
              case SDL_TEXTINPUT:
                if(focus_text == 0){
                  for(unsigned int i = 0; i != strlen(event.text.text); i++){
                      fontstyle->text_putch(event.text.text[i]);
                  }//for
                }//if focus_text == 0
                break; //SDL_TEXTINPUT
            } //switch event.type
        } // while events
/******************* SDL POLLING  *******************/
/******************* PLAYER *******************/

        vec *v = &vectores[0];

        // update deltas for player
        if(controlkeys[CONTROL_UP   ]){
                         v->d_y -= (v->d_y > -(SPEED) ? SPEED_DELTA : 0.0f);
        }else{
          if(v->d_y < 0) v->d_y += SPEED_DELTA;
        }
        if(controlkeys[CONTROL_DOWN ]){
                         v->d_y += (v->d_y <  SPEED ? SPEED_DELTA : 0.0f);
        }else{
          if(v->d_y > 0) v->d_y -= SPEED_DELTA;
        }
        if(controlkeys[CONTROL_LEFT ]){
                         v->d_x -= (v->d_x > -(SPEED) ? SPEED_DELTA : 0.0f);
        }else{
          if(v->d_x < 0) v->d_x += SPEED_DELTA;
        }
        if(controlkeys[CONTROL_RIGHT]){
                         v->d_x += (v->d_x <  SPEED ? SPEED_DELTA : 0.0f);
        }else{
          if(v->d_x > 0) v->d_x -= SPEED_DELTA;
        }
        if(controlkeys[CONTROL_BUT1 ]) {

        }

        /* prevent slippage of float (d_x <= 0.5) */
        if(v->d_x < SPEED_DELTA && v->d_x > -SPEED_DELTA) v->d_x = 0;
        if(v->d_y < SPEED_DELTA && v->d_y > -SPEED_DELTA) v->d_y = 0;

/******************* PLAYER *******************/
/******************* WORLD  *******************/

        // texture is the play area for the vectors
        SDL_SetRenderTarget(renderer, texture);

        /* set background color in function of the time the last vector was hit */
        unsigned char color[2];
        // draw background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_RenderCopy(renderer,background,  NULL ,NULL);

        //SDL_SetRenderDrawColor( renderer , color[0]&0x3f, color[1]&0x3f, color[2]&0x3f, 0xff);
        //SDL_SetRenderDrawColor( renderer , combo*4,combo*4,combo*4, 25);
        //SDL_RenderClear(renderer);
        //SDL_RenderFillRect(renderer, &texRect);

        if(hit != 0){
          //draw bg fx
          SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
          color[0] = (unsigned char)hit;
          color[1] = (unsigned char)hit;
          color[2] = (unsigned char)hit;
          hit = hit - 2;

          SDL_SetRenderDrawColor( renderer , color[0], color[1], color[2], 0xff-(collisions[0].w*5));
          SDL_RenderFillRect(renderer, &texRect);
          SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        //draw the floor line in bright red
        SDL_SetRenderDrawColor( renderer , 0xff, 0x00, 0x00, 0xff);
        SDL_RenderDrawLine(renderer,
            0,        round(floor_coord),
            texWidth, round(floor_coord)
        );

        // scenery colour
        int c = (combo_count>>2) & 0xff;
        //if(d < 100) c = 0xff;
        SDL_SetRenderDrawColor(renderer, 0xff,c,c, SDL_ALPHA_OPAQUE);

        // draw wallz
        for(unsigned int i = 0; i != 4; i++){
          SDL_RenderDrawLine(renderer,
              walls[i].w.x, walls[i].w.y,
              walls[i].v.x, walls[i].v.y
          );
        }

/********************** WORLD **********************/
/******************* VECTOR LOOP *******************/

        vec     *va = &vectores[0];

        float vx = v->x;
        float vy = v->y;
        float dx = v->d_x;
        float dy = v->d_y;
        float nx, ny;

        int rvx;
        int rvy;
        int isalive = 0;


        if(killed >= MAX_VECTOR-1){
          
          SDL_SetTextureBlendMode(victory,SDL_BLENDMODE_BLEND);
          SDL_SetTextureAlphaMod(victory,winc);
          SDL_RenderCopy(renderer,victory,NULL,&rvictory);
          if(winc != 0xff) winc++;
        } else {

          for( unsigned int i = 0; i < MAX_VECTOR; i++ ){

              isalive = flags[i];
              if(isalive & V_FLAG_DEAD) continue;

              v  = &vectores[i];
              vx = v->x;   //local copy to avoid array-pointer-struct lookupz
              vy = v->y;
              //dx = v->d_x;
              //dy = v->d_y;
              //(SPEED) * ((float)rand() / (float)RAND_MAX) - (SPEED*0.5) ;

              /*randomize direction*/
              if(i > 0){
                v->d_x += (2*SPEED_DELTA) * ((float)rand() / (float)RAND_MAX) - SPEED_DELTA;
                v->d_y += (2*SPEED_DELTA) * ((float)rand() / (float)RAND_MAX) - SPEED_DELTA;
              }

              dx = v->d_x ;//+  ((float)rand() / (float)RAND_MAX)  ;
              dy = v->d_y ;//+  ((float)rand() / (float)RAND_MAX)  ;
              nx = vx + dx;
              ny = vy + dy;

              //if flagged as alive, do the boundary check
              if(isalive & V_FLAG_ALIVE){

                //bounce on walls: dx *= <1 is for damping, >1 for springing*/
                if(nx <= 0           && dx < 0) dx *= -0.9;
                if(nx >= texWidth    && dx > 0) dx *= -0.9;
                //bounce on floor no limit for sky and going down
                if(ny >= floor_coord && dy > 0) dy *= -0.9;
                //bounce on ceiling
                if(ny <= 0           && dy < 0) dy *= -0.9;
                //gravity (only if above the floor)
                //if(v->y < floor_coord)                         v->d_y += 0.040f;

                //if (fabsf(dx) > SPEED) dx = SPEED * signbit(dx);
                //if (fabsf(dy) > SPEED) dy = SPEED * signbit(dy);

                vx   += dx;
                vy   += dy;

                rvx   = round(vx);
                rvy   = round(vy);

                fat.x = rvx-8;
                fat.y = rvy-8;
              } // if isalive == V_FLAG_ALIVE


              if(isalive & (V_FLAG_ALIVE | V_FLAG_DYING )){
                color[0] = ((unsigned char*) &(v->c))[0];
                color[1] = ((unsigned char*) &(v->c))[1];
                color[2] = ((unsigned char*) &(v->c))[2];

                if(color[0] == 0 || color[0] == 255) v->dr *= -1;
                if(color[1] == 0 || color[1] == 255) v->dg *= -1;
                if(color[2] == 0 || color[2] == 255) v->db *= -1;

                color[0] += v->dr;
                color[1] += v->dg;
                color[2] += v->db;
              }

              // draw an image in the vector pos with a coloured bg
              SDL_SetRenderDrawColor( renderer , color[0]*2, color[1]*2, color[2]*2, 0xff);
              if(i == 0 ) {
                SDL_RenderCopy(renderer,player, &gatorect ,&fat);
              } else {
                SDL_RenderCopy(renderer,gato,   &gatorect ,&fat);
              }

              // check for collision of this vector and player
              if( i > 0                         &&   // i == 0 is player, ignore it
                  flags[i] |  V_FLAG_ALIVE      &&   // this vector has not been hit
                  abs(round(vx - va->x)) < 10   &&   // distance from other vectors is square of -5..5 in x and y
                  abs(round(vy - va->y)) < 10
              ) {
                killed++;
                if(combo_count-combo_start < 1500) combo++;
                combo_start = SDL_GetTicks();

                //set explosion rect
                collisions[i].x = vx;
                collisions[i].y = vy;
                collisions[i].w = 0;
                collisions[i].h = 0;

                //put this vector somewhere
                vx = -100;
                vy = i * 4;
                dx = 0.0f;
                dy = 0.0f;

                // this vector will do the dying animation
                flags[i] = V_FLAG_DYING;
                hit = 254;
              } /* if vector killed */


              //do the explosion loop
              if(
                  i > 0 &&
                  isalive == V_FLAG_DYING
              ) {
                collisions[i].x -= collisions[i].w & 1; //= v->x - (collisions[i].w / 2);
                collisions[i].y -= collisions[i].h & 1; //= v->y - (collisions[i].h / 2);
                collisions[i].w++;
                collisions[i].h++;

                if(collisions[i].w >= (0xff/5)) flags[i] = V_FLAG_DEAD;
                //SDL_SetRenderDrawColor( renderer , color[0], color[1], color[2], 0xff-(collisions[i].w*5));
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor( renderer , color[0], color[1], color[2], 0xff-(collisions[i].w*5));
                SDL_RenderFillRect(renderer, &collisions[i]);

              }//if

              v->x   = vx;
              v->y   = vy;
              v->d_x = (dx > SPEED ? SPEED : (dx < -SPEED ? -SPEED : dx));
              v->d_y = (dy > SPEED ? SPEED : (dy < -SPEED ? -SPEED : dy));

              bounce_wall(v, walls[0]);
              bounce_wall(v, walls[1]);

              //fontstyle->cursor_y = i;
              //fontstyle->printf("%03d (%3d,%3d) %2x:%2d %2x:%2d %2x:%2d  ",i, (int)v->x, (int)v->y, color[0], v->dr, color[1], v->dg, color[2], v->db );
              //printf("%03d: %3d:%2d %3d:%2d %3d:%2d\n",i,  color[0], v->dr, color[1], v->dg, color[2], v->db );
          }
        } /* if (killed >= MAX_VECTOR-1) */
/******************* VECTOR LOOP *******************/
/************************ UI ***********************/

        combo_count = SDL_GetTicks();

        if(combomax < combo) combomax = combo;
        if(combo_count - combo_start > 1500){
          combo = 0;
        }

        SDL_Rect box = {0,0,TEXT_WIDTH-1,TEXT_HEIGHT-1};
        fontstyle->drawbox(&box);
        fontstyle->cursor_x=1;
        fontstyle->cursor_y=1;
        fontstyle->printf("Vectores: %4d / %4d", killed, MAX_VECTOR-1); // MAX_VECTOR-1 bcz player is vector 0
        fontstyle->cursor_y=2;
        fontstyle->drawbar(MAX_VECTOR - killed, MAX_VECTOR, TEXT_WIDTH-23);

        fontstyle->cursor_y=4;
        fontstyle->printf("Player: %2x %s", flags[0], "Cloud Strife");

        box = {TEXT_WIDTH-21,0,20,TEXT_HEIGHT-1};
        fontstyle->drawbox(&box);
        fontstyle->cursor_x=TEXT_WIDTH-20;
        fontstyle->cursor_y=1;
        fontstyle->printf("    Time  %6.2f", (float)time/1000  );

        fontstyle->cursor_x=TEXT_WIDTH-20;
        fontstyle->cursor_y=2;
        fontstyle->printf("Hi-Score   %2d  ", combomax);
        fontstyle->cursor_x=TEXT_WIDTH-20;
        fontstyle->cursor_y=3;
        fontstyle->printf("   Combo   %2d  ", combo);

        fontstyle->cursor_x=TEXT_WIDTH-20;
        fontstyle->cursor_y=4;
        fontstyle->drawbar(1500-(combo_count-combo_start), 1500, 18);


        /*print debug info*/
        fontstyle->cursor_x = 0;
        fontstyle->cursor_y = 4;
        if(false) fontstyle->printf(" f%2d u%2d d%2d l%2d r%2d b1%2d b2%2d %04f %04f  ",
            focus_text,
            controlkeys[CONTROL_UP],
            controlkeys[CONTROL_DOWN],
            controlkeys[CONTROL_LEFT],
            controlkeys[CONTROL_RIGHT],
            controlkeys[CONTROL_BUT1],
            controlkeys[CONTROL_BUT2],
            v->d_x,
            v->d_y
        );

        fontstyle->render();

        //Restore the default render target
        SDL_SetRenderTarget(renderer, NULL);

        SDL_RenderCopy( renderer, texture, NULL, NULL );

        // show text area
        SDL_Rect textarearect    = {
          (int) ( screenWidth / 2 - (fontstyle->text_width()  * 8) / 2 ),
          (int) ( screenHeight    - (fontstyle->text_height() * 8)     ),
          TEXT_WIDTH*8,TEXT_HEIGHT*8
        };

        SDL_Rect textarearectsrc = {0,   0,TEXT_WIDTH*8,TEXT_HEIGHT*8};

        SDL_RenderCopy( renderer, fontstyle->textarea, &textarearectsrc,  &textarearect);

        SDL_RenderPresent( renderer );

/********************* UI *********************/



        const Uint64        end  = SDL_GetPerformanceCounter();
        const static Uint64 freq = SDL_GetPerformanceFrequency();

        seconds = (end - start) ;
        if( seconds >= 1000000000){

          fontstyle->cursor_x = 1;
          fontstyle->cursor_y = 8;
          fontstyle->printf("fps %3d ", framespersecond );
          framespersecond = 0;
          start = end ;
          //cout << "Frame time: " << seconds * 1000.0 << "ms" << endl;
        }
    } /* while (running)*/

    SDL_DestroyRenderer( renderer );
    SDL_DestroyWindow( window );
    SDL_Quit();
}
