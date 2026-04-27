#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cef_base.h"
#include "cef_app.h"
#include "cef_browser.h"
#include "cef_client.h"
#include "cef_command_line.h"
#include "cef_render_handler.h"
#include "wrapper/cef_helpers.h"
#include "wrapper/cef_helpers.h"
#include "raylib.h"

struct BrowserSurface {
    int width;
    int height;
    unsigned char *pixels;
    int dirty;
    CefRefPtr<CefBrowser> browser;
};

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
    const unsigned char *src;
    unsigned char *dst;
    int pixel_count;
    int i;

    if (!browser_surface_resize(surface, width, height)) {
        return;
    }

    src = (const unsigned char *)buffer;
    dst = surface->pixels;
    pixel_count = width * height;

    for (i = 0; i < pixel_count; ++i) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = src[3];

        src += 4;
        dst += 4;
    }

    surface->dirty = 1;
}

class BrowserClient : public CefClient,
                      public CefRenderHandler,
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

static void build_file_url(char *buffer, int buffer_size, const char *path)
{
    snprintf(buffer, (size_t)buffer_size, "file://%s", path);
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
    CefMainArgs main_args(argc, argv);
    CefRefPtr<CefApp> app;
    int exit_code;
    CefSettings settings;
    BrowserSurface surface;
    CefRefPtr<BrowserClient> client;
    CefWindowInfo window_info;
    CefBrowserSettings browser_settings;
    char url[4096];
    Texture2D browser_texture;
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
    settings.external_message_pump = false;
    CefString(&settings.cache_path) = "cache/cef";
    CefString(&settings.root_cache_path) = "cache/cef";

    cef_ok = CefInitialize(main_args, settings, app, 0);
    if (!cef_ok) {
        printf("failed to initialize CEF\n");
        browser_surface_shutdown(&surface);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_width, screen_height, "Steam TV Baseline");
    //SetTargetFPS(60);

    blank_image = GenImageColor(screen_width, screen_height, BLACK);
    browser_texture = LoadTextureFromImage(blank_image);
    UnloadImage(blank_image);

    client = new BrowserClient(&surface, screen_width, screen_height);
    window_info.SetAsWindowless(0);
    browser_settings.background_color = CefColorSetARGB(255, 17, 19, 24);

    build_file_url(url, sizeof(url), STEAM_TV_STATIC_SITE_FILE);
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

        DrawTexturePro(browser_texture,
                       Rectangle{0.0f, 0.0f, (float)browser_texture.width, (float)browser_texture.height},
                       Rectangle{0.0f, 0.0f, (float)current_width, (float)current_height},
                       Vector2{0.0f, 0.0f},
                       0.0f,
                       WHITE);

        {
            float frame_time_ms;
            int fps;
            char perf_text[128];

            frame_time_ms = GetFrameTime() * 1000.0f;
            fps = GetFPS();
            snprintf(perf_text, sizeof(perf_text), "frame %.2f ms | %d fps", frame_time_ms, fps);

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
    UnloadTexture(browser_texture);
    CloseWindow();

    CefShutdown();
    browser_surface_shutdown(&surface);

    return 0;
}
