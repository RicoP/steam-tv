#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cef_base.h"
#include "cef_app.h"
#include "cef_browser.h"
#include "cef_client.h"
#include "cef_command_line.h"
#include "cef_display_handler.h"
#include "cef_load_handler.h"
#include "cef_request.h"
#include "cef_request_handler.h"
#include "cef_render_handler.h"
#include "include/internal/cef_logging_internal.h"
#include "wrapper/cef_helpers.h"
#include "raylib.h"

struct BrowserSurface {
    int width;
    int height;
    unsigned char *pixels;
    int dirty;
    CefRefPtr<CefBrowser> browser;
};

static int g_has_start_url_argument = 0;
static char g_start_url_argument[4096];
static const char *BROWSER_SWIZZLE_FRAGMENT_SHADER =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "void main()\n"
    "{\n"
    "    vec4 bgra = texture(texture0, fragTexCoord);\n"
    "    finalColor = bgra.bgra*colDiffuse*fragColor;\n"
    "}\n";

static int format_text(char *buffer, size_t buffer_size, const char *format, ...)
{
    va_list args;
    int result;

    if (!buffer || buffer_size == 0) {
        return 0;
    }

    va_start(args, format);
    result = vsnprintf(buffer, buffer_size, format, args);
    va_end(args);

    if (result < 0 || (size_t)result >= buffer_size) {
        buffer[0] = 0;
        return 0;
    }

    return 1;
}

static void log_steam_tv_message(int severity, const char *format, ...)
{
    char log_message[8192];
    va_list args;
    int result;

    va_start(args, format);
    result = vsnprintf(log_message, sizeof(log_message), format, args);
    va_end(args);

    if (result < 0) {
        cef_log(__FILE__, __LINE__, LOGSEVERITY_ERROR, "[steam_tv] log formatting failed");
        return;
    }

    if ((size_t)result >= sizeof(log_message)) {
        log_message[sizeof(log_message) - 1] = 0;
        cef_log(__FILE__, __LINE__, LOGSEVERITY_WARNING, "[steam_tv] log message truncated");
    }

    cef_log(__FILE__, __LINE__, severity, log_message);
}

static void browser_surface_init(struct BrowserSurface *surface)
{
    surface->width = 0;
    surface->height = 0;
    surface->pixels = 0;
    surface->dirty = 0;
}

static void browser_surface_shutdown(BrowserSurface *surface)
{
    if (surface->pixels) {
        free(surface->pixels);
    }

    surface->pixels = 0;
    surface->width = 0;
    surface->height = 0;
    surface->dirty = 0;
    surface->browser = nullptr;
}

static int browser_surface_resize(BrowserSurface *surface, int width, int height)
{
    int byte_count;
    unsigned char *new_pixels;

    if (width <= 0 || height <= 0) {
        return 0;
    }

    if (surface->width == width && surface->height == height && surface->pixels) {
        return 1;
    }

    byte_count = width * height * 4;
    new_pixels = (unsigned char *)realloc(surface->pixels, (size_t)byte_count);
    if (!new_pixels) {
        return 0;
    }

    surface->pixels = new_pixels;
    surface->width = width;
    surface->height = height;
    memset(surface->pixels, 0, (size_t)byte_count);
    surface->dirty = 1;

    return 1;
}

static void browser_surface_copy_bgra(BrowserSurface *surface,
                                      const void *buffer,
                                      int width,
                                      int height)
{
    int byte_count;

    if (!browser_surface_resize(surface, width, height)) {
        return;
    }

    byte_count = width * height * 4;
    memcpy(surface->pixels, buffer, (size_t)byte_count);
    surface->dirty = 1;
}

