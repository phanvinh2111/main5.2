# SDL2 Implementation Files cho MU Mobile

## 1. Core Files

### SDL2Main.cpp
```cpp
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "GameManager.h"
#include "SDLRenderer.h"
#include "TouchInput.h"

#ifdef __ANDROID__
#include <jni.h>
#include <android/native_activity.h>
#endif

#ifdef __IOS__
#include <UIKit/UIKit.h>
#endif

// Global instances
GameManager* g_GameManager = nullptr;
SDLRenderer* g_Renderer = nullptr;
TouchInput* g_TouchInput = nullptr;

// Platform-specific main function
#ifdef __ANDROID__
extern "C" JNIEXPORT int JNICALL SDL_main(int argc, char* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        SDL_Log("SDL2 initialization failed: %s", SDL_GetError());
        return -1;
    }
    
    // Get display info
    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    
    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "MU Mobile",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        displayMode.w, displayMode.h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN
    );
    
    if (!window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        return -1;
    }
    
    // Initialize renderer
    g_Renderer = new SDLRenderer();
    if (!g_Renderer->Initialize(window)) {
        SDL_Log("Renderer initialization failed");
        return -1;
    }
    
    // Initialize touch input
    g_TouchInput = new TouchInput();
    g_TouchInput->Initialize();
    
    // Initialize game manager
    g_GameManager = new GameManager();
    g_GameManager->Initialize();
    
    // Main game loop
    bool running = true;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();
    
    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // Handle events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                    
                case SDL_FINGERDOWN:
                case SDL_FINGERUP:
                case SDL_FINGERMOTION:
                    g_TouchInput->HandleEvent(event);
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;
                    
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        g_Renderer->Resize(event.window.data1, event.window.data2);
                    }
                    break;
            }
        }
        
        // Update game logic
        g_GameManager->Update(deltaTime);
        
        // Render
        g_Renderer->BeginFrame();
        g_GameManager->Render();
        g_Renderer->EndFrame();
        
        // Cap frame rate
        Uint32 frameTime = SDL_GetTicks() - currentTime;
        if (frameTime < 16) { // 60 FPS
            SDL_Delay(16 - frameTime);
        }
    }
    
    // Cleanup
    delete g_GameManager;
    delete g_TouchInput;
    delete g_Renderer;
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
```

### SDLRenderer.h
```cpp
#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <vector>
#include <string>

class SDLRenderer {
private:
    SDL_Window* window;
    SDL_GLContext glContext;
    int windowWidth;
    int windowHeight;
    
    // OpenGL state
    bool glInitialized;
    
    // UI rendering
    struct UIVertex {
        float x, y, u, v;
        uint32_t color;
    };
    std::vector<UIVertex> uiVertices;
    GLuint uiVBO;
    
public:
    SDLRenderer();
    ~SDLRenderer();
    
    bool Initialize(SDL_Window* window);
    void Shutdown();
    
    void BeginFrame();
    void EndFrame();
    
    void Resize(int width, int height);
    
    // 3D rendering (existing OpenGL code)
    void Render3DScene();
    
    // 2D UI rendering
    void RenderUI();
    void RenderVirtualJoystick();
    void RenderSkillButtons();
    void RenderStatusBars();
    
    // Utility functions
    void SetViewport(int x, int y, int width, int height);
    void SetOrthoProjection(float left, float right, float bottom, float top);
    void SetPerspectiveProjection(float fov, float aspect, float near, float far);
    
private:
    bool InitializeOpenGL();
    void InitializeUI();
    void RenderQuad(float x, float y, float width, float height, float u, float v, float uw, float vh, uint32_t color);
    void RenderCircle(float x, float y, float radius, uint32_t color);
};
```

