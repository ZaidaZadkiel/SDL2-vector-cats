#ifndef FONT_INCLUDED
#define FONT_INCLUDED

#include "SDL2/SDL.h"

class font {

    #define TEXT_WIDTH   75
    #define TEXT_HEIGHT  10
    #define FLAG_CHANGED 1

    public:

    SDL_Renderer *renderer;

    unsigned char textbuffer[TEXT_WIDTH*TEXT_HEIGHT];
    unsigned char flagbuffer[TEXT_WIDTH*TEXT_HEIGHT];

    // buffer for the printf so as to avoid redrawing the same text
             char printbuffer[TEXT_WIDTH];

    SDL_Texture *textfont;
    unsigned int w;
    unsigned int h;
    unsigned int glyph_w;
    unsigned int glyph_h;

    char *composition;
    Sint32 cursor = 0;
    Sint32 selection_len = 0;
    unsigned char cursor_x=0;
    unsigned char cursor_y=0;

    SDL_Texture *textarea;

    // getters
    int text_height(){
        return TEXT_HEIGHT;
    }
    int text_width(){
        return TEXT_WIDTH;
    }

    int chcur(unsigned char c){
      // assumes cursor is updated before call
      if(textbuffer[cursor] != c){
        textbuffer[cursor] = c;
        flagbuffer[cursor] = FLAG_CHANGED;
        return 1;
      }
      return 0;
    }

    int chindex(unsigned char c, int index){
      // uses index without modifying cursor position
      if(textbuffer[index] != c){
        textbuffer[index] = c;
        flagbuffer[index] = FLAG_CHANGED;
        return 1;
      }
      return 0;
    }

    int process_string(unsigned char *str){
        printf("%s\n", str);
        return 0;
    }

    /* units are in text characters inside the buffer NOT pixel! */
    void drawbox(SDL_Rect *r){
      int x, y, w, h;
      x = r->x;
      y = r->y;
      w = r->w;
      h = r->h;

      //corners
      text_chxy(6, x,   y);
      text_chxy(5, x+w, y);
      text_chxy(3, x,   y+h);
      text_chxy(4, x+w, y+h);

      // horizontal line, top and bottom
      for(x = r->x + w-1; x != r->x; x--){
        text_chxy(2, x,y);
        text_chxy(2, x,y+h);
      }

      // vertical line, left and right
      x = r->x;
      for(y = r->y + h-1; y != r->y; y--){
        text_chxy(1, x, y);
        text_chxy(1, x+w, y);
      }

    }// void drawbox(...)

// TODO: add some intermediate buffer to hold the formatted string and pass char by char parsing it (\n should d newline instead of printable)
    int printf(const char *fmt, ...){
      va_list arg_ptr;
      int changed = 0;
      int res;

      //recalc cursor
      cursor = cursor_y*TEXT_WIDTH+cursor_x;

      memset(printbuffer, 0, TEXT_WIDTH);
      va_start(arg_ptr, fmt);
      changed = vsnprintf(&printbuffer[0], TEXT_WIDTH, fmt, arg_ptr);
      va_end(arg_ptr);

      //(for(;changed; changed--) {
      int c = 0;
      for(res = 0; res != changed; res++){
        c = (cursor+res);
        this->chindex(printbuffer[res], c);
      }//for
      return res;
    }// font::printf(...)

    //progress and max are in total value, width is in characters
    int drawbar(int progress, int max, int width){

      // get the textbuffer position
      cursor = cursor_y*TEXT_WIDTH+cursor_x;

      // cut width if exceeds TEXT_WIDTH
      if(cursor_x+width > TEXT_WIDTH) width = TEXT_WIDTH-cursor_x;

      //value is in chars
      int value=0;
      if(progress > 0 && max > 0)  value = (double)progress/max * width; // normal case, value is % of progress
      if(value >= width)           value = width;                        // % value exceeds width, clip to width
      if(progress < 0)             value = 0;                            // if progress is negative, value is 0

      // do for each character in cursor_x..width
      int counter = width-1;
      char c;

      //empty bar
      while(counter-- ){
        if(counter < value){
          this->chindex(16, 1+cursor+counter);
        }else {
          this->chindex(13, 1+cursor+counter);
        }
      }

      // filled bar
      while(value-- > 1){
        //this->chindex(16, cursor+value);
      }//while

      //start of bar character
      if(value==0) {
        // empty horizontal start
        this->chindex(14, cursor);
      }else{
        // filled horizontal start
        this->chindex(11, cursor);
      }

      if(progress >= max){
        // filled horizontal end
        this->chindex(15, cursor+width);
      }else{
        // empty horizontal end
        this->chindex(12, cursor+width);
      }
      return 0;
    } //font::drawbar()