class BrowserClient : public CefClient,
                      public CefDisplayHandler,
                      public CefRenderHandler,
                      public CefLoadHandler,
                      public CefRequestHandler,
                      public CefLifeSpanHandler {
public:
    BrowserClient(BrowserSurface *surface, int width, int height)
    {
        surface_ = surface;
        view_width_ = width;
        view_height_ = height;
    }

    CefRefPtr<CefRenderHandler> GetRenderHandler() override
    {
        return this;
    }

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override
    {
        return this;
    }

    CefRefPtr<CefLoadHandler> GetLoadHandler() override
    {
        return this;
    }

    CefRefPtr<CefRequestHandler> GetRequestHandler() override
    {
        return this;
    }

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override
    {
        return this;
    }

    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override
    {
        rect = CefRect(0, 0, view_width_, view_height_);
    }

    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList &dirtyRects,
                 const void *buffer,
                 int width,
                 int height) override
    {
        if (type != PET_VIEW) {
            return;
        }

        browser_surface_copy_bgra(surface_, buffer, width, height);
    }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override
    {
        CEF_REQUIRE_UI_THREAD();
        surface_->browser = browser;
    }

    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                          cef_log_severity_t level,
                          const CefString& message,
                          const CefString& source,
                          int line) override
    {
        log_steam_tv_message(LOGSEVERITY_INFO,
                             "[steam_tv] console level=%d source=%s line=%d message=%s",
                             (int)level,
                             source.ToString().c_str(),
                             line,
                             message.ToString().c_str());

        return false;
    }

    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override
    {
        log_steam_tv_message(LOGSEVERITY_INFO,
                             "[steam_tv] before_browse main=%d gesture=%d redirect=%d url=%s",
                             frame->IsMain() ? 1 : 0,
                             user_gesture ? 1 : 0,
                             is_redirect ? 1 : 0,
                             request->GetURL().ToString().c_str());

        return false;
    }

    void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                              bool is_loading,
                              bool can_go_back,
                              bool can_go_forward) override
    {
        log_steam_tv_message(LOGSEVERITY_INFO,
                             "[steam_tv] loading_state loading=%d can_back=%d can_forward=%d",
                             is_loading ? 1 : 0,
                             can_go_back ? 1 : 0,
                             can_go_forward ? 1 : 0);
    }

    void OnLoadStart(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     TransitionType transition_type) override
    {
        log_steam_tv_message(LOGSEVERITY_INFO,
                             "[steam_tv] load_start main=%d transition=%d url=%s",
                             frame->IsMain() ? 1 : 0,
                             (int)transition_type,
                             frame->GetURL().ToString().c_str());
    }

    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int http_status_code) override
    {
        log_steam_tv_message(LOGSEVERITY_INFO,
                             "[steam_tv] load_end main=%d status=%d url=%s",
                             frame->IsMain() ? 1 : 0,
                             http_status_code,
                             frame->GetURL().ToString().c_str());
    }

    void OnLoadError(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     ErrorCode error_code,
                     const CefString& error_text,
                     const CefString& failed_url) override
    {
        log_steam_tv_message(LOGSEVERITY_ERROR,
                             "[steam_tv] load_error main=%d code=%d text=%s url=%s",
                             frame->IsMain() ? 1 : 0,
                             (int)error_code,
                             error_text.ToString().c_str(),
                             failed_url.ToString().c_str());
    }

    bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int popup_id,
                       const CefString& target_url,
                       const CefString& target_frame_name,
                       CefLifeSpanHandler::WindowOpenDisposition target_disposition,
                       bool user_gesture,
                       const CefPopupFeatures& popupFeatures,
                       CefWindowInfo& windowInfo,
                       CefRefPtr<CefClient>& client,
                       CefBrowserSettings& settings,
                       CefRefPtr<CefDictionaryValue>& extra_info,
                       bool* no_javascript_access) override
    {
        CEF_REQUIRE_UI_THREAD();

        {
            log_steam_tv_message(LOGSEVERITY_INFO,
                                 "[steam_tv] before_popup id=%d gesture=%d target=%s frame=%s",
                                 popup_id,
                                 user_gesture ? 1 : 0,
                                 target_url.ToString().c_str(),
                                 target_frame_name.ToString().c_str());
        }

        if (!target_url.empty()) {
            browser->GetMainFrame()->LoadURL(target_url);
        }

        return true;
    }

    bool DoClose(CefRefPtr<CefBrowser> browser) override
    {
        return false;
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override
    {
        CEF_REQUIRE_UI_THREAD();
        surface_->browser = nullptr;
    }

    void set_size(int width, int height)
    {
        if (width <= 0 || height <= 0) {
            return;
        }

        if (view_width_ == width && view_height_ == height) {
            return;
        }

        view_width_ = width;
        view_height_ = height;

        if (surface_->browser) {
            surface_->browser->GetHost()->WasResized();
        }
    }

private:
    BrowserSurface *surface_;
    int view_width_;
    int view_height_;

    IMPLEMENT_REFCOUNTING(BrowserClient);
    DISALLOW_COPY_AND_ASSIGN(BrowserClient);
};

