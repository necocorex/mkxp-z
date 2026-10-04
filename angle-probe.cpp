#include <unknwn.h>
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cmath>

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::UI::Core;
using Microsoft::WRL::ComPtr;

typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLSurface;
typedef void* EGLContext;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef unsigned int GLbitfield;
typedef unsigned char GLboolean;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef char GLchar;

#define DECL(ret, name, args) typedef ret (*T_##name) args; static T_##name name = nullptr;
DECL(void*, eglGetProcAddress, (const char*))
DECL(EGLDisplay, eglGetPlatformDisplayEXT, (EGLenum, void*, const EGLint*))
DECL(EGLBoolean, eglInitialize, (EGLDisplay, EGLint*, EGLint*))
DECL(EGLBoolean, eglChooseConfig, (EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*))
DECL(EGLSurface, eglCreateWindowSurface, (EGLDisplay, EGLConfig, void*, const EGLint*))
DECL(EGLContext, eglCreateContext, (EGLDisplay, EGLConfig, EGLContext, const EGLint*))
DECL(EGLBoolean, eglMakeCurrent, (EGLDisplay, EGLSurface, EGLSurface, EGLContext))
DECL(EGLBoolean, eglSwapBuffers, (EGLDisplay, EGLSurface))
DECL(EGLBoolean, eglQuerySurface, (EGLDisplay, EGLSurface, EGLint, EGLint*))
DECL(EGLint, eglGetError, ())
DECL(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))
DECL(void, glClear, (GLbitfield))
DECL(void, glEnable, (GLenum))
DECL(void, glScissor, (GLint, GLint, GLsizei, GLsizei))
DECL(void, glViewport, (GLint, GLint, GLsizei, GLsizei))
DECL(GLuint, glCreateShader, (GLenum))
DECL(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*))
DECL(void, glCompileShader, (GLuint))
DECL(void, glGetShaderiv, (GLuint, GLenum, GLint*))
DECL(GLuint, glCreateProgram, ())
DECL(void, glAttachShader, (GLuint, GLuint))
DECL(void, glBindAttribLocation, (GLuint, GLuint, const GLchar*))
DECL(void, glLinkProgram, (GLuint))
DECL(void, glGetProgramiv, (GLuint, GLenum, GLint*))
DECL(void, glUseProgram, (GLuint))
DECL(GLint, glGetUniformLocation, (GLuint, const GLchar*))
DECL(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))
DECL(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))
DECL(void, glEnableVertexAttribArray, (GLuint))
DECL(void, glDrawArrays, (GLenum, GLint, GLsizei))
DECL(GLenum, glGetError, ())

static HMODULE hEGL = nullptr;
static HMODULE hGL = nullptr;
static EGLDisplay g_dpy = nullptr;
static EGLSurface g_surf = nullptr;
static EGLContext g_ctx = nullptr;
static GLuint g_prog = 0;
static GLint g_loc = -1;
static int g_ok = 0;
static unsigned g_err = 0;
static int g_W = 1920;
static int g_H = 1080;

static bool LoadAll(unsigned& err)
{
    int idx = 0;
#define LOADE(name) idx++; name = reinterpret_cast<T_##name>(GetProcAddress(hEGL, #name)); if (!name) { err = idx; return false; }
#define LOADG(name) idx++; name = reinterpret_cast<T_##name>(GetProcAddress(hGL, #name)); if (!name) { err = idx; return false; }
    LOADE(eglGetProcAddress)
    LOADE(eglInitialize)
    LOADE(eglChooseConfig)
    LOADE(eglCreateWindowSurface)
    LOADE(eglCreateContext)
    LOADE(eglMakeCurrent)
    LOADE(eglSwapBuffers)
    LOADE(eglQuerySurface)
    LOADE(eglGetError)
    LOADG(glClearColor)
    LOADG(glClear)
    LOADG(glEnable)
    LOADG(glScissor)
    LOADG(glViewport)
    LOADG(glCreateShader)
    LOADG(glShaderSource)
    LOADG(glCompileShader)
    LOADG(glGetShaderiv)
    LOADG(glCreateProgram)
    LOADG(glAttachShader)
    LOADG(glBindAttribLocation)
    LOADG(glLinkProgram)
    LOADG(glGetProgramiv)
    LOADG(glUseProgram)
    LOADG(glGetUniformLocation)
    LOADG(glUniform4f)
    LOADG(glVertexAttribPointer)
    LOADG(glEnableVertexAttribArray)
    LOADG(glDrawArrays)
    LOADG(glGetError)
    return true;
}