    /* x, y are in textbuffer coordz (i.e. char instead of px) */
    int text_chxy(char c, int x, int y){
      cursor_x = (x <= TEXT_WIDTH  ? (x >= 0 ? x : 0) : TEXT_WIDTH);
      cursor_y = (y <= TEXT_HEIGHT ? (y >= 0 ? y : 0) : TEXT_HEIGHT);
      cursor   = cursor_y * TEXT_WIDTH + cursor_x;

      if(cursor > TEXT_HEIGHT*TEXT_WIDTH || cursor < 0) return -1;
      this->chcur(c);
      return 0;
    } //font::text_chxy(...)



    // parse character and print result
    int text_putch(SDL_Keycode c){
        //printf("key %d\n", c);
        switch(c){
            case SDLK_RETURN: {
                process_string(&textbuffer[cursor_y * TEXT_WIDTH]); //full buffer line at cursor position (?)
                c = 0;
                cursor_x = 0;
                cursor_y++;
                break;
            }//sdlk_return
            case SDLK_BACKSPACE:
                c = 0;
                if(cursor_x>=1){
                    cursor_x--;
                    cursor--; //should not go to the line above
                    textbuffer[cursor]=0;
                    flagbuffer[cursor]=FLAG_CHANGED;
                    flagbuffer[cursor+1]=FLAG_CHANGED;
                }
                break;
            case SDLK_LEFT:
                c = textbuffer[cursor-1];
                if(cursor_x>=1){
                    cursor_x--;
                    cursor--;
                }
                break;
            case SDLK_RIGHT:
                c = 0;
                if(cursor_x <= TEXT_WIDTH){
                    cursor_x++;
                    cursor++;
                }
                break;
            default:
                cursor_x++;
                break;
        }//switch

        if(cursor_x == TEXT_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }

        if((cursor_y >= TEXT_HEIGHT) || (cursor >= TEXT_HEIGHT*TEXT_WIDTH) ) {
            cursor_x = 0;
            cursor_y = TEXT_HEIGHT-1;
            memcpy((char*)&textbuffer[0], (char*)&textbuffer[TEXT_WIDTH], TEXT_WIDTH*(TEXT_HEIGHT));
            for(int i = cursor_y*TEXT_WIDTH; i != TEXT_WIDTH*TEXT_HEIGHT; i++){
                textbuffer[i]      = 0;
            }
            memset(flagbuffer, FLAG_CHANGED, TEXT_HEIGHT*TEXT_WIDTH);
        }

        flagbuffer[cursor] = FLAG_CHANGED;
        textbuffer[cursor] = c;
        cursor             = cursor_y * TEXT_WIDTH + cursor_x;

        return cursor;
    } // font::text_putch(...)



    void render(){
        int texWidth  = 0;
        int texHeight = 0;

        SDL_GetRendererOutputSize(renderer, &texWidth, &texHeight);

        //Render to textarea texture to draw the text.
        SDL_SetRenderTarget(renderer, textarea);

        SDL_Rect rect = {0,0,8,8};
        SDL_Rect dst  = {0,0,8,8};

        rect.x = cursor_x*8;
        rect.y = cursor_y*8;
        rect.w = 8;

        //SDL_SetRenderDrawColor( renderer , 0xff, 0x00, 0x00, 0xff );
        //SDL_RenderFillRect(renderer, &rect );

        int chrcount;
        chrcount  = 0;
        rect.x = 0;
        rect.w = 8;
        rect.h = 8;

        for(int i=0; i!=TEXT_WIDTH*TEXT_HEIGHT; i++){
            rect.y = textbuffer[i] * 8;
            dst.x  = (i*8) % texWidth;
            dst.y  =  8*(i/TEXT_WIDTH);

            if(flagbuffer[i] & FLAG_CHANGED) {
                chrcount ++;
                flagbuffer[i] = 0;
                SDL_RenderCopy(  renderer, this->textfont, &rect, &dst);
            }
        }

        this->cursor_x = 1;
        this->cursor_y = 5;
        this->printf("printed %4d chars   ", chrcount);
    }

    font(){
        //empty init
    }

    font(SDL_Renderer *renderer){

        // save pointer referece
        this->renderer = renderer;

        //zero out text buffer and fla buffer
        memset(flagbuffer, 0, TEXT_HEIGHT*TEXT_WIDTH);
        memset(textbuffer, 0, TEXT_HEIGHT*TEXT_WIDTH);

        //load image from disk
        SDL_Surface *s = IMG_Load("res/text.png");
        textfont       = SDL_CreateTextureFromSurface(renderer, s);
        SDL_FreeSurface(s);

        //create text console renderable target
        Uint32 fmt;
        int access, tw, th;
        SDL_QueryTexture(textfont, &fmt, &access, &tw, &th);
        textarea = SDL_CreateTexture(
            renderer,
            fmt,
            SDL_TEXTUREACCESS_TARGET,
            TEXT_WIDTH * 8, TEXT_HEIGHT * 8
        ); //SDL_CreateTexture

    }; //font::font()

}
;

#endif // FONT_INCLUDED
