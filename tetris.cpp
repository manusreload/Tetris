/* 
 * File:   main.cpp
 * Author: Manus
 *
 * Created on 10 de octubre de 2013, 15:04
 */

#include <cstdlib>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <ctime>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <sys/time.h>
#include <iosfwd>
#include <sstream>

#define GRID_HEIGHT 20
#define GRID_WIDTH 10

#define MOVE_LEFT       0
#define MOVE_RIGHT      1
#define MOVE_DOWN       2
#define MOVE_UP         3 //Rotation

using namespace std;

//Alamaceno la posicion de cada pieza dentro de ella pues el display siempre tiene que mostrarlas todas.

struct Piece {
    int x, y;
    int color;
    int shape[3][3]; //RGB en cada fila/columna, 0 = aire
};

int GameVelocity = 500;
Piece * current;
int gameGrid[GRID_WIDTH][GRID_HEIGHT];
TTF_Font*font;

int COLORS[] = {0x0000FF, 0xFF0000, 0x00FF00, 0xFFFF00};
long lastTick = 0;

int points = 0;
bool lose = false;
bool pause = false;
void rotate(Piece& piece, short arrd);
void generate_piece(Piece &piece);
void update_scene(SDL_Surface *screen);
bool new_tick();
bool game_tick();
void reset();

timeval before;

unsigned long mtime() {
    gettimeofday(&before, NULL);
    return (time(NULL) * 1000000 + before.tv_usec) / 1000;
}

/**
 * 
 * @param max
 * @param min
 * @return rand [min-max]
 */
int random(int max) {
    return rand() % max;
}

void rotate_right(Piece& piece) {
    rotate(piece, 1);
}

void rotate_left(Piece& piece) {
    rotate(piece, -1);
}

bool tile_free(int x, int y) {
    if (x >= 0 && y >= 0 &&
            x < GRID_WIDTH && y < GRID_HEIGHT) {
        return gameGrid[x][y] == 0;
    }
    return false;
}

bool move(int addr) {
    if (current == NULL) return false;
    if (addr == MOVE_RIGHT) {
        int offset = 0;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                if (current->shape[i][j] != 0) {
                    offset = i;
                    if (!tile_free(current->x + i + 1, current->y + j))
                        return false;
                }

        if (offset + current->x < GRID_WIDTH - 1) {
            current->x++;
            return true;
        }
    } else if (addr == MOVE_LEFT) {
        int offset = 3;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                if (current->shape[i][j] != 0) {
                    offset = (i < offset) ? i : offset;
                    if (!tile_free(current->x + i - 1, current->y + j))
                        return false;
                }
        if (offset >= 0 && offset + current->x > 0) {
            current->x--;
            return true;
        }
    } else if (addr == MOVE_UP) {
        Piece aux = *current;
        rotate(aux, 1);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (aux.shape[i][j] != 0) {
                    if (aux.x + i >= GRID_WIDTH)
                        return false;
                    if (aux.x + i < 0)
                        return false;
                    if (!tile_free(aux.x + i, aux.y + j))
                        return false;
                }
            }
        }
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                current->shape[i][j] = aux.shape[i][j];
            }
        }
        return true;

    } else //if MOVE_DOWN
    {
        bool last = true;
        for (int y = current->y; y >= 0; y--) {
            bool available = true;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    if (current->shape[i][j] != 0 && !tile_free(current->x + i, y + j - 1)) {
                        available = false;
                    }
                }
            }
            if (!available && last) {
                current->y = y;
                return true;
            }
            last = available;

        }
    }

    return false;
}

void rotate(Piece& piece, short addr) {
    int factor = 2 * addr;
    int aux[3][3];
    if (addr > 0) {
        aux[0][0] = piece.shape[2][0];
        aux[0][1] = piece.shape[1][0];
        aux[0][2] = piece.shape[0][0];

        aux[1][0] = piece.shape[2][1];
        aux[1][1] = piece.shape[1][1];
        aux[1][2] = piece.shape[0][1];

        aux[2][0] = piece.shape[2][2];
        aux[2][1] = piece.shape[1][2];
        aux[2][2] = piece.shape[0][2];
    } else {
        aux[0][0] = piece.shape[0][2];
        aux[0][1] = piece.shape[1][2];
        aux[0][2] = piece.shape[2][2];

        aux[1][0] = piece.shape[0][1];
        aux[1][1] = piece.shape[1][1];
        aux[1][2] = piece.shape[2][1];

        aux[2][0] = piece.shape[0][0];
        aux[2][1] = piece.shape[1][0];
        aux[2][2] = piece.shape[2][0];
    }

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++)
            piece.shape[i][j] = aux[i][j];
    }
}