### SDLRenderer.cpp
```cpp
#include "SDLRenderer.h"
#include <cmath>

SDLRenderer::SDLRenderer() 
    : window(nullptr), glContext(nullptr), windowWidth(0), windowHeight(0), glInitialized(false) {
}

SDLRenderer::~SDLRenderer() {
    Shutdown();
}

bool SDLRenderer::Initialize(SDL_Window* window) {
    this->window = window;
    
    // Get window size
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    
    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // Create OpenGL context
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        SDL_Log("OpenGL context creation failed: %s", SDL_GetError());
        return false;
    }
    
    // Initialize OpenGL
    if (!InitializeOpenGL()) {
        return false;
    }
    
    // Initialize UI rendering
    InitializeUI();
    
    return true;
}

void SDLRenderer::Shutdown() {
    if (uiVBO) {
        glDeleteBuffers(1, &uiVBO);
        uiVBO = 0;
    }
    
    if (glContext) {
        SDL_GL_DeleteContext(glContext);
        glContext = nullptr;
    }
    
    window = nullptr;
}

bool SDLRenderer::InitializeOpenGL() {
    // Enable features
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Set viewport
    glViewport(0, 0, windowWidth, windowHeight);
    
    glInitialized = true;
    return true;
}

void SDLRenderer::InitializeUI() {
    // Create VBO for UI rendering
    glGenBuffers(1, &uiVBO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    
    // Enable vertex attributes
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
}

void SDLRenderer::BeginFrame() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void SDLRenderer::EndFrame() {
    SDL_GL_SwapWindow(window);
}

void SDLRenderer::Resize(int width, int height) {
    windowWidth = width;
    windowHeight = height;
    glViewport(0, 0, width, height);
}

void SDLRenderer::Render3DScene() {
    // Set 3D projection
    SetPerspectiveProjection(45.0f, (float)windowWidth / windowHeight, 0.1f, 1000.0f);
    
    // Set modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Render 3D scene (existing code from ZzzScene.cpp)
    // This code remains largely unchanged
    RenderCharacters();
    RenderMonsters();
    RenderEffects();
    RenderTerrain();
}

void SDLRenderer::RenderUI() {
    // Set 2D projection
    SetOrthoProjection(0, windowWidth, windowHeight, 0);
    
    // Disable depth test for UI
    glDisable(GL_DEPTH_TEST);
    
    // Render UI elements
    RenderVirtualJoystick();
    RenderSkillButtons();
    RenderStatusBars();
    
    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);
}

void SDLRenderer::RenderVirtualJoystick() {
    // Get joystick from input system
    auto joystick = g_TouchInput->GetJoystick();
    if (!joystick) return;
    
    // Render joystick background
    RenderCircle(joystick->GetCenterX(), joystick->GetCenterY(), joystick->GetRadius(), 0x33000000);
    
    // Render joystick handle
    RenderCircle(joystick->GetCurrentX(), joystick->GetCurrentY(), 20.0f, 0xCCFFFFFF);
}

void SDLRenderer::RenderSkillButtons() {
    // Get skill buttons from UI system
    auto buttons = g_GameManager->GetUI()->GetSkillButtons();
    
    for (auto button : buttons) {
        float x = button->GetX();
        float y = button->GetY();
        float width = button->GetWidth();
        float height = button->GetHeight();
        
        // Render button background
        uint32_t color = button->IsPressed() ? 0xCC666666 : 0xCC333333;
        RenderQuad(x, y, width, height, 0, 0, 1, 1, color);
        
        // Render button text
        // (Text rendering would be implemented separately)
    }
}

void SDLRenderer::RenderStatusBars() {
    auto character = g_GameManager->GetCurrentCharacter();
    if (!character) return;
    
    float barWidth = 200.0f;
    float barHeight = 20.0f;
    float x = 50.0f;
    float y = 50.0f;
    
    // Health bar
    RenderBar(x, y, barWidth, barHeight, character->GetHealthPercent(), 0xFFFF0000);
    
    // Mana bar
    y += barHeight + 10;
    RenderBar(x, y, barWidth, barHeight, character->GetManaPercent(), 0xFF0000FF);
    
    // Experience bar
    y += barHeight + 10;
    RenderBar(x, y, barWidth, barHeight, character->GetExpPercent(), 0xFF00FF00);
}

void SDLRenderer::RenderBar(float x, float y, float width, float height, float percent, uint32_t color) {
    // Background
    RenderQuad(x, y, width, height, 0, 0, 1, 1, 0x33000000);
    
    // Fill
    RenderQuad(x, y, width * percent, height, 0, 0, 1, 1, color);
}

void SDLRenderer::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void SDLRenderer::SetOrthoProjection(float left, float right, float bottom, float top) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(left, right, bottom, top, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void SDLRenderer::SetPerspectiveProjection(float fov, float aspect, float near, float far) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fov, aspect, near, far);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void SDLRenderer::RenderQuad(float x, float y, float width, float height, float u, float v, float uw, float vh, uint32_t color) {
    glBegin(GL_QUADS);
    glColor4ub((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);
    glTexCoord2f(u, v);
    glVertex2f(x, y);
    glTexCoord2f(u + uw, v);
    glVertex2f(x + width, y);
    glTexCoord2f(u + uw, v + vh);
    glVertex2f(x + width, y + height);
    glTexCoord2f(u, v + vh);
    glVertex2f(x, y + height);
    glEnd();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void SDLRenderer::RenderCircle(float x, float y, float radius, uint32_t color) {
    glBegin(GL_TRIANGLE_FAN);
    glColor4ub((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);
    glVertex2f(x, y);
    
    for (int i = 0; i <= 32; i++) {
        float angle = 2.0f * M_PI * i / 32.0f;
        glVertex2f(x + radius * cos(angle), y + radius * sin(angle));
    }
    glEnd();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}
```

