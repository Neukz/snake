#include <stdio.h>

extern "C"
{
    #include "./SDL2-2.0.10/include/SDL.h"
    #include "./SDL2-2.0.10/include/SDL_main.h"
}


#define WINDOW_WIDTH 1080
#define WINDOW_HEIGHT 640
#define BOARD_WIDTH 800
#define BOARD_HEIGHT 600
#define INFO_PANEL_HEIGHT 40
#define SEGMENT_SIZE 20
#define INITIAL_SNAKE_LENGTH 3


typedef enum
{
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

typedef struct
{
    int x;
    int y;
} Segment;


void DrawPixel(SDL_Surface* surface, int x, int y, Uint32 color)
{
    int bpp = surface->format->BytesPerPixel;
    Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;
    *(Uint32*)p = color;
}

void DrawRectangle(SDL_Surface* screen, int x, int y, int width, int height, Uint32 outlineColor, Uint32 fillColor)
{
    for (int row = y; row < y + height; row++)
    {
        for (int col = x; col < x + width; col++)
        {
            if (row == y || row == y + height - 1 || col == x || col == x + width - 1)
            {
                DrawPixel(screen, col, row, outlineColor);
            }
            else
            {
                DrawPixel(screen, col, row, fillColor);
            }
        }
    }
}

void DrawString(SDL_Surface* screen, int x, int y, const char* text, SDL_Surface* charset)
{
    int px, py, c;
    SDL_Rect s, d;
    s.w = 8;
    s.h = 8;
    d.w = 8;
    d.h = 8;
    while (*text)
    {
        c = *text & 255;
        px = (c % 16) * 8;
        py = (c / 16) * 8;
        s.x = px;
        s.y = py;
        d.x = x;
        d.y = y;
        SDL_BlitSurface(charset, &s, screen, &d);
        x += 8;
        text++;
    }
}

class Snake
{
private:
    int length;
    Segment* body;
    Segment* head;
    Direction direction;

    int IsOppositeDirection(Direction newDirection)
    {
        return (direction == UP && newDirection == DOWN) ||
            (direction == DOWN && newDirection == UP) ||
            (direction == LEFT && newDirection == RIGHT) ||
            (direction == RIGHT && newDirection == LEFT);
    }

    int IsDirectionIntoEdge(Direction newDirection)
    {
        int leftEdge = (WINDOW_WIDTH - BOARD_WIDTH) / 2;
        int rightEdge = (WINDOW_WIDTH + BOARD_WIDTH) / 2 - SEGMENT_SIZE;
        int topEdge = INFO_PANEL_HEIGHT;
        int bottomEdge = INFO_PANEL_HEIGHT + BOARD_HEIGHT - SEGMENT_SIZE;

        return (newDirection == LEFT && head->x <= leftEdge) ||
            (newDirection == RIGHT && head->x >= rightEdge) ||
            (newDirection == UP && head->y <= topEdge) ||
            (newDirection == DOWN && head->y >= bottomEdge);
    }

    // Change direction automatically when snake hits the edge
    void HandleEdgeInteraction()
    {
        int leftEdge = (WINDOW_WIDTH - BOARD_WIDTH) / 2;
        int rightEdge = (WINDOW_WIDTH + BOARD_WIDTH) / 2 - SEGMENT_SIZE;
        int topEdge = INFO_PANEL_HEIGHT;
        int bottomEdge = INFO_PANEL_HEIGHT + BOARD_HEIGHT - SEGMENT_SIZE;

        if (direction == LEFT && head->x <= leftEdge)
        {
			direction = IsDirectionIntoEdge(UP) ? DOWN : UP;
        }
        else if (direction == RIGHT && head->x >= rightEdge)
        {
			direction = IsDirectionIntoEdge(DOWN) ? UP : DOWN;
        }
        else if (direction == UP && head->y <= topEdge)
        {
			direction = IsDirectionIntoEdge(RIGHT) ? LEFT : RIGHT;
        }
        else if (direction == DOWN && head->y >= bottomEdge)
        {
			direction = IsDirectionIntoEdge(LEFT) ? RIGHT : LEFT;
        }
    }

public:
    Snake()
    {
        Initialize();
    }

    ~Snake()
    {
        free(body);
    }

    void Initialize()
    {
        length = INITIAL_SNAKE_LENGTH;
        body = (Segment*)malloc(length * sizeof(Segment));
        head = &body[0];
        direction = RIGHT;
        int startX = (WINDOW_WIDTH - BOARD_WIDTH) / 2 + BOARD_WIDTH / 2;
        int startY = INFO_PANEL_HEIGHT + BOARD_HEIGHT / 2;
        for (int i = 0; i < length; i++)
        {
            body[i].x = startX - i * SEGMENT_SIZE;
            body[i].y = startY;
        }
    }

    void SetDirection(Direction newDirection)
    {
        if (!IsOppositeDirection(newDirection) && !IsDirectionIntoEdge(newDirection))
        {
            direction = newDirection;
        }
    }

    void Move()
    {
        HandleEdgeInteraction();

        for (int i = length - 1; i > 0; i--)
        {
            body[i] = body[i - 1]; // Shift body segments
        }

        // Move head based on current direction
        if (direction == UP)
        {
            head->y -= SEGMENT_SIZE;
        }
        else if (direction == DOWN)
        {
            head->y += SEGMENT_SIZE;
        }
        else if (direction == LEFT)
        {
            head->x -= SEGMENT_SIZE;
        }
        else if (direction == RIGHT)
        {
            head->x += SEGMENT_SIZE;
        }
    }

    void Draw(SDL_Surface* screen, Uint32 color)
    {
        for (int i = 0; i < length; i++)
        {
            DrawRectangle(screen, body[i].x, body[i].y, SEGMENT_SIZE, SEGMENT_SIZE, color, color);
        }
    }
};

class Game
{
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* screen;
    SDL_Surface* charset;
    SDL_Texture* scrtex;
    Snake snake;
    int quit;

public:
    Game() : window(NULL), renderer(NULL), screen(NULL), charset(NULL), scrtex(NULL), quit(0) {}

    ~Game()
    {
        Cleanup();
    }

    int Initialize()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            printf("SDL_Init error: %s\n", SDL_GetError());
            return 0;
        }

        if (SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer) != 0)
        {
            printf("SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
            SDL_Quit();
            return 0;
        }

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_SetWindowTitle(window, "Snake");
        SDL_ShowCursor(SDL_DISABLE);

        screen = SDL_CreateRGBSurface(0, WINDOW_WIDTH, WINDOW_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        scrtex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);

        charset = SDL_LoadBMP("cs8x8.bmp");
        if (!charset)
        {
            printf("SDL_LoadBMP(cs8x8.bmp) error: %s\n", SDL_GetError());
            Cleanup();
            return 0;
        }
        SDL_SetColorKey(charset, 1, 0x000000);
        return 1;
    }

    void Run()
    {
        SDL_Event event;
        Uint32 black = SDL_MapRGB(screen->format, 0, 0, 0);
        Uint32 white = SDL_MapRGB(screen->format, 255, 255, 255);
        Uint32 green = SDL_MapRGB(screen->format, 0, 255, 0);

        while (quit == 0)
        {
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_QUIT:
                        quit = 1;
                        break;
                    case SDL_KEYDOWN:
                        switch (event.key.keysym.sym)
                        {
                            case SDLK_ESCAPE:
                                quit = 1;
                                break;
                            case SDLK_UP:
                                snake.SetDirection(UP);
                                break;
                            case SDLK_DOWN:
                                snake.SetDirection(DOWN);
                                break;
                            case SDLK_LEFT:
                                snake.SetDirection(LEFT);
                                break;
                            case SDLK_RIGHT:
                                snake.SetDirection(RIGHT);
                                break;
                            case SDLK_n:    // Restart game
                                snake.Initialize();
                                break;
                        }
                        break;
                }
            }

            // Draw info panel
            DrawRectangle(screen, 0, 0, WINDOW_WIDTH, INFO_PANEL_HEIGHT, white, black);
            DrawString(screen, 15, 15, "Press 'Esc' to Quit, 'n' for New Game", charset);

            // Draw game board
            DrawRectangle(screen, (WINDOW_WIDTH - BOARD_WIDTH) / 2, INFO_PANEL_HEIGHT, BOARD_WIDTH, BOARD_HEIGHT, white, black);

            snake.Move();
            snake.Draw(screen, green);

            SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
            SDL_RenderCopy(renderer, scrtex, NULL, NULL);
            SDL_RenderPresent(renderer);

            SDL_Delay(100); // Control speed
        }
    }

    void Cleanup()
    {
        SDL_FreeSurface(charset);
        SDL_FreeSurface(screen);
        SDL_DestroyTexture(scrtex);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
};


int main(int argc, char** argv)
{
    Game game;
    if (game.Initialize() == 0)
    {
        return 1;
    }

    game.Run();

    return 0;
}