int main(int argc, char *argv[]) {
    SDL_Surface *screen;
    SDL_Event event;

    if (SDL_Init(SDL_INIT_VIDEO) == -1) {
        printf("Error: %s\n", SDL_GetError());
        return 1;
    }

    screen = SDL_SetVideoMode(320, 500, 32, SDL_SWSURFACE | SDL_RESIZABLE);

    if (screen == NULL) {
        printf("Error: %s\n", SDL_GetError());
        return 1;
    }
    printf("Initializing core system...\n");

    SDL_WM_SetCaption("Tetris", NULL);


    TTF_Init();

    font = TTF_OpenFont("arial.ttf", 18);
    if(font == NULL)
    {
        printf("Error loading font!!\n");
        return 0;
    }
    bool running = true;

    while (running) {
        if (new_tick()) {
            update_scene(screen);
        }
        while (SDL_PollEvent(&event)) {

            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    SDLKey keyPressed = event.key.keysym.sym;
                    printf("Key pressed: %d\n", keyPressed);
                    bool __update_scene = false;
                    switch (keyPressed) {
                        case SDLK_ESCAPE:
                            running = false;
                            break;
                        case SDLK_LEFT:
                            __update_scene = move(MOVE_LEFT);
                            break;
                        case SDLK_RIGHT:
                            __update_scene = move(MOVE_RIGHT);
                            break;
                        case SDLK_UP:
                            __update_scene = move(MOVE_UP);
                            break;
                        case SDLK_DOWN:
                            __update_scene = move(MOVE_DOWN);
                            break;
                        case SDLK_SPACE:
                            if(lose)
                            {
                                reset();
                            }
                            else
                            {
                                
                                pause = !pause;
                            }
                            break;
                    }
                    if (__update_scene) {
                        update_scene(screen);
                    }
                    break;
            }

        }
    }
    SDL_Quit();
    return 0;
}

bool new_tick() {
    if (mtime() - lastTick > GameVelocity) {
        lastTick = mtime();
        return game_tick();
    }
    return false;
}

void reset() {
    for (int i = 0; i < GRID_WIDTH; i++) {
        for (int j = 0; j < GRID_HEIGHT; j++) {
            gameGrid[i][j] = 0;
        }
    }
    GameVelocity = 500;
    current = NULL;
    lose = false;
}

bool game_tick() {
    if(pause||lose) return true;
    if (current == NULL) {
        Piece * p = new Piece();
        generate_piece(*p);
        current = p;
        current->y--;
        return true;
    } else {
        bool stop = false;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                int x, y;
                x = current->x + i;
                y = current->y + j;
                if (current->shape[i][j] != 0 && (x < 0 || y <= 0 || x >= GRID_WIDTH || y >= GRID_HEIGHT)) {
                    stop = true;
                    break;
                }
                if (current->shape[i][j] != 0 && !tile_free(x, y - 1)) {
                    stop = true;
                    break;
                }
            }
            if (stop) break;
        }
        if (stop) {
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    int x, y;
                    x = current->x + i;
                    y = current->y + j;
                    if (x >= 0 && y >= 0 && x < GRID_WIDTH && y < GRID_HEIGHT && current->shape[i][j] != 0) {
                        gameGrid[x][y] = current->shape[i][j];
                    }
                }
            }
            bool found = true;
            int pos = 0;
            while (found) {
                found = false;
                for (int j = 0; j < GRID_HEIGHT; j++) {
                    int sum = 0;
                    for (int i = 0; i < GRID_WIDTH; i++) {
                        if (gameGrid[i][j] != 0) {
                            sum++;
                        }
                    }
                    if (sum == GRID_WIDTH) {
                        found = true;
                        pos = j;
                        break;
                    }
                }
                int mult = 1;
                if (found) {
                    points += (20 ^ (mult++));
                    for (int i = 0; i < GRID_WIDTH; i++) {
                        for (int j = pos; j < GRID_HEIGHT - 1; j++) {
                            gameGrid[i][j] = gameGrid[i][j + 1];
                        }
                    }
                }
            }
            GameVelocity--;

            if (current->y + 3 >= GRID_HEIGHT)
                lose = true;

            delete current;
            current = NULL;
        } else {
            current->y--;
        }


    }
    return true;
}

