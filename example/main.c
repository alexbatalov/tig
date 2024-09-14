#include <windows.h>

#include <tig/tig.h>

static struct {
    int x;
    int y;
    const char* str;
    tig_button_handle_t button_handle;
} buttons[] = {
    { 410, 143, "Single Player", TIG_BUTTON_HANDLE_INVALID },
    { 410, 193, "MultiPlayer", TIG_BUTTON_HANDLE_INVALID },
    { 410, 243, "Options", TIG_BUTTON_HANDLE_INVALID },
    { 410, 293, "Credits", TIG_BUTTON_HANDLE_INVALID },
    { 410, 343, "Exit Game", TIG_BUTTON_HANDLE_INVALID },
};

static tig_window_handle_t window_handle = TIG_WINDOW_HANDLE_INVALID;
static tig_font_handle_t font = TIG_FONT_HANDLE_INVALID;
static tig_font_handle_t hover_font = TIG_FONT_HANDLE_INVALID;
static tig_sound_handle_t music_handle = TIG_SOUND_HANDLE_INVALID;

static int art_file_path_resolver(tig_art_id_t art_id, char* path)
{
    int type = tig_art_type(art_id);
    int num = tig_art_num_get(art_id);

    switch (type) {
    case TIG_ART_TYPE_INTERFACE:
        switch (num) {
        case 329:
            strcpy(path, "art\\interface\\mainmenuback.art");
            return TIG_OK;
        case 327:
            strcpy(path, "art\\interface\\morph30font.art");
            return TIG_OK;
        }
        break;
    }

    path[0] = '\0';
    return TIG_ERR_16;
}

static int sound_file_path_resolver(int sound_id, char* path)
{
    switch (sound_id) {
    case 3000:
        strcpy(path, "sound\\button_medium.wav");
        return TIG_OK;
    case 3001:
        strcpy(path, "sound\\button_medium_release.wav");
        return TIG_OK;
    case 3026:
        // The 3026 is actually `morphtext_hover.wav`, but it looks like there
        // is nothing but silence.
        strcpy(path, "sound\\morphtext_click.wav");
        return TIG_OK;
    }

    path[0] = '\0';
    return TIG_ERR_16;
}

static void do_movie(const char* movie)
{
    char path[TIG_MAX_PATH];

    if (tig_file_extract(movie, path)) {
        tig_movie_play(path, 0, 0);
    }
}

static void do_music(const char* music)
{
    tig_sound_create(&music_handle, TIG_SOUND_TYPE_MUSIC);
    tig_sound_set_volume(music_handle, 100);
    tig_sound_play_streamed_indefinitely(music_handle, music, 250, TIG_SOUND_HANDLE_INVALID);
}

