
#define CGL_LOGGING_ENABLED
#define CGL_IMPLEMENTATION
#include "cgl.h"

#ifndef CHIP8_NO_UI

#pragma warning(push, 0)
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL3_IMPLEMENTATION
#define NK_KEYSTATE_BASED_INPUT
#include "nuklear.h"
#include "nuklear_glfw_gl4.h"
#pragma warning(pop)

#endif


#ifndef CHIP8_NO_UI
#define chip8_log(...) sprintf(buffer, __VA_ARGS__)
#endif

#define chip8_rand rand

static char buffer[4096];


#define CHIP8_IMPLEMENTATION
#include "chip8.h"

#define CHIP8_NO_UI

#ifndef CHIP8_NO_UI

static struct {
    struct nk_context* ctx;
    struct nk_colorf bg;
    struct nk_glfw glfw;
} nuklear_data;

#endif

static struct
{
    float scale_x;
    float scale_y;
    float offset_x;
    float offset_y;
    CGL_tilemap* tilemap;
} tilemap_data;


static struct chip8 vm;
static chip8_u8 input[16];
static bool has_exited = false;
static bool is_running = false;
static char path[4096];



#define TILE_COUNT_X 64
#define TILE_COUNT_Y 32

bool load_rom(const char* path)
{
    chip8_u8* data = NULL;

    FILE* file = fopen(path, "rb");
    if(file == NULL) return false;

    fseek(file, 0, SEEK_END);
    chip8_u16 size = (chip8_u16)ftell(file);
    fseek(file, 0, SEEK_SET);
    
    data = (chip8_u8*)malloc(size);

    if(data == NULL) { fclose(file); return false; }

    fread(data, 1, size, file);
    fclose(file);

    if(!chip8_load_rom(&vm, data, size)) { free(data); return false;}

    free(data);

    is_running = false;
    has_exited = false;

#ifdef CHIP8_NO_UI
    is_running = true;
#endif

    return true;
}

void drop_func(GLFWwindow* window, int path_count, const char** paths)
{
    path[0] = '\0';
    strcat(path, paths[0]);
    if(!load_rom(path)) printf("Unable to load ROM %s", path);
}

void update_input(CGL_window* window)
{
    input[0] = CGL_window_get_key(window, CGL_KEY_1) == CGL_PRESS;
    input[1] = CGL_window_get_key(window, CGL_KEY_2) == CGL_PRESS;
    input[2] = CGL_window_get_key(window, CGL_KEY_3) == CGL_PRESS;
    input[3] = CGL_window_get_key(window, CGL_KEY_4) == CGL_PRESS;
    input[4] = CGL_window_get_key(window, CGL_KEY_Q) == CGL_PRESS;
    input[5] = CGL_window_get_key(window, CGL_KEY_W) == CGL_PRESS;
    input[6] = CGL_window_get_key(window, CGL_KEY_E) == CGL_PRESS;
    input[7] = CGL_window_get_key(window, CGL_KEY_R) == CGL_PRESS;
    input[8] = CGL_window_get_key(window, CGL_KEY_A) == CGL_PRESS;
    input[9] = CGL_window_get_key(window, CGL_KEY_S) == CGL_PRESS;
    input[10] = CGL_window_get_key(window, CGL_KEY_D) == CGL_PRESS;
    input[11] = CGL_window_get_key(window, CGL_KEY_F) == CGL_PRESS;
    input[12] = CGL_window_get_key(window, CGL_KEY_Z) == CGL_PRESS;
    input[13] = CGL_window_get_key(window, CGL_KEY_X) == CGL_PRESS;
    input[14] = CGL_window_get_key(window, CGL_KEY_C) == CGL_PRESS;
    input[15] = CGL_window_get_key(window, CGL_KEY_V) == CGL_PRESS;
}