## 2. Input System

### TouchInput.h
```cpp
#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <functional>

struct TouchPoint {
    SDL_FingerID id;
    float x, y;
    bool active;
    Uint32 timestamp;
};

class VirtualJoystick;
class TouchButton;

class TouchInput {
private:
    TouchPoint touches[10]; // Support up to 10 touch points
    VirtualJoystick* joystick;
    std::vector<TouchButton*> buttons;
    
    // Gesture recognition
    struct Gesture {
        float startX, startY;
        float endX, endY;
        Uint32 startTime;
        Uint32 endTime;
        bool isSwipe;
        bool isTap;
    };
    
    std::vector<Gesture> gestures;
    
public:
    TouchInput();
    ~TouchInput();
    
    void Initialize();
    void HandleEvent(const SDL_Event& event);
    void Update();
    
    // Touch point management
    TouchPoint* GetTouchPoint(SDL_FingerID id);
    TouchPoint* GetFreeTouchPoint();
    
    // Gesture recognition
    void ProcessGestures();
    bool IsSwipe(float minDistance, float maxTime);
    bool IsTap(float maxDistance, float maxTime);
    
    // Getters
    VirtualJoystick* GetJoystick() const { return joystick; }
    const std::vector<TouchButton*>& GetButtons() const { return buttons; }
    
private:
    void OnTouchDown(const SDL_TouchFingerEvent& event);
    void OnTouchUp(const SDL_TouchFingerEvent& event);
    void OnTouchMotion(const SDL_TouchFingerEvent& event);
    
    void UpdateJoystick(const TouchPoint& touch);
    void UpdateButtons(const TouchPoint& touch);
};
```