static bool InitEgl(CoreWindow const& window)
{
    hEGL = LoadPackagedLibrary(L"libEGL.dll", 0);
    if (!hEGL) { g_err = GetLastError(); return false; }
    g_ok = 1;
    hGL = LoadPackagedLibrary(L"libGLESv2.dll", 0);
    if (!hGL) { g_err = GetLastError(); return false; }
    g_ok = 2;
    if (!LoadAll(g_err)) return false;
    g_ok = 3;

    eglGetPlatformDisplayEXT = reinterpret_cast<T_eglGetPlatformDisplayEXT>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!eglGetPlatformDisplayEXT) { g_err = 1; return false; }
    const EGLint dattr[] = { 0x3203, 0x3208, 0x3038 };
    g_dpy = eglGetPlatformDisplayEXT(0x3202, nullptr, dattr);
    if (!g_dpy) { g_err = eglGetError(); return false; }
    g_ok = 4;

    EGLint maj = 0;
    EGLint mnr = 0;
    if (!eglInitialize(g_dpy, &maj, &mnr)) { g_err = eglGetError(); return false; }
    g_ok = 5;

    const EGLint cattr[] = { 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3033, 4, 0x3040, 4, 0x3038 };
    EGLConfig cfg = nullptr;
    EGLint n = 0;
    if (!eglChooseConfig(g_dpy, cattr, &cfg, 1, &n) || n < 1) { g_err = eglGetError(); return false; }
    g_ok = 6;

    const EGLint sattr[] = { 0x3038 };
    g_surf = eglCreateWindowSurface(g_dpy, cfg, get_abi(window), sattr);
    if (!g_surf) { g_err = eglGetError(); return false; }
    g_ok = 7;

    const EGLint xattr[] = { 0x3098, 2, 0x3038 };
    g_ctx = eglCreateContext(g_dpy, cfg, nullptr, xattr);
    if (!g_ctx) { g_err = eglGetError(); return false; }
    g_ok = 8;

    if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) { g_err = eglGetError(); return false; }
    g_ok = 9;

    EGLint w = 0;
    EGLint h = 0;
    eglQuerySurface(g_dpy, g_surf, 0x3057, &w);
    eglQuerySurface(g_dpy, g_surf, 0x3056, &h);
    if (w > 0 && h > 0) { g_W = w; g_H = h; }

    static const char* vs = "attribute vec2 p; void main() { gl_Position = vec4(p, 0.0, 1.0); }";
    static const char* fs = "precision mediump float; uniform vec4 c; void main() { gl_FragColor = c; }";
    GLint st = 0;
    GLuint v = glCreateShader(0x8B31);
    glShaderSource(v, 1, &vs, nullptr);
    glCompileShader(v);
    glGetShaderiv(v, 0x8B81, &st);
    if (!st) { g_err = 1; return false; }
    GLuint f = glCreateShader(0x8B30);
    glShaderSource(f, 1, &fs, nullptr);
    glCompileShader(f);
    glGetShaderiv(f, 0x8B81, &st);
    if (!st) { g_err = 2; return false; }
    g_prog = glCreateProgram();
    glAttachShader(g_prog, v);
    glAttachShader(g_prog, f);
    glBindAttribLocation(g_prog, 0, "p");
    glLinkProgram(g_prog);
    glGetProgramiv(g_prog, 0x8B82, &st);
    if (!st) { g_err = 3; return false; }
    g_loc = glGetUniformLocation(g_prog, "c");
    return true;
}

static void BoxPx(int x, int y, int w, int h, float r, float g, float b)
{
    glScissor(x, g_H - y - h, w, h);
    glClearColor(r, g, b, 1.0f);
    glClear(0x4000);
}