int main(int argc, char** argv)
{
    if(argc == 2)
    {
        path[0] = '\0';
        strcat(path, argv[1]);
        if(!load_rom(path)) printf("Unable to load ROM %s", path);
    }

    srand((uint32_t)time(NULL));
    if(!CGL_init()) return -1;
    CGL_window* main_window = CGL_window_create(640, 340, "chip8");
    if(!main_window) return -1;
    CGL_window_make_context_current(main_window);
    if(!CGL_gl_init()) return -1;
    CGL_framebuffer* default_framebuffer = CGL_framebuffer_create_from_default(main_window);   

    glfwSetDropCallback(CGL_window_get_glfw_handle(main_window), drop_func);

    tilemap_data.offset_x = 0.0f;
    tilemap_data.offset_y = 0.0f;
    tilemap_data.scale_x = 1.0f;
    tilemap_data.scale_y = 1.0f;
    tilemap_data.tilemap = CGL_tilemap_create(TILE_COUNT_X, TILE_COUNT_Y, 10, 10, 0);
    memset(input, 0, sizeof(input));

#ifndef CHIP8_NO_UI

    // setup nuklear
    nuklear_data.ctx = nk_glfw3_init(&nuklear_data.glfw, CGL_window_get_glfw_handle(main_window), NK_GLFW3_INSTALL_CALLBACKS);

    {
        struct nk_font_atlas* atlas;
        nk_glfw3_font_stash_begin(&nuklear_data.glfw, &atlas);
        nk_glfw3_font_stash_end(&nuklear_data.glfw);
    }

#endif

    CGL_window_resecure_callbacks(main_window);
    CGL_tilemap_set_auto_upload(tilemap_data.tilemap, false);

    while(!CGL_window_should_close(main_window))
    { 
        if(!has_exited && is_running)
        {
            chip8_update_timer(&vm);
            has_exited = !chip8_cycle(&vm, input);
            // ideally you should play a buzzer sound
            // if vm.ST is greater than 0 but i am
            // not implementing sound at all here
            // altough it can be easily done by
            // printf("\a");
            for(int i = 0 ; i < 32 ; i++)
            {
                for(int j = 0 ; j < 64 ; j++)
                {
                    float color = vm.display[i * 64 + j] == 1 ? 0.5f : 0.0f;
                    CGL_tilemap_set_tile_color(tilemap_data.tilemap, j, 31 - i, color, color, color);
                }
            }
            CGL_tilemap_upload(tilemap_data.tilemap);
        }

        {
           CGL_framebuffer_bind(default_framebuffer);
            CGL_gl_clear(0.2f, 0.2f, 0.2f, 1.0f);
            int rx = 0, ry = 0;
            CGL_framebuffer_get_size(default_framebuffer, &rx, &ry);
            CGL_tilemap_render(tilemap_data.tilemap, tilemap_data.scale_x, tilemap_data.scale_y, tilemap_data.offset_x, tilemap_data.offset_y, NULL);
        }

        CGL_window_poll_events(main_window);

        update_input(main_window);

#ifndef CHIP8_NO_UI
        nk_glfw3_new_frame(&nuklear_data.glfw);

        static char buffer2[4096];
        if (nk_begin(nuklear_data.ctx, "Control", nk_rect(50, 50, 230, 250),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(nuklear_data.ctx, 30, 2);
            nk_label(nuklear_data.ctx, "Program Active : ", NK_TEXT_ALIGN_LEFT);
            nk_label(nuklear_data.ctx, ( has_exited ? "No" : "Yes"), NK_TEXT_ALIGN_LEFT);

            nk_layout_row_dynamic(nuklear_data.ctx, 30, 2);
            nk_label(nuklear_data.ctx, "Program Running : ", NK_TEXT_ALIGN_LEFT);
            nk_label(nuklear_data.ctx, ( is_running ? "No" : "Yes"), NK_TEXT_ALIGN_LEFT);

            nk_layout_row_dynamic(nuklear_data.ctx, 30, 2);
            nk_label(nuklear_data.ctx, "ROM : ", NK_TEXT_ALIGN_LEFT);
            nk_label(nuklear_data.ctx, ( path == NULL ? "No ROM" : path), NK_TEXT_ALIGN_LEFT);

            sprintf(buffer2, "Curent Instruction: %s", buffer);
            nk_layout_row_dynamic(nuklear_data.ctx, 30, 1);
            nk_label(nuklear_data.ctx, buffer2, NK_TEXT_ALIGN_LEFT);

            nk_layout_row_dynamic(nuklear_data.ctx, 30, 3);
            if (nk_button_label(nuklear_data.ctx, "Start")) is_running = true;
            if (nk_button_label(nuklear_data.ctx, "Pause")) is_running = false;
            if (nk_button_label(nuklear_data.ctx, "Step")) 
            {
                if(!is_running && !has_exited)
                {
                    chip8_update_timer(&vm);
                    has_exited = !chip8_cycle(&vm);
                    for(int i = 0 ; i < 32 ; i++)
                    {
                        for(int j = 0 ; j < 64 ; j++)
                        {
                            float color = vm.display[i * 64 + j] == 1 ? 0.5f : 0.0f;
                            CGL_tilemap_set_tile_color(tilemap_data.tilemap, j, 31 - i, color, color, color);
                        }
                    }
                }
            }

            nk_layout_row_dynamic(nuklear_data.ctx, 30, 1);
            sprintf(buffer2, "I: %d PC: %d  ST: %d  DT: %d  SP: %d", vm.I, vm.PC, vm.ST, vm.DT, vm.SP);
            nk_label(nuklear_data.ctx, buffer2, NK_TEXT_ALIGN_LEFT);


            nk_layout_row_dynamic(nuklear_data.ctx, 30, 1);
            if (nk_button_label(nuklear_data.ctx, "Reset"))
            {
                if(!load_rom(path)) printf("Unable to load ROM %s", path);
            }

            nk_layout_row_dynamic(nuklear_data.ctx, 30, 1);
            nk_label(nuklear_data.ctx, "Registors : ", NK_TEXT_ALIGN_LEFT);
            for(chip8_u8 i = 0 ; i < 16 ; i+=2)
            {
                sprintf(buffer2, "V%c: %d     V%c: %d", chip8__to_hex(i), vm.regs[i], chip8__to_hex(i + 1), vm.regs[i + 1]);
                nk_layout_row_dynamic(nuklear_data.ctx, 30, 1);
                nk_label(nuklear_data.ctx, buffer2, NK_TEXT_ALIGN_LEFT);
            }


            nk_layout_row_dynamic(nuklear_data.ctx, 30, 1);
            if (nk_button_label(nuklear_data.ctx, "Exit")) break;


        }
        nk_end(nuklear_data.ctx);
        nk_glfw3_render(&nuklear_data.glfw, NK_ANTI_ALIASING_ON, 1024 * 512, 1024 * 128);
#endif

        CGL_window_swap_buffers(main_window);
        if(CGL_window_get_key(main_window, CGL_KEY_ESCAPE) == CGL_PRESS) break;
    }

#ifndef CHIP8_NO_UI
    nk_glfw3_shutdown(&nuklear_data.glfw);
#endif

    CGL_tilemap_destroy(tilemap_data.tilemap);
    CGL_framebuffer_destroy(default_framebuffer);
    CGL_gl_shutdown();
    CGL_window_destroy(main_window);
    CGL_shutdown();

    return EXIT_SUCCESS;
}