### TouchInput.cpp
```cpp
#include "TouchInput.h"
#include "VirtualJoystick.h"
#include "TouchButton.h"
#include <algorithm>

TouchInput::TouchInput() : joystick(nullptr) {
    // Initialize touch points
    for (int i = 0; i < 10; i++) {
        touches[i].id = 0;
        touches[i].active = false;
    }
}

TouchInput::~TouchInput() {
    delete joystick;
    for (auto button : buttons) {
        delete button;
    }
}

void TouchInput::Initialize() {
    // Enable touch events
    SDL_SetHint(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    
    // Create virtual joystick
    joystick = new VirtualJoystick();
    joystick->SetPosition(100, 400); // Bottom-left corner
    
    // Create skill buttons
    for (int i = 0; i < 4; i++) {
        TouchButton* button = new TouchButton();
        button->SetPosition(600 + i * 80, 400);
        button->SetSize(60, 60);
        button->SetText(std::to_string(i + 1));
        buttons.push_back(button);
    }
}

void TouchInput::HandleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_FINGERDOWN:
            OnTouchDown(event.tfinger);
            break;
        case SDL_FINGERUP:
            OnTouchUp(event.tfinger);
            break;
        case SDL_FINGERMOTION:
            OnTouchMotion(event.tfinger);
            break;
    }
}

void TouchInput::Update() {
    // Process gestures
    ProcessGestures();
    
    // Update joystick
    if (joystick) {
        joystick->Update();
    }
    
    // Update buttons
    for (auto button : buttons) {
        button->Update();
    }
}

TouchPoint* TouchInput::GetTouchPoint(SDL_FingerID id) {
    for (int i = 0; i < 10; i++) {
        if (touches[i].id == id) {
            return &touches[i];
        }
    }
    return nullptr;
}

TouchPoint* TouchInput::GetFreeTouchPoint() {
    for (int i = 0; i < 10; i++) {
        if (!touches[i].active) {
            return &touches[i];
        }
    }
    return nullptr;
}

void TouchInput::OnTouchDown(const SDL_TouchFingerEvent& event) {
    TouchPoint* touch = GetTouchPoint(event.fingerId);
    if (!touch) {
        touch = GetFreeTouchPoint();
    }
    
    if (touch) {
        touch->id = event.fingerId;
        touch->x = event.x;
        touch->y = event.y;
        touch->active = true;
        touch->timestamp = SDL_GetTicks();
        
        // Check if touch is on joystick
        if (joystick && joystick->IsPointInside(touch->x, touch->y)) {
            joystick->OnTouchDown(touch->x, touch->y);
        }
        
        // Check if touch is on buttons
        for (auto button : buttons) {
            if (button->IsPointInside(touch->x, touch->y)) {
                button->OnTouchDown();
            }
        }
    }
}

void TouchInput::OnTouchUp(const SDL_TouchFingerEvent& event) {
    TouchPoint* touch = GetTouchPoint(event.fingerId);
    if (touch && touch->active) {
        touch->active = false;
        
        // Update joystick
        if (joystick) {
            joystick->OnTouchUp();
        }
        
        // Update buttons
        for (auto button : buttons) {
            button->OnTouchUp();
        }
        
        // Process gesture
        Gesture gesture;
        gesture.startX = touch->x;
        gesture.startY = touch->y;
        gesture.endX = event.x;
        gesture.endY = event.y;
        gesture.startTime = touch->timestamp;
        gesture.endTime = SDL_GetTicks();
        gesture.isSwipe = IsSwipe(50.0f, 500.0f); // 50px, 500ms
        gesture.isTap = IsTap(20.0f, 200.0f); // 20px, 200ms
        
        gestures.push_back(gesture);
    }
}

void TouchInput::OnTouchMotion(const SDL_TouchFingerEvent& event) {
    TouchPoint* touch = GetTouchPoint(event.fingerId);
    if (touch && touch->active) {
        touch->x = event.x;
        touch->y = event.y;
        
        // Update joystick
        if (joystick) {
            joystick->OnTouchMotion(touch->x, touch->y);
        }
    }
}

void TouchInput::ProcessGestures() {
    for (auto& gesture : gestures) {
        if (gesture.isTap) {
            // Handle tap gesture
            OnTapGesture(gesture.endX, gesture.endY);
        } else if (gesture.isSwipe) {
            // Handle swipe gesture
            OnSwipeGesture(gesture.startX, gesture.startY, gesture.endX, gesture.endY);
        }
    }
    gestures.clear();
}

bool TouchInput::IsSwipe(float minDistance, float maxTime) {
    // Calculate distance and time
    float dx = gestures.back().endX - gestures.back().startX;
    float dy = gestures.back().endY - gestures.back().startY;
    float distance = sqrt(dx * dx + dy * dy);
    float time = gestures.back().endTime - gestures.back().startTime;
    
    return distance >= minDistance && time <= maxTime;
}

bool TouchInput::IsTap(float maxDistance, float maxTime) {
    // Calculate distance and time
    float dx = gestures.back().endX - gestures.back().startX;
    float dy = gestures.back().endY - gestures.back().startY;
    float distance = sqrt(dx * dx + dy * dy);
    float time = gestures.back().endTime - gestures.back().startTime;
    
    return distance <= maxDistance && time <= maxTime;
}

void TouchInput::OnTapGesture(float x, float y) {
    // Handle tap at position (x, y)
    // This could trigger actions like auto-attack, item pickup, etc.
}

void TouchInput::OnSwipeGesture(float startX, float startY, float endX, float endY) {
    // Handle swipe gesture
    // This could trigger special moves, camera rotation, etc.
}
```

### VirtualJoystick.h
```cpp
#pragma once
#include <functional>

class VirtualJoystick {
private:
    float centerX, centerY;
    float radius;
    float currentX, currentY;
    bool active;
    float deadZone;
    
    std::function<void(float, float)> onMoveCallback;
    
public:
    VirtualJoystick();
    
    void SetPosition(float x, float y);
    void SetRadius(float r);
    void SetDeadZone(float zone);
    
    void OnTouchDown(float x, float y);
    void OnTouchUp();
    void OnTouchMotion(float x, float y);
    
    void Update();
    
    bool IsPointInside(float x, float y) const;
    
    // Getters
    float GetCenterX() const { return centerX; }
    float GetCenterY() const { return centerY; }
    float GetCurrentX() const { return currentX; }
    float GetCurrentY() const { return currentY; }
    float GetRadius() const { return radius; }
    bool IsActive() const { return active; }
    
    // Movement
    float GetDirectionX() const;
    float GetDirectionY() const;
    float GetMagnitude() const;
    
    // Callbacks
    void SetOnMoveCallback(std::function<void(float, float)> callback) {
        onMoveCallback = callback;
    }
    
private:
    void UpdatePosition(float x, float y);
    void ClampToRadius(float& x, float& y);
};
```

