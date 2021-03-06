#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <strsafe.h>
#include <stdint.h>

#include "job_queue.h"

#define USE_STRETCH_DI_BITS 0
#define USE_LIDKA_PRED 0
#define USE_MULTI_THREAD 1

const int32_t NCELLS_X = 1600;
const int32_t NCELLS_Y = 960;
const int32_t PIXELS_PER_CELL = 1;
const int32_t BYTES_PER_PIXEL = 4;
const uint32_t COLOR_DEAD = 0x00222222;
const uint32_t COLOR_ALIVE = 0x00fed844;

const int32_t WINDOW_HEIGHT = NCELLS_Y * PIXELS_PER_CELL;
const int32_t WINDOW_WIDTH =  NCELLS_X * PIXELS_PER_CELL;
const uint32_t colors[2] = {COLOR_DEAD, COLOR_ALIVE};

const size_t BUFFER_SIZE = 512;

static struct {
    void * buffer;
    BITMAPINFO bitmap_info;
    HDC back_buffer_context;
    HBITMAP hbitmap;
} screen;

static bool running = true;

uint32_t boards[2][(NCELLS_Y + 2) * (NCELLS_X + 2)];
uint32_t * current_board = boards[0];
uint32_t * next_board = boards[1];

static inline uint32_t bcoord(uint32_t x, uint32_t y) { return y * NCELLS_X + x; }
static inline uint32_t min2(uint32_t a, uint32_t b) { return (a < b) ? a : b; }

static inline void swap_boards()
{
    uint32_t * temp = current_board;
    current_board = next_board;
    next_board = temp;
}

static void update_board(uint32_t* old_board, uint32_t* new_board, int32_t startx, int32_t starty, int32_t endx, int32_t endy)
{
   // char buffer[BUFFER_SIZE];
   // StringCbPrintfA(buffer, BUFFER_SIZE, "tid: %8X x: %d..%d, y: %d..%d\n", GetCurrentThreadId(), startx, endx, starty, endy);
   // OutputDebugStringA(buffer);
    for (auto y = starty; y < endy; y++)
    {
        for (auto x = startx; x < endx; x++) 
        {
            auto coord = bcoord(x, y);
            auto live_neighbours = 
                       old_board[coord - 1 - NCELLS_X] +
                       old_board[coord     - NCELLS_X] +
                       old_board[coord + 1 - NCELLS_X] +
                       old_board[coord - 1           ] +
                       old_board[coord + 1           ] +
                       old_board[coord - 1 + NCELLS_X] +
                       old_board[coord     + NCELLS_X] +
                       old_board[coord + 1 + NCELLS_X];
            if (old_board[coord]) {
                if ((live_neighbours >= 2) && (live_neighbours <= 3)) {
                    new_board[coord] = 1;
                } else {
                    new_board[coord] = 0;
                }
            } else {
                if (live_neighbours == 3) {
                    new_board[coord] = 1;
                } else {
                    new_board[coord] = 0;
                }
            }
        }
    }
}

void Win32DrawRect(int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y, uint32_t color)
{
    uint32_t * row = ((uint32_t *)screen.buffer) + (start_y * WINDOW_WIDTH);
    uint32_t pitch = (WINDOW_WIDTH);
    for (auto y = start_y; y < end_y; y++)
    {
        uint32_t *pixel = row + start_x;
        for (auto x = start_x; x < end_x; x++)
        {
            *pixel = color;
            pixel++;
        }
        row += pitch;
    }
}
                

