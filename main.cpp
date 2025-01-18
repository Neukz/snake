/*
    Snake
    Kacper Neumann, 203394

    This game has been developed based on the SDL2 demo program provided by Prof. Dariusz Dereniowski.

    Functions/methods do not exceed 1024 bytes.
    Counter: https://gre-v-el.github.io/Cpp-Func-Length-Counter/
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern "C"
{
#include "./SDL2-2.0.10/include/SDL.h"
#include "./SDL2-2.0.10/include/SDL_main.h"
}

// --- CONFIGURATION ---
// Screen dimensions
#define WINDOW_WIDTH 1080
#define WINDOW_HEIGHT 640
#define SEGMENT_SIZE 20 // Basic board unit
#define INFO_HEIGHT 50
#define BOARD_WIDTH (SEGMENT_SIZE * 25)
#define BOARD_HEIGHT (SEGMENT_SIZE * 25)
#define PROGRESS_BAR_WIDTH 200
#define PROGRESS_BAR_HEIGHT 20

// Positioning
#define LEFT_EDGE ((WINDOW_WIDTH - BOARD_WIDTH) / 2)
#define RIGHT_EDGE (LEFT_EDGE + BOARD_WIDTH)
#define TOP_EDGE (INFO_HEIGHT)
#define BOTTOM_EDGE (TOP_EDGE + BOARD_HEIGHT)
#define PROGRESS_BAR_X ((WINDOW_WIDTH - PROGRESS_BAR_WIDTH) / 2)
#define PROGRESS_BAR_Y (BOTTOM_EDGE + 10)

// Text settings
#define INFO_TEXT_SCALE 1
#define GAME_OVER_TEXT_SCALE 2.5

// Snake settings
#define INITIAL_SNAKE_X (LEFT_EDGE + (BOARD_WIDTH / 2 / SEGMENT_SIZE) * SEGMENT_SIZE)   // Start in the middle of the board
#define INITIAL_SNAKE_Y (TOP_EDGE + (BOARD_HEIGHT / 2 / SEGMENT_SIZE) * SEGMENT_SIZE)
#define INITIAL_SNAKE_LENGTH 3  // Number of segments
#define INITIAL_SNAKE_MOVE_INTERVAL 200 // ms
#define SPEED_UP_INTERVAL 7000 // ms
#define SPEED_UP_FACTOR 0.9 // Range (0, 1) for increasing speed

// Food settings
#define FOOD_POINTS 1

// Bonus dot settings
#define BONUS_PROBABILITY 30 // %
#define BONUS_INTERVAL 3000 // ms
#define BONUS_DURATION 5000 // ms
#define BONUS_SHRINK_COUNT 3 // Segments to shorten
#define BONUS_SLOW_DOWN_FACTOR 1.2 // Range (1, inf) for decreasing speed
#define BONUS_POINTS 2

// Colors
#define BACKGROUND_COLOR 0x000000
#define OUTLINE_COLOR 0xFFFFFF
#define SNAKE_COLOR 0x00FF00
#define FOOD_COLOR 0x0000FF
#define BONUS_COLOR 0xFF0000

// --- TYPE DEFINITIONS ---
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

// --- UTILITY FUNCTIONS ---
int RandomInt(int min, int max)
{
    return rand() % (max - min + 1) + min;
}

// Get the starting x-coordinate for displaying centered text
int CenterTextX(const char* text, float scale)
{
	return (WINDOW_WIDTH - strlen(text) * 8 * scale) / 2;
}

// --- DRAWING FUNCTIONS ---
void DrawPixel(SDL_Surface* surface, int x, int y, Uint32 color)
{
    int bpp = surface->format->BytesPerPixel;
    Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;
    *(Uint32*)p = color;
}

void DrawLine(SDL_Surface* screen, int x, int y, int length, int dx, int dy, Uint32 color)
{
    for (int i = 0; i < length; i++)
    {
        DrawPixel(screen, x, y, color);
        x += dx;
        y += dy;
    }
}

void DrawRectangle(SDL_Surface* screen, int x, int y, int width, int height, Uint32 outlineColor, Uint32 fillColor)
{
	if (outlineColor != NULL)   // Outline is optional
    {
        DrawLine(screen, x, y, height, 0, 1, outlineColor);
        DrawLine(screen, x + width - 1, y, height, 0, 1, outlineColor);
        DrawLine(screen, x, y, width, 1, 0, outlineColor);
        DrawLine(screen, x, y + height - 1, width, 1, 0, outlineColor);
    }

	if (fillColor != NULL)  // Fill is optional
    {
        for (int i = y + 1; i < y + height - 1; i++)
        {
            DrawLine(screen, x + 1, i, width - 2, 1, 0, fillColor);
        }
    }
}

void DrawString(SDL_Surface* screen, int x, int y, const char* text, SDL_Surface* charset, float scale)
{
    int px, py, c;
    SDL_Rect s, d;
    s.w = 8;
    s.h = 8;
    d.w = 8 * scale;
    d.h = 8 * scale;
    while (*text)
    {
        c = *text & 255;
        px = (c % 16) * 8;
        py = (c / 16) * 8;
        s.x = px;
        s.y = py;
        d.x = x;
        d.y = y;
        SDL_BlitScaled(charset, &s, screen, &d);
        x += 8 * scale;
        text++;
    }
}

// --- CLASSES ---
class Snake
{
private:
    int length;
    Segment* body;
	Segment* head;  // Pointer to body[0]
    Direction direction;
    Uint32 lastMoveTime;
    int moveInterval;
    int mayChangeDirection; // Flag to prevent changing direction twice in one move

    int IsOppositeDirection(Direction newDirection)
    {
        return ((direction == UP && newDirection == DOWN) ||
            (direction == DOWN && newDirection == UP) ||
            (direction == LEFT && newDirection == RIGHT) ||
            (direction == RIGHT && newDirection == LEFT)) ? 1 : 0;
    }

    int IsDirectionIntoEdge(Direction newDirection)
    {
        return ((newDirection == LEFT && head->x <= LEFT_EDGE) ||
            (newDirection == RIGHT && head->x >= RIGHT_EDGE - SEGMENT_SIZE) ||
            (newDirection == UP && head->y <= TOP_EDGE) ||
            (newDirection == DOWN && head->y >= BOTTOM_EDGE - SEGMENT_SIZE)) ? 1 : 0;
    }

    void ChangeDirectionOnEdge()
    {
        if (direction == LEFT && head->x <= LEFT_EDGE)
        {
            direction = IsDirectionIntoEdge(UP) ? DOWN : UP;
        }
        else if (direction == RIGHT && head->x >= RIGHT_EDGE - SEGMENT_SIZE)
        {
            direction = IsDirectionIntoEdge(DOWN) ? UP : DOWN;
        }
        else if (direction == UP && head->y <= TOP_EDGE)
        {
            direction = IsDirectionIntoEdge(RIGHT) ? LEFT : RIGHT;
        }
        else if (direction == DOWN && head->y >= BOTTOM_EDGE - SEGMENT_SIZE)
        {
            direction = IsDirectionIntoEdge(LEFT) ? RIGHT : LEFT;
        }
    }

public: 
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
        lastMoveTime = SDL_GetTicks();
		moveInterval = INITIAL_SNAKE_MOVE_INTERVAL;
		mayChangeDirection = 1;
        for (int i = 0; i < length; i++)
        {
            body[i].x = INITIAL_SNAKE_X - i * SEGMENT_SIZE;
            body[i].y = INITIAL_SNAKE_Y;
        }
    }

    void SetDirection(Direction newDirection)
    {
        if (mayChangeDirection && !IsOppositeDirection(newDirection) && !IsDirectionIntoEdge(newDirection))
        {
            direction = newDirection;
			mayChangeDirection = 0;
        }
    }

    int CollidesWith(Segment segment)
    {
        for (int i = 0; i < length; i++)
        {
            if (body[i].x == segment.x && body[i].y == segment.y)
            {
                return 1;
            }
        }
        return 0;
    }

    int HeadCollidesWith(Segment segment)
    {
        return (head->x == segment.x && head->y == segment.y) ? 1 : 0;
    }

    int SelfCollision()
    {
        for (int i = 1; i < length; i++)
        {
            if (HeadCollidesWith(body[i]))
            {
                return 1;
            }
        }
        return 0;
    }

    void Grow()
    {
        length++;
        body = (Segment*)realloc(body, length * sizeof(Segment));
        body[length - 1] = body[length - 2];
		head = &body[0];
    }

    void Shrink(int count)
    {
        length -= count;
        if (length < INITIAL_SNAKE_LENGTH)
        {
            length = INITIAL_SNAKE_LENGTH; // Minimum length
        }
        body = (Segment*)realloc(body, length * sizeof(Segment));
        head = &body[0];
    }

    void Move(Uint32 currentTime)
    {
		// Move snake with a fixed interval
        if (currentTime - lastMoveTime >= moveInterval)
        {
            ChangeDirectionOnEdge();

            for (int i = length - 1; i > 0; i--)
            {
                body[i] = body[i - 1];  // Shift body segments
            }

            // Move head based on current direction
            switch (direction)
            {
                case UP:
                    head->y -= SEGMENT_SIZE;
                    break;
                case DOWN:
                    head->y += SEGMENT_SIZE;
                    break;
                case LEFT:
                    head->x -= SEGMENT_SIZE;
                    break;
                case RIGHT:
                    head->x += SEGMENT_SIZE;
                    break;
            }

            lastMoveTime = currentTime;
			mayChangeDirection = 1;
        }
    }

    void AdjustSpeed(float factor)
    {
        moveInterval = (int)(moveInterval * factor);
    }

	void Draw(SDL_Surface* screen)
    {
        for (int i = 0; i < length; i++)
        {
            DrawRectangle(screen, body[i].x, body[i].y, SEGMENT_SIZE, SEGMENT_SIZE, NULL, SNAKE_COLOR);
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
	SDL_Event event;
    Snake snake;
    Segment food;
    Segment bonus;
    Uint32 startTime;
    Uint32 currentTime;
	Uint32 lastSpeedUpTime;
	Uint32 lastBonusTime;
    int points;
	int bonusActive;
    int quit;           // Flag to check if the game should end
	int initialized;    // Flag to check if initialization was successful

    void GenerateFood()
    {
        do
        {
            food.x = LEFT_EDGE + RandomInt(0, (BOARD_WIDTH / SEGMENT_SIZE) - 1) * SEGMENT_SIZE;
            food.y = TOP_EDGE + RandomInt(0, (BOARD_HEIGHT / SEGMENT_SIZE) - 1) * SEGMENT_SIZE;
		} while (snake.CollidesWith(food) || (bonus.x == food.x && bonus.y == food.y)); // Prevent from spawning on snake or bonus
    }

    void GenerateBonus()
    {
        do
        {
            bonus.x = LEFT_EDGE + RandomInt(0, (BOARD_WIDTH / SEGMENT_SIZE) - 1) * SEGMENT_SIZE;
            bonus.y = TOP_EDGE + RandomInt(0, (BOARD_HEIGHT / SEGMENT_SIZE) - 1) * SEGMENT_SIZE;
		} while (snake.CollidesWith(bonus) || (bonus.x == food.x && bonus.y == food.y));    // Prevent from spawning on snake or food
        bonusActive = 1;
    }

    void DrawBonusProgressBar()
    {
        Uint32 elapsedTime = currentTime - lastBonusTime;
        if (elapsedTime >= BONUS_DURATION)
        {
            bonusActive = 0;
        }
        else
        {
            int barWidth = (int)((1.0 - (float)elapsedTime / BONUS_DURATION) * PROGRESS_BAR_WIDTH);
            DrawRectangle(screen, PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, OUTLINE_COLOR, NULL);
            DrawRectangle(screen, PROGRESS_BAR_X, PROGRESS_BAR_Y, barWidth, PROGRESS_BAR_HEIGHT, NULL, BONUS_COLOR);
        }
    }

    void GameOver()
    {
        while (1)
        {
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_QUIT:
                        quit = 1;
                        return;
                    case SDL_KEYDOWN:
                        switch (event.key.keysym.sym)
                        {
                            case SDLK_ESCAPE:
                                quit = 1;
                                return;
                            case SDLK_n:
                                NewGame();
                                return;
                        }
                        break;
                }
            }

            SDL_FillRect(screen, NULL, BACKGROUND_COLOR);

            const char* gameOver = "Game Over!";
            char score[32];
            sprintf(score, "Score: %d", points);
            const char* hint = "Press 'Esc' to Quit or 'n' to Restart";

            DrawString(screen, CenterTextX(gameOver, GAME_OVER_TEXT_SCALE), WINDOW_HEIGHT / 2 - 50, gameOver, charset, GAME_OVER_TEXT_SCALE);
            DrawString(screen, CenterTextX(score, GAME_OVER_TEXT_SCALE), WINDOW_HEIGHT / 2, score, charset, GAME_OVER_TEXT_SCALE);
            DrawString(screen, CenterTextX(hint, GAME_OVER_TEXT_SCALE), WINDOW_HEIGHT / 2 + 50, hint, charset, GAME_OVER_TEXT_SCALE);;

            RefreshScreen();
        }
    }

    void HandleControls()
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
                        case SDLK_n:
                            NewGame();
                            break;
                    }
                    break;
            }
        }
    }

    void HandleBonus()
    {
        if (bonusActive && currentTime - lastBonusTime >= BONUS_DURATION)
        {
            bonusActive = 0;
			lastBonusTime = currentTime;
        }

        if (!bonusActive && currentTime - lastBonusTime >= BONUS_INTERVAL)
        {
            if (RandomInt(1, 100) <= BONUS_PROBABILITY)
            {
                GenerateBonus();
            }
            lastBonusTime = currentTime;
        }

        if (bonusActive && snake.HeadCollidesWith(bonus))
        {
			points += BONUS_POINTS;
            bonusActive = 0;
            lastBonusTime = currentTime;
            if (RandomInt(0, 1) == 0)
            {
                snake.Shrink(BONUS_SHRINK_COUNT);
            }
            else
            {
                snake.AdjustSpeed(BONUS_SLOW_DOWN_FACTOR);
            }
        }
    }

    void UpdateScreen()
    {
        SDL_FillRect(screen, NULL, BACKGROUND_COLOR);

		float elapsedTime = (currentTime - startTime) * 0.001;   // Convert ms to s
		char info[256];
        sprintf(info, "'Esc' - Quit  |  'n' - Restart  |  Time: %.2f s  |  Score: %d  |  Implemented Requirements: 1, 2, 3, 4, A, B, C, D", elapsedTime, points);

        // Draw info
        DrawString(screen, CenterTextX(info, 1), 20, info, charset, INFO_TEXT_SCALE);
        DrawRectangle(screen, 0, 0, WINDOW_WIDTH, INFO_HEIGHT, OUTLINE_COLOR, BACKGROUND_COLOR);

        // Draw game board
        DrawRectangle(screen, LEFT_EDGE, TOP_EDGE, BOARD_WIDTH, BOARD_HEIGHT, OUTLINE_COLOR, BACKGROUND_COLOR);

		// Draw food
        DrawRectangle(screen, food.x, food.y, SEGMENT_SIZE, SEGMENT_SIZE, NULL, FOOD_COLOR);

		// Draw bonus
        if (bonusActive)
        {
            DrawRectangle(screen, bonus.x, bonus.y, SEGMENT_SIZE, SEGMENT_SIZE, NULL, BONUS_COLOR);
			DrawBonusProgressBar();
        }
		
        snake.Draw(screen);

        RefreshScreen();
    }

    void RefreshScreen()
    {
        SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
        SDL_RenderCopy(renderer, scrtex, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    void NewGame()
    {
        snake.Initialize();
        GenerateFood();
        startTime = SDL_GetTicks();
        lastSpeedUpTime = startTime;
		bonusActive = 0;
        points = 0;
    }

public:
    Game()
    {
		quit = 0;
		initialized = 0;
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            printf("SDL_Init error: %s\n", SDL_GetError());
            return;
        }

        if (SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer) != 0)
        {
            printf("SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
            SDL_Quit();
            return;
        }

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_SetWindowTitle(window, "Snake | Kacper Neumann, 203394");
        SDL_ShowCursor(SDL_DISABLE);

        screen = SDL_CreateRGBSurface(0, WINDOW_WIDTH, WINDOW_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        scrtex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);

        charset = SDL_LoadBMP("cs8x8.bmp");
        if (charset == NULL)
        {
            printf("SDL_LoadBMP(cs8x8.bmp) error: %s\n", SDL_GetError());
            Cleanup();
            return;
        }
        SDL_SetColorKey(charset, 1, 0x000000);

        NewGame();
        initialized = 1;
    }

    ~Game()
    {
        Cleanup();
    }

	int GetInitialized()
	{
		return initialized;
	}

    void Run()
    {
        while (quit == 0)
        {
            HandleControls();

            currentTime = SDL_GetTicks();
            if (currentTime - lastSpeedUpTime >= SPEED_UP_INTERVAL)
            {
                snake.AdjustSpeed(SPEED_UP_FACTOR);
                lastSpeedUpTime = currentTime;
            }

            HandleBonus();

            if (snake.HeadCollidesWith(food))
            {
                snake.Grow();
                GenerateFood();
				points += FOOD_POINTS;
            }

            snake.Move(currentTime);
            if (snake.SelfCollision())
            {
                GameOver();
            }

            UpdateScreen();
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

// --- MAIN PROGRAM ---
int main(int argc, char** argv)
{
    srand(time(NULL));

    Game game;
	if (game.GetInitialized() == 0)
    {
		return EXIT_FAILURE;
    }

    game.Run();

    return EXIT_SUCCESS;
}