### VirtualJoystick.cpp
```cpp
#include "VirtualJoystick.h"
#include <cmath>

VirtualJoystick::VirtualJoystick() 
    : centerX(0), centerY(0), radius(50.0f), currentX(0), currentY(0), 
      active(false), deadZone(10.0f) {
}

void VirtualJoystick::SetPosition(float x, float y) {
    centerX = x;
    centerY = y;
    currentX = x;
    currentY = y;
}

void VirtualJoystick::SetRadius(float r) {
    radius = r;
}

void VirtualJoystick::SetDeadZone(float zone) {
    deadZone = zone;
}

void VirtualJoystick::OnTouchDown(float x, float y) {
    active = true;
    UpdatePosition(x, y);
}

void VirtualJoystick::OnTouchUp() {
    active = false;
    currentX = centerX;
    currentY = centerY;
    
    // Notify callback
    if (onMoveCallback) {
        onMoveCallback(0, 0);
    }
}

void VirtualJoystick::OnTouchMotion(float x, float y) {
    if (active) {
        UpdatePosition(x, y);
    }
}

void VirtualJoystick::Update() {
    // Apply dead zone
    float magnitude = GetMagnitude();
    if (magnitude < deadZone) {
        currentX = centerX;
        currentY = centerY;
    }
}

bool VirtualJoystick::IsPointInside(float x, float y) const {
    float dx = x - centerX;
    float dy = y - centerY;
    return (dx * dx + dy * dy) <= (radius * radius);
}

float VirtualJoystick::GetDirectionX() const {
    if (!active) return 0.0f;
    
    float dx = currentX - centerX;
    float dy = currentY - centerY;
    float length = sqrt(dx * dx + dy * dy);
    
    if (length > 0) {
        return dx / length;
    }
    return 0.0f;
}

float VirtualJoystick::GetDirectionY() const {
    if (!active) return 0.0f;
    
    float dx = currentX - centerX;
    float dy = currentY - centerY;
    float length = sqrt(dx * dx + dy * dy);
    
    if (length > 0) {
        return dy / length;
    }
    return 0.0f;
}

float VirtualJoystick::GetMagnitude() const {
    if (!active) return 0.0f;
    
    float dx = currentX - centerX;
    float dy = currentY - centerY;
    return sqrt(dx * dx + dy * dy);
}

void VirtualJoystick::UpdatePosition(float x, float y) {
    float dx = x - centerX;
    float dy = y - centerY;
    float distance = sqrt(dx * dx + dy * dy);
    
    if (distance <= radius) {
        currentX = x;
        currentY = y;
    } else {
        currentX = centerX + (dx / distance) * radius;
        currentY = centerY + (dy / distance) * radius;
    }
    
    // Notify callback
    if (onMoveCallback) {
        onMoveCallback(GetDirectionX(), GetDirectionY());
    }
}

void VirtualJoystick::ClampToRadius(float& x, float& y) {
    float dx = x - centerX;
    float dy = y - centerY;
    float distance = sqrt(dx * dx + dy * dy);
    
    if (distance > radius) {
        x = centerX + (dx / distance) * radius;
        y = centerY + (dy / distance) * radius;
    }
}
```

## 3. Game Integration

### GameManager.h (Modified)
```cpp
#pragma once
#include "Character.h"
#include "NetworkManager.h"
#include "MobileUI.h"

class GameManager {
private:
    Character* currentCharacter;
    NetworkManager* networkManager;
    MobileUI* mobileUI;
    
    bool isInitialized;
    float lastUpdateTime;
    
public:
    GameManager();
    ~GameManager();
    
    bool Initialize();
    void Shutdown();
    
    void Update(float deltaTime);
    void Render();
    
    // Getters
    Character* GetCurrentCharacter() const { return currentCharacter; }
    NetworkManager* GetNetworkManager() const { return networkManager; }
    MobileUI* GetUI() const { return mobileUI; }
    
    // Game state
    void SetCurrentCharacter(Character* character);
    void HandleMovement(float x, float y);
    void HandleSkillUse(int skillId);
    
private:
    void UpdateCharacter(float deltaTime);
    void UpdateNetwork(float deltaTime);
    void UpdateUI(float deltaTime);
};
```

