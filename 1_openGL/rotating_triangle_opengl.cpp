// Basic OpenGL rotating triangle.
// Build in "x64 Native Tools Command Prompt for VS":
// cl /EHsc rotating_triangle_opengl.cpp user32.lib gdi32.lib opengl32.lib

#include <windows.h>
#include <gl/GL.h>

static HDC g_dc;
static HGLRC g_glrc;
static float g_angle;

static void Render()
{
    RECT rc;
    GetClientRect(WindowFromDC(g_dc), &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    glViewport(0, 0, width, height);
    glClearColor(0.15f, 0.08f, 0.22f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = height > 0 ? (float)width / (float)height : 1.0f;
    glFrustum(-aspect, aspect, -1.0, 1.0, 1.5, 10.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -4.0f);
    glRotatef(g_angle, 0.0f, 1.0f, 0.0f);
    glRotatef(g_angle * 0.65f, 1.0f, 0.0f, 0.0f);

    glBegin(GL_TRIANGLES);
    glColor3f(0.92f, 0.35f, 1.0f);
    glVertex3f(0.0f, 0.95f, 0.0f);
    glColor3f(0.35f, 0.90f, 1.0f);
    glVertex3f(-0.95f, -0.65f, 0.0f);
    glColor3f(1.0f, 0.92f, 0.35f);
    glVertex3f(0.95f, -0.65f, 0.0f);
    glEnd();

    glLineWidth(4.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(0.0f, 0.95f, 0.0f);
    glVertex3f(-0.95f, -0.65f, 0.0f);
    glVertex3f(0.95f, -0.65f, 0.0f);
    glEnd();

    SwapBuffers(g_dc);
}

static bool SetupOpenGL(HWND hwnd)
{
    g_dc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(g_dc, &pfd);
    if (!format || !SetPixelFormat(g_dc, format, &pfd)) {
        return false;
    }

    g_glrc = wglCreateContext(g_dc);
    return g_glrc && wglMakeCurrent(g_dc, g_glrc);
}

static void CleanupOpenGL(HWND hwnd)
{
    if (g_glrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_glrc);
        g_glrc = nullptr;
    }
    if (g_dc) {
        ReleaseDC(hwnd, g_dc);
        g_dc = nullptr;
    }
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        if (!SetupOpenGL(hwnd)) {
            MessageBox(hwnd, L"OpenGL context creation failed.", L"Error", MB_ICONERROR);
            return -1;
        }
        SetTimer(hwnd, 1, 16, nullptr);
        return 0;
    case WM_TIMER:
        g_angle += 1.2f;
        if (g_angle >= 360.0f) {
            g_angle -= 360.0f;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        CleanupOpenGL(hwnd);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCmd)
{
    const wchar_t className[] = L"BasicOpenGLRotatingTriangle";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = className;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, className, L"OpenGL - Rotating Triangle",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, instance, nullptr);

    ShowWindow(hwnd, showCmd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
