#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define ENEMY_COUNT 3
#define LEVEL1_WIDTH 19
#define LEVEL1_HEIGHT 9
#define FONTBANK_SIZE 16

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL_mixer.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"
#include "Map.h"

// ————— GAME STATE ————— //
struct GameState
{
    Entity* player;
    Entity* enemies;

    Map* map;

    Mix_Music* bgm;
    Mix_Chunk* jump_sfx;
};

// ————— CONSTANTS ————— //
const int   WINDOW_WIDTH = 640,
WINDOW_HEIGHT = 480;

//105, 215, 255
const float BG_RED = 105.0f / 255.0f;   // Softer R value for cute sky blue
const float BG_GREEN = 215.0f / 255.0f; // Softer G value for cute sky blue
const float BG_BLUE = 255.0f / 255.0f;  // Softer B value with a hint of pink for a cute sky blue
const float BG_OPACITY = 1.0f;          // Full opacity

const int   VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char GAME_WINDOW_NAME[] = "Rise of AI";

const char  V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;

const char  SPRITESHEET_FILEPATH[] = "assets/images/player_sprite.png",
    MAP_TILESET_FILEPATH[] = "assets/images/tileset.png",
    BGM_FILEPATH[] = "assets/audio/dooblydoo.mp3",
    JUMP_SFX_FILEPATH[] = "assets/audio/jump_sfx.wav",
    ENEMY_FILEPATH[] = "assets/images/enemies.png",
    FONT_FILEPATH[] = "assets/fonts/square_font.png";


const int NUMBER_OF_TEXTURES = 1;
const GLint LEVEL_OF_DETAIL = 0;
const GLint TEXTURE_BORDER = 0;

unsigned int LEVEL_1_DATA[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2,
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
};

// enemy starting positions
const float ENEMY_START_POS[][2] = { {6.0f, -5.0f}, {4.0f, -2.0f}, {15.0f, -3.0f} };
//        g_game_state.enemies[i].set_position(glm::vec3((float)i + 2.0f, -5.0f, 0.0f));

// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
bool g_game_is_running = true;
bool g_game_over = false, g_game_won = true;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float   g_previous_ticks = 0.0f,
g_accumulator = 0.0f;

void shutdown();

// ————— GENERAL FUNCTIONS ————— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint texture_id;
    glGenTextures(NUMBER_OF_TEXTURES, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return texture_id;
}

void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text, float screen_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for each character
    // Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their position
        //    relative to the whole sentence)
        int spritesheet_index = (int)text[i];  // ascii value of character
        float offset = (screen_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
            });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    program->SetModelMatrix(model_matrix);
    glUseProgram(program->programID);

    glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices.data());
    glEnableVertexAttribArray(program->positionAttribute);
    glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texture_coordinates.data());
    glEnableVertexAttribArray(program->texCoordAttribute);

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    glDisableVertexAttribArray(program->positionAttribute);
    glDisableVertexAttribArray(program->texCoordAttribute);
}