### GameManager.cpp (Modified)
```cpp
#include "GameManager.h"
#include "TouchInput.h"

GameManager::GameManager() 
    : currentCharacter(nullptr), networkManager(nullptr), mobileUI(nullptr), 
      isInitialized(false), lastUpdateTime(0) {
}

GameManager::~GameManager() {
    Shutdown();
}

bool GameManager::Initialize() {
    // Initialize network manager
    networkManager = new NetworkManager();
    if (!networkManager->Initialize()) {
        return false;
    }
    
    // Initialize mobile UI
    mobileUI = new MobileUI();
    mobileUI->Initialize();
    
    // Setup joystick callback
    auto joystick = g_TouchInput->GetJoystick();
    if (joystick) {
        joystick->SetOnMoveCallback([this](float x, float y) {
            HandleMovement(x, y);
        });
    }
    
    isInitialized = true;
    return true;
}

void GameManager::Shutdown() {
    delete mobileUI;
    delete networkManager;
    delete currentCharacter;
    
    mobileUI = nullptr;
    networkManager = nullptr;
    currentCharacter = nullptr;
    isInitialized = false;
}

void GameManager::Update(float deltaTime) {
    if (!isInitialized) return;
    
    // Update network
    UpdateNetwork(deltaTime);
    
    // Update character
    UpdateCharacter(deltaTime);
    
    // Update UI
    UpdateUI(deltaTime);
    
    lastUpdateTime += deltaTime;
}

void GameManager::Render() {
    if (!isInitialized) return;
    
    // Render 3D scene
    g_Renderer->Render3DScene();
    
    // Render UI
    g_Renderer->RenderUI();
}

void GameManager::HandleMovement(float x, float y) {
    if (currentCharacter) {
        // Convert joystick input to character movement
        Vector3 direction(x, 0, y);
        currentCharacter->Move(direction);
        
        // Send movement packet to server
        if (networkManager) {
            networkManager->SendMovementPacket(direction);
        }
    }
}

void GameManager::HandleSkillUse(int skillId) {
    if (currentCharacter) {
        // Use skill
        currentCharacter->UseSkill(skillId);
        
        // Send skill packet to server
        if (networkManager) {
            networkManager->SendSkillPacket(skillId);
        }
    }
}

void GameManager::UpdateCharacter(float deltaTime) {
    if (currentCharacter) {
        currentCharacter->Update(deltaTime);
    }
}

void GameManager::UpdateNetwork(float deltaTime) {
    if (networkManager) {
        networkManager->Update(deltaTime);
    }
}

void GameManager::UpdateUI(float deltaTime) {
    if (mobileUI) {
        mobileUI->Update(deltaTime);
    }
}

void GameManager::SetCurrentCharacter(Character* character) {
    currentCharacter = character;
}
```

## 4. Build Configuration