void render_board(uint32_t* board, int32_t startx, int32_t starty, int32_t endx, int32_t endy)
{
    for (auto y = starty; y < endy; y++)
    {
        for (auto x = startx; x < endx; x++) 
        {
            auto coord = bcoord(x, y);
            auto start_x = (x - 1) * PIXELS_PER_CELL;
            auto end_x = start_x + PIXELS_PER_CELL;
            auto start_y = (y - 1) * PIXELS_PER_CELL;
            auto end_y = start_y + PIXELS_PER_CELL;
            Win32DrawRect(start_x, start_y, end_x, end_y, colors[board[coord]]);
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Win32UpdateWindow(HDC device_context, int32_t x, int32_t y, int32_t width, int32_t height)
{
#if USE_STRETCH_DI_BITS
    StretchDIBits(device_context, 
            x, y, width, height, 
            x, y, width, height, 
            screen.buffer, 
            &screen.bitmap_info,
            DIB_RGB_COLORS,
            SRCCOPY);
#else
    BitBlt( device_context,
          x, y,
          width, height,
          screen.back_buffer_context,
          x, y,
          SRCCOPY);
#endif
}

void Win32AllocateScreenBuffer(int32_t width, int32_t height) {
    screen.bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    screen.bitmap_info.bmiHeader.biWidth = width;
    screen.bitmap_info.bmiHeader.biHeight = -height;
    screen.bitmap_info.bmiHeader.biPlanes = 1;
    screen.bitmap_info.bmiHeader.biBitCount = 32;
    screen.bitmap_info.bmiHeader.biCompression = BI_RGB;

#if USE_STRETCH_DI_BITS
    screen.buffer = VirtualAlloc(0, (width * height * BYTES_PER_PIXEL), MEM_COMMIT, PAGE_READWRITE);
#else
    screen.back_buffer_context = CreateCompatibleDC(0);
    screen.hbitmap = CreateDIBSection( 
          screen.back_buffer_context,
          &screen.bitmap_info,
          DIB_RGB_COLORS,
          &screen.buffer,
          0, 0);
    SelectObject(screen.back_buffer_context, screen.hbitmap);
#endif

    uint32_t * pixel = (uint32_t *)screen.buffer;
    for (auto i = 0; i < width*height; i++)
    {
        *pixel = 0x00ff00ff;
        pixel++;
    }
}

void glider_gun(uint32_t* board, uint32_t x, uint32_t y, uint32_t dir_x = 1, uint32_t dir_y = 1)
{
    auto start = bcoord(x, y);
    board[start + bcoord(dir_x *  0, dir_y * 4)] = 1;
    board[start + bcoord(dir_x *  1, dir_y * 4)] = 1;
    board[start + bcoord(dir_x *  0, dir_y * 5)] = 1;
    board[start + bcoord(dir_x *  1, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 10, dir_y * 4)] = 1;
    board[start + bcoord(dir_x * 10, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 10, dir_y * 6)] = 1;
    board[start + bcoord(dir_x * 11, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 11, dir_y * 7)] = 1;
    board[start + bcoord(dir_x * 12, dir_y * 2)] = 1;
    board[start + bcoord(dir_x * 12, dir_y * 8)] = 1;
    board[start + bcoord(dir_x * 13, dir_y * 2)] = 1;
    board[start + bcoord(dir_x * 13, dir_y * 8)] = 1;
    board[start + bcoord(dir_x * 14, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 15, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 15, dir_y * 7)] = 1;
    board[start + bcoord(dir_x * 16, dir_y * 4)] = 1;
    board[start + bcoord(dir_x * 16, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 16, dir_y * 6)] = 1;
    board[start + bcoord(dir_x * 17, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 20, dir_y * 2)] = 1;
    board[start + bcoord(dir_x * 20, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 20, dir_y * 4)] = 1;
    board[start + bcoord(dir_x * 21, dir_y * 2)] = 1;
    board[start + bcoord(dir_x * 21, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 21, dir_y * 4)] = 1;
    board[start + bcoord(dir_x * 22, dir_y * 1)] = 1;
    board[start + bcoord(dir_x * 22, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 24, dir_y * 0)] = 1;
    board[start + bcoord(dir_x * 24, dir_y * 1)] = 1;
    board[start + bcoord(dir_x * 24, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 24, dir_y * 6)] = 1;
    board[start + bcoord(dir_x * 34, dir_y * 2)] = 1;
    board[start + bcoord(dir_x * 34, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 35, dir_y * 2)] = 1;
    board[start + bcoord(dir_x * 35, dir_y * 3)] = 1;
}

void glider(uint32_t* board, uint32_t x, uint32_t y, uint32_t dir_x = 1, uint32_t dir_y = 1)
{
    auto start = bcoord(x, y);
    board[start + bcoord(dir_x * 2, dir_y * 0)] = 1;
    board[start + bcoord(dir_x * 2, dir_y * 1)] = 1;
    board[start + bcoord(dir_x * 2, dir_y * 2)] = 1;
    board[start + bcoord(dir_x * 1, dir_y * 2)] = 1;
    board[start + bcoord(dir_x * 0, dir_y * 1)] = 1;
}

void lidka_pred(uint32_t* board, uint32_t x, uint32_t y, uint32_t dir_x = 1, uint32_t dir_y = 1)
{
    auto start = bcoord(x, y);
    board[start + bcoord(dir_x * 0, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 1, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 2, dir_y * 5)] = 1;
    board[start + bcoord(dir_x * 3, dir_y * 4)] = 1;
    board[start + bcoord(dir_x * 3, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 4, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 6, dir_y * 0)] = 1;
    board[start + bcoord(dir_x * 6, dir_y * 1)] = 1;
    board[start + bcoord(dir_x * 7, dir_y * 1)] = 1;
    board[start + bcoord(dir_x * 8, dir_y * 1)] = 1;
    board[start + bcoord(dir_x * 8, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 8, dir_y * 4)] = 1;
    board[start + bcoord(dir_x * 8, dir_y * 5)] = 1;
}

void spaceship(uint32_t* board, uint32_t x, uint32_t y, uint32_t dir_x = 1, uint32_t dir_y = 1)
{
    auto start = bcoord(x, y);
    board[start + bcoord(dir_x * 0, dir_y * 1)] = 1;
    board[start + bcoord(dir_x * 0, dir_y * 2)] = 1;
    board[start + bcoord(dir_x * 0, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 1, dir_y * 0)] = 1;
    board[start + bcoord(dir_x * 1, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 2, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 3, dir_y * 3)] = 1;
    board[start + bcoord(dir_x * 4, dir_y * 0)] = 1;
    board[start + bcoord(dir_x * 4, dir_y * 2)] = 1;
}

struct chunk_spec_t {
   uint32_t startx;
   uint32_t starty;
   uint32_t endx;
   uint32_t endy;
};

void update_chunck_handler(void * param)
{
   chunk_spec_t * chunk = (chunk_spec_t *)param;

   update_board(current_board, next_board, 
         chunk->startx, 
         chunk->starty, 
         chunk->endx, 
         chunk->endy);
}

void render_chunck_handler(void * param)
{
   chunk_spec_t * chunk = (chunk_spec_t *)param;

   render_board(current_board,
         chunk->startx, 
         chunk->starty, 
         chunk->endx, 
         chunk->endy);
}

void game_update_and_render()
{
#if USE_MULTI_THREAD
   chunk_spec_t chunk;
   uint32_t y_step = 64;
   uint32_t x_step = 64;
   uint32_t starty = 1;
   uint32_t endy = min2(starty+y_step, NCELLS_Y);

   // OutputDebugStringA("Update");
   while (starty < NCELLS_Y) {
      uint32_t startx = 1;
      uint32_t endx = min2(startx+x_step, NCELLS_X);
      chunk.starty = starty;
      chunk.endy   = endy;
      while (startx < NCELLS_X) {
         chunk.startx = startx;
         chunk.endx   = endx;

         job_queue_push(update_chunck_handler, &chunk, sizeof(chunk));

         startx = min2(startx + x_step, NCELLS_X);
         endx   = min2(endx + x_step, NCELLS_X);
      }
      starty = min2(starty + y_step, NCELLS_Y);
      endy   = min2(endy + y_step, NCELLS_Y);
   }
   job_queue_wait_until_done();

   swap_boards();

   // OutputDebugStringA("Render");
   starty = 1;
   endy = min2(starty+y_step, NCELLS_Y+1);
   while (starty < NCELLS_Y+1) {
      uint32_t startx = 1;
      uint32_t endx = min2(startx+x_step, NCELLS_X+1);
      chunk.starty = starty;
      chunk.endy   = endy;
      while (startx < NCELLS_X+1) {
         chunk.startx = startx;
         chunk.endx   = endx;

         job_queue_push(render_chunck_handler, &chunk, sizeof(chunk));

         startx = min2(startx + x_step, NCELLS_X+1);
         endx   = min2(endx + x_step, NCELLS_X+1);
      }
      starty = min2(starty + y_step, NCELLS_Y+1);
      endy   = min2(endy + y_step, NCELLS_Y+1);
   }
   job_queue_wait_until_done();
#else
   update_board(current_board, next_board, 1, 1, NCELLS_X, NCELLS_Y);
   swap_boards();
   render_board(current_board, 1, 1, NCELLS_X+1, NCELLS_Y+1);
#endif
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    // Register the window class.
    const wchar_t CLASS_NAME[]  = L"GameOfLifeWindow";
    LARGE_INTEGER StartingTime, EndingTime, ElapsedUsWork, ElapsedUsTotal, ElapsedUsUpdateRender;
    LARGE_INTEGER Frequency;
    LARGE_INTEGER TargerUsPerFrame;
    uint64_t work_accumulator = 0, total_accumulator = 0, update_render_accumulator = 0;
    const uint32_t max_accumulator = 60;
    int32_t accumulator = 0;
    TargerUsPerFrame.QuadPart = (1000 * 1000) / 30;
    QueryPerformanceFrequency(&Frequency); 
    char buffer[BUFFER_SIZE];

    WNDCLASS wc = { };

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.style 	     = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;

    RegisterClass(&wc);

    // Create the window.
    RECT wr = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);

    HWND hwnd = CreateWindowEx(
            0,                              // Optional window styles.
            CLASS_NAME,                     // Window class
            L"Game of Life",                // Window text
            WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, // Window style

            // Size and position
            CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,

            NULL,       // Parent window    
            NULL,       // Menu
            hInstance,  // Instance handle
            NULL        // Additional application data
            );

    if (hwnd == NULL)
    {
        return 0;
    }

    timeBeginPeriod(1);

    job_queue_init();

    Win32AllocateScreenBuffer(WINDOW_WIDTH, WINDOW_HEIGHT);

    ShowWindow(hwnd, nCmdShow);

#if USE_LIDKA_PRED
    lidka_pred(current_board, NCELLS_X / 2, NCELLS_Y / 2);
#else
    const int32_t lut[] = {(5*NCELLS_X)/6, (NCELLS_X / 2 - 5), (NCELLS_X / 2 + 5), (1*NCELLS_X)/6};
    for (uint32_t i = 0; i < 32; i++)
    {
        auto x = lut[i & 0x3];
        auto y = (9*NCELLS_Y / 16) + (((i/4) - 4) * ((12 * NCELLS_Y) / 100));
        auto dx = (i % 2 == 0) ? 1 : -1;
        auto dy = (i < 16) ? -1 : 1;
        glider_gun(current_board, x, y, dx, dy);
        update_board(current_board, next_board, 1, 1, NCELLS_X, NCELLS_Y);
        swap_boards();
    }
    spaceship(current_board, NCELLS_X - 8, NCELLS_Y / 2 + 2);
    spaceship(current_board, 8, NCELLS_Y / 2 - 2, -1, -1);
#endif

    MSG msg = { };
    while (running)
    {
        QueryPerformanceCounter(&StartingTime);

        game_update_and_render();

        QueryPerformanceCounter(&EndingTime);
        ElapsedUsUpdateRender.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
        ElapsedUsUpdateRender.QuadPart *= 1000000;
        ElapsedUsUpdateRender.QuadPart /= Frequency.QuadPart;
        update_render_accumulator += ElapsedUsUpdateRender.QuadPart;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) {
                running = false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        HDC hdc = GetDC(hwnd);
        RECT client_rect;
        GetClientRect(hwnd, &client_rect);
        auto width = client_rect.right - client_rect.left;
        auto height = client_rect.bottom - client_rect.top;
        Win32UpdateWindow(hdc, 0, 0, width, height);

        QueryPerformanceCounter(&EndingTime);
        ElapsedUsWork.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
        ElapsedUsWork.QuadPart *= 1000000;
        ElapsedUsWork.QuadPart /= Frequency.QuadPart;
        work_accumulator += ElapsedUsWork.QuadPart;

        if (ElapsedUsWork.QuadPart > TargerUsPerFrame.QuadPart)
        {
           OutputDebugStringA("Missed frame");
        } else {
           uint32_t sleep_ms = (TargerUsPerFrame.QuadPart - ElapsedUsWork.QuadPart) / 1000;
           Sleep(sleep_ms);
        }

        QueryPerformanceCounter(&EndingTime);
        ElapsedUsTotal.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
        ElapsedUsTotal.QuadPart *= 1000000;
        ElapsedUsTotal.QuadPart /= Frequency.QuadPart;
        total_accumulator += ElapsedUsTotal.QuadPart;

        if (accumulator++ >= max_accumulator)
        {
           StringCbPrintfA(buffer, BUFFER_SIZE, "AVG UpdateRender: %8lld us Work: %8lld us, Total: %8lld us\n", 
                 update_render_accumulator / max_accumulator, 
                 work_accumulator / max_accumulator, 
                 total_accumulator / max_accumulator);
           OutputDebugStringA(buffer);

           update_render_accumulator = 0;
           work_accumulator          = 0;
           total_accumulator         = 0;
           accumulator               = 0;
        }
    }

    timeEndPeriod(1);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                LONG x = ps.rcPaint.left;
                LONG y = ps.rcPaint.top;
                LONG width = ps.rcPaint.right - ps.rcPaint.left;
                LONG height = ps.rcPaint.bottom - ps.rcPaint.top;

                Win32UpdateWindow(hdc, x, y, width, height);

                EndPaint(hwnd, &ps);
            }
            return 0;
        case WM_SIZE:
            // Ignore
            return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