void initialise()
{
    // ————— GENERAL ————— //
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow(GAME_WINDOW_NAME,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif

    // ————— VIDEO SETUP ————— //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.Load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.SetProjectionMatrix(g_projection_matrix);
    g_shader_program.SetViewMatrix(g_view_matrix);

    glUseProgram(g_shader_program.programID);

    glClearColor(BG_RED, BG_GREEN, BG_BLUE, BG_OPACITY);

    // ————— MAP SET-UP ————— //
    GLuint map_texture_id = load_texture(MAP_TILESET_FILEPATH);
    g_game_state.map = new Map(LEVEL1_WIDTH, LEVEL1_HEIGHT, LEVEL_1_DATA, map_texture_id, 1.0f, 3, 1);

    // ————— PLAYER SET-UP ————— //
    // Existing
    g_game_state.player = new Entity();
    g_game_state.player->set_entity_type(PLAYER);
    g_game_state.player->set_position(glm::vec3(1.0f, -5.0f, 0.0f));
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_game_state.player->set_speed(2.5f);
    g_game_state.player->set_acceleration(glm::vec3(0.0f, -9.81f, 0.0f));
    g_game_state.player->m_texture_id = load_texture(SPRITESHEET_FILEPATH);

    // Walking
    g_game_state.player->m_walking[g_game_state.player->LEFT] = new int[4] { 1, 5, 9, 13 };
    g_game_state.player->m_walking[g_game_state.player->RIGHT] = new int[4] { 3, 7, 11, 15 };
    g_game_state.player->m_walking[g_game_state.player->UP] = new int[4] { 2, 6, 10, 14 };
    g_game_state.player->m_walking[g_game_state.player->DOWN] = new int[4] { 0, 4, 8, 12 };

    g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];  // start George looking left
    g_game_state.player->m_animation_frames = 4;
    g_game_state.player->m_animation_index = 0;
    g_game_state.player->m_animation_time = 0.0f;
    g_game_state.player->m_animation_cols = 4;
    g_game_state.player->m_animation_rows = 4;
    g_game_state.player->set_height(1.0f);
    g_game_state.player->set_width(0.6f);

    // --- ENEMY SET-UP --- //
    g_game_state.enemies = new Entity [ENEMY_COUNT];
    for (size_t i = 0; i < 3; i++)
    {
        g_game_state.enemies[i].set_entity_type(ENEMY);
        // starting position ↓
        g_game_state.enemies[i].set_position(glm::vec3(ENEMY_START_POS[i][0], ENEMY_START_POS[i][1], 0.0f));

        //behaviors
        switch (i)
        {
        case 0:
            g_game_state.enemies[i].set_ai_type(GUARD);
            g_game_state.enemies[i].set_ai_state(IDLE);
            g_game_state.enemies[i].set_movement(glm::vec3(0.0f));
            g_game_state.enemies[i].set_speed(1.0f);
            break;
        case 1:
            g_game_state.enemies[i].set_ai_type(JUMPER);
            g_game_state.enemies[i].set_ai_state(JUMPING);
            g_game_state.enemies[i].m_jumping_power = 3.0f;
            g_game_state.enemies[i].set_movement(glm::vec3(0.0f));
            g_game_state.enemies[i].set_speed(1.0f);
            break;
        case 2:
            g_game_state.enemies[i].set_ai_type(WALKER);
            g_game_state.enemies[i].set_ai_state(IDLE);
            g_game_state.enemies[i].set_movement(glm::vec3(0.0f));
            g_game_state.enemies[i].set_speed(1.0f);
            break;
        }

        g_game_state.enemies[i].set_acceleration(glm::vec3(0.0f, -9.81f, 0.0f));
        g_game_state.enemies[i].m_texture_id = load_texture(ENEMY_FILEPATH);
                               
        // Walking             
        g_game_state.enemies[i].m_walking[g_game_state.enemies[i].LEFT] = new int[4] { 1, 5, 9, 13 };
        g_game_state.enemies[i].m_walking[g_game_state.enemies[i].RIGHT] = new int[4] { 3, 7, 11, 15 };
        g_game_state.enemies[i].m_walking[g_game_state.enemies[i].UP] = new int[4] { 2, 6, 10, 14 };
        g_game_state.enemies[i].m_walking[g_game_state.enemies[i].DOWN] = new int[4] { 0, 4, 8, 12 };
                               
        g_game_state.enemies[i].m_animation_indices = g_game_state.enemies[i].m_walking[g_game_state.enemies[i].RIGHT]; //start looking right
        g_game_state.enemies[i].m_animation_frames = 4;
        g_game_state.enemies[i].m_animation_index = 0;
        g_game_state.enemies[i].m_animation_time = 0.0f;
        g_game_state.enemies[i].m_animation_cols = 4;
        g_game_state.enemies[i].m_animation_rows = 4;
        g_game_state.enemies[i].set_height(1.0f);
        g_game_state.enemies[i].set_width(1.0f);
    }

    // Jumping
    g_game_state.player->m_jumping_power = 7.0f;


    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

    g_game_state.bgm = Mix_LoadMUS(BGM_FILEPATH);
    Mix_PlayMusic(g_game_state.bgm, -1);
    Mix_VolumeMusic(MIX_MAX_VOLUME / 16.0f);
    Mix_HaltMusic();

    g_game_state.jump_sfx = Mix_LoadWAV(JUMP_SFX_FILEPATH);

    // ————— BLENDING ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;

            case SDLK_SPACE:
                // Jump
                if (g_game_state.player->m_collided_bottom && !g_game_over)
                {
                    g_game_state.player->m_is_jumping = true;
                    Mix_PlayChannel(-1, g_game_state.jump_sfx, 0);
                }
                break;

            default:
                break;
            }

        default:
            break;
        }
    }
    if (!g_game_over) {
        const Uint8* key_state = SDL_GetKeyboardState(NULL);

        if (key_state[SDL_SCANCODE_LEFT])
        {
            g_game_state.player->move_left();
            g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->LEFT];
        }
        else if (key_state[SDL_SCANCODE_RIGHT])
        {
            g_game_state.player->move_right();
            g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];
        }

        // This makes sure that the player can't move faster diagonally
        if (glm::length(g_game_state.player->get_movement()) > 1.0f)
        {
            g_game_state.player->set_movement(glm::normalize(g_game_state.player->get_movement()));
        }
    }
}