### CMakeLists.txt (Complete)
```cmake
cmake_minimum_required(VERSION 3.16)
project(MU_Mobile_SDL2)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(ANDROID)
    set(PLATFORM_ANDROID TRUE)
    set(ANDROID TRUE)
elseif(IOS)
    set(PLATFORM_IOS TRUE)
    set(IOS TRUE)
elseif(WIN32)
    set(PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
elseif(UNIX)
    set(PLATFORM_LINUX TRUE)
endif()

# SDL2
if(PLATFORM_ANDROID)
    # Android SDL2
    set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/sdl2")
    set(SDL2_INCLUDE_DIRS "${SDL2_DIR}/include")
    set(SDL2_LIBRARIES "${SDL2_DIR}/lib/${ANDROID_ABI}/libSDL2.so")
elseif(PLATFORM_IOS)
    # iOS SDL2
    set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/sdl2")
    set(SDL2_INCLUDE_DIRS "${SDL2_DIR}/include")
    set(SDL2_LIBRARIES "${SDL2_DIR}/lib/libSDL2.a")
else()
    # Desktop SDL2
    find_package(SDL2 REQUIRED)
    find_package(SDL2_image REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    find_package(SDL2_mixer REQUIRED)
endif()

# OpenGL
if(PLATFORM_ANDROID)
    find_library(OPENGLES2_LIBRARY GLESv2)
    find_library(EGL_LIBRARY EGL)
elseif(PLATFORM_IOS)
    # iOS uses OpenGL ES
else()
    find_package(OpenGL REQUIRED)
endif()

# Source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/core/*.cpp"
    "src/renderer/*.cpp"
    "src/input/*.cpp"
    "src/ui/*.cpp"
    "src/platform/*.cpp"
)

# Create executable/library
if(PLATFORM_ANDROID)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
elseif(PLATFORM_IOS)
    add_library(${PROJECT_NAME} STATIC ${SOURCES})
else()
    add_executable(${PROJECT_NAME} ${SOURCES})
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    "src"
    "src/core"
    "src/renderer"
    "src/input"
    "src/ui"
    "src/platform"
)

# Link libraries
if(PLATFORM_ANDROID)
    target_link_libraries(${PROJECT_NAME}
        ${SDL2_LIBRARIES}
        ${OPENGLES2_LIBRARY}
        ${EGL_LIBRARY}
        log
        android
    )
elseif(PLATFORM_IOS)
    target_link_libraries(${PROJECT_NAME}
        ${SDL2_LIBRARIES}
        "-framework Foundation"
        "-framework UIKit"
        "-framework OpenGLES"
        "-framework AudioToolbox"
        "-framework CoreAudio"
    )
else()
    target_link_libraries(${PROJECT_NAME}
        SDL2::SDL2
        SDL2_image::SDL2_image
        SDL2_ttf::SDL2_ttf
        SDL2_mixer::SDL2_mixer
        OpenGL::GL
    )
endif()

# Compiler flags
if(PLATFORM_ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        ANDROID
        __ANDROID__
    )
elseif(PLATFORM_IOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        IOS
        __IOS__
    )
endif()

# Installation
if(NOT PLATFORM_ANDROID AND NOT PLATFORM_IOS)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif()
```

## 5. Platform-Specific Files

### AndroidManifest.xml
```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.mumobile.client">

    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
    <uses-permission android:name="android.permission.WAKE_LOCK" />
    <uses-permission android:name="android.permission.VIBRATE" />

    <application
        android:allowBackup="true"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:theme="@style/AppTheme"
        android:hardwareAccelerated="true">

        <activity
            android:name=".MainActivity"
            android:label="@string/app_name"
            android:screenOrientation="landscape"
            android:configChanges="orientation|keyboardHidden|screenSize"
            android:theme="@style/AppTheme.NoActionBar">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>

    </application>

</manifest>
```

### MainActivity.java
```java
package com.mumobile.client;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;

public class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Hide status bar and navigation bar
        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        );
        
        // Hide system UI
        View decorView = getWindow().getDecorView();
        int uiOptions = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                     | View.SYSTEM_UI_FLAG_FULLSCREEN
                     | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        decorView.setSystemUiVisibility(uiOptions);
        
        // Load native library
        System.loadLibrary("MU_Mobile_Android");
    }
    
    @Override
    protected void onResume() {
        super.onResume();
        
        // Hide system UI again
        View decorView = getWindow().getDecorView();
        int uiOptions = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                     | View.SYSTEM_UI_FLAG_FULLSCREEN
                     | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        decorView.setSystemUiVisibility(uiOptions);
    }
}
```

## Conclusion

Với implementation này, bạn có thể:

1. **Giữ nguyên 90% code C++** hiện có
2. **Chỉ thay đổi rendering và input** cho mobile
3. **Tái sử dụng network protocol** và game logic
4. **Performance cao** với native C++
5. **Size nhỏ** chỉ thêm 2-3MB SDL2 library

**Key Benefits:**
- ✅ Minimal code changes
- ✅ High performance
- ✅ Small app size
- ✅ Easy maintenance
- ✅ Cross-platform support

**Next Steps:**
1. Setup SDL2 development environment
2. Copy existing C++ files vào src/core/
3. Implement SDL2 rendering wrapper
4. Test với basic rendering
5. Add touch input system
6. Integrate với existing game logic

Với approach này, bạn có thể port MU Online sang mobile một cách hiệu quả và giữ nguyên được chất lượng game gốc! 🎮📱