void generate_piece(Piece& piece) {
    switch (random(4)) {
        case 0:
            piece.shape[0][0] = COLORS[0];
            piece.shape[0][1] = COLORS[0];
            piece.shape[0][2] = COLORS[0];

            piece.shape[1][2] = COLORS[0];
            break;
        case 1:
            piece.shape[0][0] = COLORS[1];
            piece.shape[0][1] = COLORS[1];
            piece.shape[0][2] = COLORS[1];
            break;
        case 2:
            piece.shape[0][0] = COLORS[2];
            piece.shape[0][1] = COLORS[2];
            piece.shape[1][0] = COLORS[2];
            piece.shape[1][1] = COLORS[2];
            break;
        case 3:
            piece.shape[2][0] = COLORS[3];
            piece.shape[2][1] = COLORS[3];
            piece.shape[2][2] = COLORS[3];

            piece.shape[1][2] = COLORS[3];
            break;
    }

    piece.x = (GRID_WIDTH / 2) - 2;
    piece.y = GRID_HEIGHT - 2;
}
void draw_text(SDL_Surface * screen, char * data, int x, int y)
{
    
    SDL_Color c = {0xFF, 0x00, 0x00};
    SDL_Surface *t = TTF_RenderText_Blended(font, data, c);
    if (t != NULL) {

        SDL_Rect text_rect = {x, y, t->w, t->h};
        SDL_BlitSurface(t, &t->clip_rect, screen, &text_rect);

    }
    else
    {
        printf("Null pointer!\n");
    }
}
void update_scene(SDL_Surface *screen) {
    
    //Draw background
    SDL_Rect rect = {0, 0, screen->w, screen->h};
    Uint32 a = 0x000000; //SDL_MapRGB(screen->format, 255, 0, 0);
    SDL_FillRect(screen, &rect, a);
    int tile_w = screen->w / GRID_WIDTH;
    int tile_h = screen->h / GRID_HEIGHT;
    for (int i = 0; i < GRID_WIDTH; i++) {
        for (int j = 0; j < GRID_HEIGHT; j++) {
            if (gameGrid[i][j] > 0) {
                SDL_Rect tile_rect = {i * tile_w, (GRID_HEIGHT - j - 1) * tile_h, tile_w, tile_h};
                SDL_FillRect(screen, &tile_rect, gameGrid[i][j]);
            }
        }
    }
    if (current != NULL) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (current->shape[i][j] == 0) continue;
                SDL_Rect tile_rect = {(current->x + i) * tile_w, (GRID_HEIGHT - (current->y + j) - 1) * tile_h, tile_w, tile_h};
                SDL_FillRect(screen, &tile_rect, current->shape[i][j]);

            }
        }
    }
    
    std::stringstream ss;
    ss << "Puntos " << points;
    
    SDL_Color c = {0xFF, 0x00, 0x00};
    SDL_Surface *t = TTF_RenderText_Blended(font, ss.str().c_str(), c);
    if (t != NULL) {

        SDL_Rect text_rect = {0, 0, t->w, t->h};
        SDL_BlitSurface(t, &t->clip_rect, screen, &text_rect);

    }
    else
    {
        printf("Null pointer!\n");
    }

    if(lose)
    {
        
        draw_text(screen, "YOU LOSE!!", screen->w / 2, screen->h / 2);
    }
    if(pause)
    {
        
        draw_text(screen, "PAUSE", screen->w / 2, screen->h / 2);
    }
    SDL_UpdateRect(screen, 0, 0, 0, 0);
}