static void RenderFrame(float t)
{
    int u = g_H / 20;
    glEnable(0x0C11);
    glViewport(0, 0, g_W, g_H);
    BoxPx(0, 0, g_W, g_H, 0.08f, 0.08f, 0.08f);
    for (int i = 0; i < 10; i++)
    {
        bool on = i < g_ok;
        BoxPx(u + i * u * 3 / 2, u, u, u, on ? 0.0f : 0.3f, on ? 1.0f : 0.3f, on ? 0.0f : 0.3f);
    }

    BoxPx(u, 3 * u, 4 * u, 3 * u, 0.3f, 0.3f, 0.3f);
    glScissor(u, g_H - 6 * u, 4 * u, 3 * u);
    glUseProgram(g_prog);
    glUniform4f(g_loc, 1.0f, 0.0f, 1.0f, 1.0f);
    static const float verts[] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };
    glVertexAttribPointer(0, 2, 0x1406, 0, 0, verts);
    glEnableVertexAttribArray(0);
    glDrawArrays(4, 0, 3);

    BoxPx(6 * u, 3 * u, 4 * u, 3 * u,
        0.5f + 0.5f * std::sin(t),
        0.5f + 0.5f * std::sin(t + 2.0f),
        0.5f + 0.5f * std::sin(t + 4.0f));
    eglSwapBuffers(g_dpy, g_surf);
}

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
    bool closed = false;
    IFrameworkView CreateView() { return *this; }
    void Initialize(CoreApplicationView const&) {}
    void Load(hstring const&) {}
    void Uninitialize() {}
    void SetWindow(CoreWindow const& w)
    {
        w.Closed([this](auto&&, auto&&) { closed = true; });
    }

    void FailLoop(CoreWindow const& window)
    {
        ComPtr<ID3D11Device> dev;
        ComPtr<ID3D11DeviceContext> ctx;
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
            D3D11_SDK_VERSION, &dev, nullptr, &ctx);
        ComPtr<ID3D11DeviceContext1> ctx1;
        ctx.As(&ctx1);
        ComPtr<IDXGIDevice> dxgiDev;
        dev.As(&dxgiDev);
        ComPtr<IDXGIAdapter> adapter;
        dxgiDev->GetAdapter(&adapter);
        ComPtr<IDXGIFactory2> factory;
        adapter->GetParent(IID_PPV_ARGS(&factory));

        DXGI_SWAP_CHAIN_DESC1 d = {};
        d.Width = 1920;
        d.Height = 1080;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d.BufferCount = 2;
        d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        ComPtr<IDXGISwapChain1> sc;
        factory->CreateSwapChainForCoreWindow(dev.Get(),
            reinterpret_cast<IUnknown*>(get_abi(window)), &d, nullptr, &sc);
        ComPtr<ID3D11Texture2D> bb;
        sc->GetBuffer(0, IID_PPV_ARGS(&bb));
        ComPtr<ID3D11RenderTargetView> rtv;
        dev->CreateRenderTargetView(bb.Get(), nullptr, &rtv);

        float bg[4] = { 0.08f, 0.08f, 0.08f, 1.0f };
        while (!closed)
        {
            window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
            ctx->ClearRenderTargetView(rtv.Get(), bg);
            for (int i = 0; i < 10; i++)
            {
                bool on = i < g_ok;
                float c[4] = { on ? 0.0f : 0.3f, on ? 1.0f : 0.3f, on ? 0.0f : 0.3f, 1.0f };
                D3D11_RECT rc = { 50 + i * 110, 40, 50 + i * 110 + 95, 120 };
                ctx1->ClearView(rtv.Get(), c, &rc, 1);
            }
            float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
            D3D11_RECT lab = { 50, 200, 170, 280 };
            ctx1->ClearView(rtv.Get(), red, &lab, 1);
            for (int i = 0; i < 16; i++)
            {
                bool bit = ((g_err >> (15 - i)) & 1) != 0;
                float v = bit ? 1.0f : 0.0f;
                float c[4] = { v, v, v, 1.0f };
                D3D11_RECT rc = { 200 + i * 70, 200, 200 + i * 70 + 55, 280 };
                ctx1->ClearView(rtv.Get(), c, &rc, 1);
            }
            sc->Present(1, 0);
        }
    }

    void Run()
    {
        CoreWindow window = CoreWindow::GetForCurrentThread();
        window.Activate();
        window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

        if (!InitEgl(window))
        {
            FailLoop(window);
            return;
        }

        float t = 0.0f;
        bool checked = false;
        while (!closed)
        {
            window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
            RenderFrame(t);
            t += 0.05f;
            if (!checked)
            {
                checked = true;
                if (glGetError() == 0) g_ok = 10;
            }
        }
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoreApplication::Run(make<App>());
    return 0;
}