void update()
{
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_accumulator;

    if (delta_time < FIXED_TIMESTEP)
    {
        g_accumulator = delta_time;
        return;
    }

    while (delta_time >= FIXED_TIMESTEP)
    {
        /* player */
        g_game_state.player->update(FIXED_TIMESTEP, g_game_state.player, g_game_state.enemies, ENEMY_COUNT, g_game_state.map);
        /* enemy */
        for (int i = 0; i < ENEMY_COUNT; i++) 
            g_game_state.enemies[i].update(FIXED_TIMESTEP, g_game_state.player, NULL, 0, g_game_state.map);

        delta_time -= FIXED_TIMESTEP;
    }

    g_accumulator = delta_time;

    // -- Check for Win/Loss -- //
    // are all enemies dead?
    if (!g_game_state.player->get_active_status()) { // player is dead
        g_game_over = true;
        g_game_won = false;
    }else{
        int enemies_alive = 0;
        for (int i = 0; i < ENEMY_COUNT; i++)
            if (g_game_state.enemies[i].get_active_status()) enemies_alive++;
        if (enemies_alive == 0) {
            g_game_over = true;
            g_game_won = true;
        }
    }
    
    // --  CAMERA POSITION -- //
    g_view_matrix = glm::mat4(1.0f);
    g_view_matrix = glm::translate(g_view_matrix, glm::vec3(-g_game_state.player->get_position().x, -2-g_game_state.player->get_position().y, 0.0f));
}

void render()
{
    g_shader_program.SetViewMatrix(g_view_matrix);

    glClear(GL_COLOR_BUFFER_BIT);

    g_game_state.player->render(&g_shader_program);
    
    for (int i = 0; i < ENEMY_COUNT; i++) g_game_state.enemies[i].render(&g_shader_program);

    g_game_state.map->render(&g_shader_program);

    if (g_game_over) {
        std::string message = "...";
        if (g_game_won) message = "YOU WIN!";
        else message = "YOU LOSE!";

        draw_text(&g_shader_program, load_texture(FONT_FILEPATH), message, 0.75f, 0.0f, glm::vec3(g_game_state.player->get_position().x - 2.5f, g_game_state.player->get_position().y + 2.0f, 0.0f));
    }
    SDL_GL_SwapWindow(g_display_window);
}

void shutdown()
{
    SDL_Quit();

    delete[] g_game_state.enemies;
    delete    g_game_state.player;
    delete    g_game_state.map;
    Mix_FreeChunk(g_game_state.jump_sfx);
    Mix_FreeMusic(g_game_state.bgm);
}

// ————— GAME LOOP ————— //
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}