static int build_file_url(char *buffer, int buffer_size, const char *path)
{
    return format_text(buffer, (size_t)buffer_size, "file://%s", path);
}

static int text_starts_with(const char *value, const char *prefix)
{
    unsigned int prefix_length;

    prefix_length = TextLength(prefix);
    if (TextLength(value) < prefix_length) {
        return 0;
    }

    return TextIsEqual(TextSubtext(value, 0, (int)prefix_length), prefix) ? 1 : 0;
}

static int has_url_scheme(const char *value)
{
    const char *scan;

    if (text_starts_with(value, "about:")) {
        return 1;
    }

    if (text_starts_with(value, "file:")) {
        return 1;
    }

    scan = value;
    while (*scan) {
        if (*scan == ':') {
            return text_starts_with(scan, "://");
        }

        if (*scan == '/' || *scan == '?' || *scan == '#') {
            return 0;
        }

        ++scan;
    }

    return 0;
}

static int build_browser_url(char *buffer, int buffer_size, const char *value)
{
    if (has_url_scheme(value)) {
        return format_text(buffer, (size_t)buffer_size, "%s", value);
    }

    return format_text(buffer, (size_t)buffer_size, "https://%s", value);
}

static int parse_arguments(int argc, char **argv)
{
    log_steam_tv_message(LOGSEVERITY_INFO, "[debug] parse_arguments");
    int i;

    g_has_start_url_argument = 0;
    g_start_url_argument[0] = 0;

    for (i = 1; i < argc; ++i) {
        if (TextIsEqual(argv[i], "--help") || TextIsEqual(argv[i], "-h")) {
            return 2;
        }

        if (TextIsEqual(argv[i], "--url")) {
            if (i + 1 >= argc) {
                return 0;
            }

            if (!format_text(g_start_url_argument,
                             sizeof(g_start_url_argument),
                             "%s",
                             argv[i + 1])) {
                return 0;
            }

            g_has_start_url_argument = 1;
            log_steam_tv_message(LOGSEVERITY_INFO, "[debug] g_start_url_argument=%s", g_start_url_argument);
            ++i;
            continue;
        }
    }

    return 1;
}

static void log_argument_list_after_startup(int argc, char **argv)
{
    int i;

    log_steam_tv_message(LOGSEVERITY_INFO, "[steam_tv] user_arg_count count=%d", argc - 1);

    for (i = 1; i < argc; ++i) {
        log_steam_tv_message(LOGSEVERITY_INFO,
                             "[steam_tv] argv index=%d user_index=%d value=%s",
                             i,
                             i - 1,
                             argv[i]);
    }

    log_steam_tv_message(LOGSEVERITY_INFO,
                         "[steam_tv] parsed_url value=%s",
                         g_has_start_url_argument ? g_start_url_argument : "");
}

static void print_usage(const char *program_name)
{
    printf("usage: %s [--url URL]\n", program_name);
    printf("\n");
    printf("If URL has no scheme, https:// is added.\n");
}

static int find_last_slash(const char *value)
{
    int index;
    int last_slash;

    last_slash = -1;
    for (index = 0; value[index]; ++index) {
        if (value[index] == '/') {
            last_slash = index;
        }
    }

    return last_slash;
}