static void refresh_button_text(int index, bool hovering)
{
    TigFont font_desc;
    TigRect text_rect;
    TigArtBlitInfo art_blit_info;

    tig_font_push(hovering ? hover_font : font);

    // Measure text.
    font_desc.str = buttons[index].str;
    font_desc.width = 0;
    font_desc.height = 0;
    sub_535390(&font_desc);

    text_rect.x = buttons[index].x - font_desc.width / 2;
    text_rect.y = buttons[index].y - font_desc.height / 2;
    text_rect.width = font_desc.width;
    text_rect.height = font_desc.height;

    // Blit background.
    art_blit_info.flags = 0;
    tig_art_interface_id_create(329, 0, 0, 0, &(art_blit_info.art_id));
    art_blit_info.src_rect = &text_rect;
    art_blit_info.dst_rect = &text_rect;
    tig_window_blit_art(window_handle, &art_blit_info);

    // Blit text over fresh background.
    tig_window_text_write(window_handle, buttons[index].str, &text_rect);
    tig_font_pop();
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    TigInitInfo init_info;
    init_info.texture_width = 1024;
    init_info.texture_height = 1024;
    init_info.flags = TIG_INITIALIZE_3D_HARDWARE_DEVICE | TIG_INITIALIZE_3D_SOFTWARE_DEVICE | TIG_INITIALIZE_VIDEO_MEMORY;

    char* pch = lpCmdLine;
    while (*pch != '\0') {
        *pch = (char)(unsigned char)tolower((unsigned char)*pch);
        pch++;
    }

    if (strstr(lpCmdLine, "-fps") != NULL) {
        init_info.flags |= TIG_INITIALIZE_FPS;
    }

    if (strstr(lpCmdLine, "-nosound") != NULL) {
        init_info.flags |= TIG_INITIALIZE_NO_SOUND;
    }

    if (strstr(lpCmdLine, "-no3d") != NULL) {
        init_info.flags &= ~(TIG_INITIALIZE_ANY_3D | TIG_INITIALIZE_VIDEO_MEMORY);
    }

    if (strstr(lpCmdLine, "-3dref") != NULL) {
        init_info.flags &= ~(TIG_INITIALIZE_3D_HARDWARE_DEVICE | TIG_INITIALIZE_3D_SOFTWARE_DEVICE);
        init_info.flags |= TIG_INITIALIZE_3D_REF_DEVICE;
    }

    if (strstr(lpCmdLine, "-doublebuffer") != NULL) {
        init_info.flags |= TIG_INITIALIZE_DOUBLE_BUFFER;
    }

    // Testing windowed mode.
    init_info.flags |= TIG_INITIALIZE_WINDOWED;

    init_info.width = 800;
    init_info.height = 600;

    // Testing stretched blitting.
    // init_info.width *= 2;
    // init_info.height *= 2;

    init_info.bpp = 32;

    // Testing other bpp modes.
    // init_info.bpp = 24;
    // init_info.bpp = 16;

    init_info.instance = hInstance;
    init_info.default_window_proc = DefWindowProcA;
    init_info.art_file_path_resolver = art_file_path_resolver;
    init_info.sound_file_path_resolver = sound_file_path_resolver;

    // Initialize TIG.
    if (tig_init(&init_info) != TIG_OK) {
        return EXIT_FAILURE;
    }

    // Add assets to resource manager.
    tig_file_repository_add("arcanum1.dat");
    tig_file_repository_add("arcanum2.dat");
    tig_file_repository_add("arcanum3.dat");
    tig_file_repository_add("arcanum4.dat");
    tig_file_repository_add("modules\\arcanum");

    // Play startup movie. Observe that movie does not require window at all.
    // Instead it is rendered directly to the screen surface.
    //
    // NOTE: Causes switching to 256 color mode (flickering in windowed mode).
    // do_movie("movies\\SierraLogo.bik");

    // Create root window.
    TigWindowData window_data;
    window_data.rect.x = 0;
    window_data.rect.y = 0;
    window_data.rect.width = init_info.width;
    window_data.rect.height = init_info.height;
    window_data.background_color = tig_color_make(0, 0, 255);
    window_data.message_filter = NULL;
    window_data.flags = 0;
    tig_window_create(&window_data, &window_handle);

    // Blit background.
    TigRect src_rect = { 0, 0, 800, 600 };
    TigRect dst_rect = { 0, 0, window_data.rect.width, window_data.rect.height };
    TigArtBlitInfo art_blit_info;
    art_blit_info.flags = 0;
    tig_art_interface_id_create(329, 0, 0, 0, &(art_blit_info.art_id));
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;
    tig_window_blit_art(window_handle, &art_blit_info);

    // Create a pair of fonts - normal/hovered.
    TigFont font_desc;
    tig_art_interface_id_create(327, 0, 0, 0, &(font_desc.art_id));
    font_desc.flags = TIG_FONT_SHADOW;
    font_desc.str = NULL;
    font_desc.color = tig_color_make(100, 0, 0);
    tig_font_create(&font_desc, &font);

    font_desc.color = tig_color_make(240, 15, 15);
    tig_font_create(&font_desc, &hover_font);

    // Create main menu buttons. The buttons themselves have no art, instead
    // their art is text which is drawn in `refresh_button_text`.
    tig_font_push(font);
    for (int btn = 0; btn < 5; btn++) {
        // Measure button text.
        TigFont font_desc;
        font_desc.str = buttons[btn].str;
        font_desc.width = 0;
        font_desc.height = 0;
        sub_535390(&font_desc);

        TigRect text_rect;
        text_rect.x = buttons[btn].x - font_desc.width / 2;
        text_rect.y = buttons[btn].y - font_desc.height / 2;
        text_rect.width = font_desc.width;
        text_rect.height = font_desc.height;

        // Setup button data.
        TigButtonData button_data;
        button_data.flags = 0;
        button_data.window_handle = window_handle;
        button_data.art_id = TIG_ART_ID_INVALID;
        button_data.x = text_rect.x;
        button_data.y = text_rect.y;
        button_data.width = text_rect.width;
        button_data.height = text_rect.height;
        button_data.mouse_down_snd_id = 3000;
        button_data.mouse_up_snd_id = 3001;
        button_data.mouse_enter_snd_id = 3026;
        button_data.mouse_exit_snd_id = -1;
        tig_button_create(&button_data, &(buttons[btn].button_handle));

        // Blit button text.
        refresh_button_text(btn, false);
    }
    tig_font_pop();

    // Start music.
    do_music("sound\\music\\arcanum.mp3");

    // Main game loop.
    TigMessage msg;
    bool done = false;
    while (!done) {
        // Update library state.
        tig_ping();

        // Here goes pinging game modules.
        // gamelib_ping();

        // Update dirty rects and flush to screen.
        tig_window_display();

        // Process events.
        if (tig_message_dequeue(&msg) == TIG_OK) {
            if (msg.type == TIG_MESSAGE_BUTTON) {
                int btn = -1;
                for (int index = 0; index < 5; index++) {
                    if (buttons[index].button_handle == msg.data.button.button_handle) {
                        btn = index;
                        break;
                    }
                }
                if (btn != -1) {
                    switch (msg.data.button.state) {
                    case TIG_BUTTON_STATE_MOUSE_INSIDE:
                        refresh_button_text(btn, true);
                        break;
                    case TIG_BUTTON_STATE_MOUSE_OUTSIDE:
                        refresh_button_text(btn, false);
                        break;
                    case TIG_BUTTON_STATE_RELEASED:
                        tig_debug_printf("Btn: #%d\n", btn);
                        if (btn == 4) {
                            tig_message_post_quit(0);
                        }
                        break;
                    }
                }
            } else if (msg.type == TIG_MESSAGE_KEYBOARD) {
                if (msg.data.keyboard.key == DIK_ESCAPE
                    && msg.data.keyboard.pressed == 0) {
                    done = true;
                }
            } else if (msg.type == TIG_MESSAGE_QUIT) {
                done = true;
            }
        }
    }

    // Shutdown TIG.
    tig_exit();

    // Dump memory stats for further analysis.
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_ALL_BLOCKS);

    return EXIT_SUCCESS;
}