static int build_executable_relative_path(char *buffer,
                                          int buffer_size,
                                          const char *relative_path)
{
    char executable_path[4096];
    ssize_t length;
    int last_slash;

    length = readlink("/proc/self/exe", executable_path, sizeof(executable_path) - 1);
    if (length <= 0 || length >= (ssize_t)(sizeof(executable_path) - 1)) {
        return 0;
    }

    executable_path[length] = 0;
    last_slash = find_last_slash(executable_path);
    if (last_slash < 0) {
        return 0;
    }

    executable_path[last_slash] = 0;
    if (!format_text(buffer, (size_t)buffer_size, "%s/%s", executable_path, relative_path)) {
        return 0;
    }

    return 1;
}

static void create_directory_if_missing(const char *path)
{
    if (mkdir(path, 0755) != 0) {
        return;
    }
}

static void create_cef_cache_directories(void)
{
    char cache_root[4096];
    char cef_cache[4096];

    if (!build_executable_relative_path(cache_root, sizeof(cache_root), "cache")) {
        return;
    }

    if (!build_executable_relative_path(cef_cache, sizeof(cef_cache), "cache/cef")) {
        return;
    }

    create_directory_if_missing(cache_root);
    create_directory_if_missing(cef_cache);
}

static void send_mouse_state(BrowserSurface *surface)
{
    CefMouseEvent mouse_event;
    Vector2 mouse_position;

    if (!surface->browser) {
        return;
    }

    mouse_position = GetMousePosition();
    mouse_event.x = (int)mouse_position.x;
    mouse_event.y = (int)mouse_position.y;
    mouse_event.modifiers = 0;

    surface->browser->GetHost()->SendMouseMoveEvent(mouse_event, false);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        surface->browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT, false, 1);
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        surface->browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT, true, 1);
    }
}

int main(int argc, char **argv)
{

    int argument_result;

    argument_result = parse_arguments(argc, argv);
    if (argument_result == 2) {
        print_usage(argv[0]);
        return 0;
    }

    if (!argument_result) {
        printf("missing URL after --url\n");
        print_usage(argv[0]);
        return 1;
    }

    CefMainArgs main_args(argc, argv);
    int exit_code;
    CefRefPtr<CefApp> app;
    CefSettings settings;
    BrowserSurface surface;
    CefRefPtr<BrowserClient> client;
    CefWindowInfo window_info;
    CefBrowserSettings browser_settings;
    char url[4096];
    char cef_cache_path[4096];
    char cef_log_path[4096];
    Texture2D browser_texture;
    Shader browser_shader;
    Image blank_image;
    int screen_width;
    int screen_height;
    int cef_ok;

    exit_code = CefExecuteProcess(main_args, app, 0);
    if (exit_code >= 0) {
        return exit_code;
    }

    screen_width = 1280;
    screen_height = 720;

    browser_surface_init(&surface);
    browser_surface_resize(&surface, screen_width, screen_height);

    settings.no_sandbox = true;
    settings.windowless_rendering_enabled = true;
    settings.external_message_pump = true;
    settings.persist_session_cookies = true;

    create_cef_cache_directories();

    if (build_executable_relative_path(cef_cache_path, sizeof(cef_cache_path), "cache/cef")) {
        CefString(&settings.cache_path) = cef_cache_path;
        CefString(&settings.root_cache_path) = cef_cache_path;
    }

    if (build_executable_relative_path(cef_log_path, sizeof(cef_log_path), "steam_tv.log")) {
        CefString(&settings.log_file) = cef_log_path;
    }

    cef_ok = CefInitialize(main_args, settings, app, 0);
    if (!cef_ok) {
        printf("failed to initialize CEF\n");
        browser_surface_shutdown(&surface);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_width, screen_height, "Steam TV Baseline");
    SetTargetFPS(60);

    blank_image = GenImageColor(screen_width, screen_height, BLACK);
    browser_texture = LoadTextureFromImage(blank_image);
    UnloadImage(blank_image);
    browser_shader = LoadShaderFromMemory(0, BROWSER_SWIZZLE_FRAGMENT_SHADER);
    if (!IsShaderValid(browser_shader)) {
        printf("failed to load browser swizzle shader\n");
        UnloadTexture(browser_texture);
        CloseWindow();
        CefShutdown();
        browser_surface_shutdown(&surface);
        return 1;
    }

    client = new BrowserClient(&surface, screen_width, screen_height);
    window_info.SetAsWindowless(0);
    browser_settings.background_color = CefColorSetARGB(255, 17, 19, 24);

    if (g_has_start_url_argument) {
        if (!build_browser_url(url, sizeof(url), g_start_url_argument)) {
            log_steam_tv_message(LOGSEVERITY_ERROR, "[steam_tv] failed to format start URL");
            CefShutdown();
            browser_surface_shutdown(&surface);
            return 1;
        }
    } else {
        if (!build_file_url(url, sizeof(url), STEAM_TV_STATIC_SITE_FILE)) {
            log_steam_tv_message(LOGSEVERITY_ERROR, "[steam_tv] failed to format static site URL");
            CefShutdown();
            browser_surface_shutdown(&surface);
            return 1;
        }
    }

    log_argument_list_after_startup(argc, argv);
    log_steam_tv_message(LOGSEVERITY_INFO, "[steam_tv] start_url url=%s", url);

    CefBrowserHost::CreateBrowser(window_info, client, url, browser_settings, nullptr, nullptr);

    while (!WindowShouldClose()) {
        int current_width;
        int current_height;

        CefDoMessageLoopWork();

        current_width = GetScreenWidth();
        current_height = GetScreenHeight();
        client->set_size(current_width, current_height);
        send_mouse_state(&surface);

        if (surface.browser && IsKeyPressed(KEY_F5)) {
            surface.browser->ReloadIgnoreCache();
        }

        if (surface.dirty && surface.pixels) {
            if (browser_texture.width != surface.width ||
                browser_texture.height != surface.height) {
                UnloadTexture(browser_texture);
                blank_image = GenImageColor(surface.width, surface.height, BLACK);
                browser_texture = LoadTextureFromImage(blank_image);
                UnloadImage(blank_image);
            }

            UpdateTexture(browser_texture, surface.pixels);
            surface.dirty = 0;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        BeginShaderMode(browser_shader);
        DrawTexturePro(browser_texture,
                       Rectangle{0.0f, 0.0f, (float)browser_texture.width, (float)browser_texture.height},
                       Rectangle{0.0f, 0.0f, (float)current_width, (float)current_height},
                       Vector2{0.0f, 0.0f},
                       0.0f,
                       WHITE);
        EndShaderMode();

        {
            float frame_time_ms;
            int fps;
            const char *perf_text;

            frame_time_ms = GetFrameTime() * 1000.0f;
            fps = GetFPS();
            perf_text = TextFormat("frame %.2f ms | %d fps", frame_time_ms, fps);

            DrawRectangle(18, 16, 262, 36, Color{0, 0, 0, 165});
            DrawText(perf_text, 28, 25, 18, Color{245, 245, 245, 235});
        }

        DrawCircleV(GetMousePosition(), 9.0f, Color{245, 245, 245, 235});
        DrawCircleLines((int)GetMouseX(), (int)GetMouseY(), 13.0f, Color{20, 20, 20, 220});
        DrawText("ESC closes the window. Mouse input is forwarded into CEF.",
                 24,
                 current_height - 42,
                 20,
                 Color{235, 238, 244, 220});

        EndDrawing();
    }

    if (surface.browser) {
        surface.browser->GetHost()->CloseBrowser(true);
        surface.browser = nullptr;
    }

    client = nullptr;
    UnloadShader(browser_shader);
    UnloadTexture(browser_texture);
    CloseWindow();

    CefShutdown();
    browser_surface_shutdown(&surface);

    return 0;
}
