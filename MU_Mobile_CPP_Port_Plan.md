# Kế hoạch Port MU Online Client sang Mobile (SDL2/Cocos2d-x)

## Tổng quan

Port MU Online Client từ C++ sang mobile sử dụng **SDL2** hoặc **Cocos2d-x** để:
- **Giữ nguyên mã nguồn C++** hiện có
- **Tái sử dụng logic game** (network, game logic, data structures)
- **Chỉ thay đổi rendering và input** cho mobile
- **Tối ưu performance** với native code

## So sánh SDL2 vs Cocos2d-x

### SDL2 (Simple DirectMedia Layer 2)
**Ưu điểm:**
- ✅ **Cross-platform**: Android, iOS, Windows, Linux, macOS
- ✅ **Lightweight**: Chỉ 2-3MB library
- ✅ **Performance**: Native C/C++ performance
- ✅ **Flexibility**: Full control over rendering
- ✅ **Mature**: Stable và well-documented
- ✅ **Easy migration**: Minimal code changes

**Nhược điểm:**
- ❌ **Manual work**: Cần tự implement UI, audio, physics
- ❌ **No built-in features**: Không có game engine features
- ❌ **More coding**: Cần viết nhiều boilerplate code

### Cocos2d-x
**Ưu điểm:**
- ✅ **Game engine features**: Built-in UI, audio, physics
- ✅ **Rich ecosystem**: Many tools và libraries
- ✅ **Mobile optimized**: Designed for mobile games
- ✅ **Less coding**: More features out of the box

**Nhược điểm:**
- ❌ **Larger size**: 10-20MB library
- ❌ **Learning curve**: More complex API
- ❌ **Overhead**: Engine overhead
- ❌ **Less control**: Limited low-level access

## Khuyến nghị: SDL2

**Lý do chọn SDL2:**
1. **Tương thích tốt** với code C++ hiện có
2. **Performance cao** cho MMORPG
3. **Dễ migration** từ OpenGL hiện tại
4. **Size nhỏ** phù hợp mobile
5. **Full control** over rendering pipeline

## Kiến trúc Port với SDL2

### 1. Project Structure
```
MU_Mobile_SDL2/
├── src/
│   ├── core/                    # Giữ nguyên từ C++ gốc
│   │   ├── network/
│   │   ├── game/
│   │   ├── data/
│   │   └── utils/
│   ├── platform/               # Platform-specific code
│   │   ├── android/
│   │   ├── ios/
│   │   └── desktop/
│   ├── renderer/               # SDL2 rendering wrapper
│   │   ├── SDLRenderer.cpp
│   │   ├── SDLTexture.cpp
│   │   └── SDLWindow.cpp
│   ├── input/                  # Mobile input handling
│   │   ├── TouchInput.cpp
│   │   ├── VirtualJoystick.cpp
│   │   └── GestureHandler.cpp
│   └── ui/                     # Mobile UI system
│       ├── MobileUI.cpp
│       ├── TouchButton.cpp
│       └── VirtualKeyboard.cpp
├── assets/                     # Game assets
├── android/                    # Android project files
├── ios/                        # iOS project files
├── CMakeLists.txt
└── README.md
```

### 2. Migration Strategy

#### Phase 1: Core Systems (Giữ nguyên)
```cpp
// Giữ nguyên các file này
- WSclient.cpp (network)
- ZzzCharacter.cpp (character logic)
- ZzzInventory.cpp (inventory)
- _struct.h (data structures)
- _enum.h (enums)
```

#### Phase 2: Rendering Layer (Thay thế)
```cpp
// Thay thế OpenGL với SDL2
- ZzzOpenglUtil.cpp → SDLRenderer.cpp
- ZzzTexture.cpp → SDLTexture.cpp
- ZzzScene.cpp → SDLScene.cpp
```

#### Phase 3: Input Layer (Thay thế)
```cpp
// Thay thế Windows input với mobile input
- Winmain.cpp → MobileMain.cpp
- Input handling → TouchInput.cpp
```

#### Phase 4: Platform Layer (Mới)
```cpp
// Thêm platform-specific code
- AndroidMain.cpp
- iOSMain.cpp
- PlatformUtils.cpp
```

## Implementation Details

### 1. SDL2 Integration

#### CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.16)
project(MU_Mobile_SDL2)

# SDL2
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_mixer REQUIRED)

# OpenGL (for existing code)
find_package(OpenGL REQUIRED)

# Platform-specific
if(ANDROID)
    set(PLATFORM_ANDROID TRUE)
elseif(IOS)
    set(PLATFORM_IOS TRUE)
endif()

# Source files
file(GLOB_RECURSE SOURCES 
    "src/core/*.cpp"
    "src/renderer/*.cpp"
    "src/input/*.cpp"
    "src/ui/*.cpp"
    "src/platform/*.cpp"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
    SDL2_mixer::SDL2_mixer
    OpenGL::GL
)

# Platform-specific settings
if(PLATFORM_ANDROID)
    target_link_libraries(${PROJECT_NAME} android log)
elseif(PLATFORM_IOS)
    target_link_libraries(${PROJECT_NAME} "-framework Foundation" "-framework UIKit")
endif()
```

#### SDLRenderer.cpp
```cpp
#include "SDLRenderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

class SDLRenderer {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_GLContext glContext;
    
public:
    bool Initialize(int width, int height, const char* title) {
        // Initialize SDL2
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            return false;
        }
        
        // Create window with OpenGL support
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        
        window = SDL_CreateWindow(
            title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
        );
        
        if (!window) {
            return false;
        }
        
        // Create OpenGL context
        glContext = SDL_GL_CreateContext(window);
        if (!glContext) {
            return false;
        }
        
        // Initialize OpenGL (existing code)
        InitializeOpenGL();
        
        return true;
    }
    
    void Render() {
        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Render 3D scene (existing OpenGL code)
        Render3DScene();
        
        // Render 2D UI
        Render2DUI();
        
        // Swap buffers
        SDL_GL_SwapWindow(window);
    }
    
    void Shutdown() {
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    
private:
    void InitializeOpenGL() {
        // Existing OpenGL initialization code
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    
    void Render3DScene() {
        // Existing 3D rendering code from ZzzScene.cpp
        // This code remains largely unchanged
    }
    
    void Render2DUI() {
        // New 2D UI rendering for mobile
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        // Render mobile UI elements
        RenderVirtualJoystick();
        RenderSkillButtons();
        RenderInventoryUI();
        
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
};
```

### 2. Mobile Input System

#### TouchInput.cpp
```cpp
#include "TouchInput.h"
#include <SDL2/SDL.h>

class TouchInput {
private:
    struct TouchPoint {
        int id;
        float x, y;
        bool active;
    };
    
    TouchPoint touches[10]; // Support up to 10 touch points
    VirtualJoystick* joystick;
    std::vector<TouchButton*> buttons;
    
public:
    void Initialize() {
        // Enable touch events
        SDL_SetHint(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1");
        
        // Initialize virtual joystick
        joystick = new VirtualJoystick();
        joystick->SetPosition(100, 400); // Bottom-left corner
    }
    
    void HandleEvent(const SDL_Event& event) {
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
    
private:
    void OnTouchDown(const SDL_TouchFingerEvent& event) {
        TouchPoint* touch = GetTouchPoint(event.fingerId);
        if (touch) {
            touch->id = event.fingerId;
            touch->x = event.x * windowWidth;
            touch->y = event.y * windowHeight;
            touch->active = true;
            
            // Check if touch is on joystick
            if (joystick->IsPointInside(touch->x, touch->y)) {
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
    
    void OnTouchUp(const SDL_TouchFingerEvent& event) {
        TouchPoint* touch = GetTouchPoint(event.fingerId);
        if (touch && touch->active) {
            touch->active = false;
            
            joystick->OnTouchUp();
            
            for (auto button : buttons) {
                button->OnTouchUp();
            }
        }
    }
    
    void OnTouchMotion(const SDL_TouchFingerEvent& event) {
        TouchPoint* touch = GetTouchPoint(event.fingerId);
        if (touch && touch->active) {
            touch->x = event.x * windowWidth;
            touch->y = event.y * windowHeight;
            
            joystick->OnTouchMotion(touch->x, touch->y);
        }
    }
    
    TouchPoint* GetTouchPoint(SDL_FingerID id) {
        for (int i = 0; i < 10; i++) {
            if (touches[i].id == id || !touches[i].active) {
                return &touches[i];
            }
        }
        return nullptr;
    }
};
```

#### VirtualJoystick.cpp
```cpp
#include "VirtualJoystick.h"

class VirtualJoystick {
private:
    float centerX, centerY;
    float radius;
    float currentX, currentY;
    bool active;
    
public:
    VirtualJoystick() : active(false), radius(50.0f) {}
    
    void SetPosition(float x, float y) {
        centerX = x;
        centerY = y;
        currentX = x;
        currentY = y;
    }
    
    bool IsPointInside(float x, float y) {
        float dx = x - centerX;
        float dy = y - centerY;
        return (dx * dx + dy * dy) <= (radius * radius);
    }
    
    void OnTouchDown(float x, float y) {
        active = true;
        UpdatePosition(x, y);
    }
    
    void OnTouchUp() {
        active = false;
        currentX = centerX;
        currentY = centerY;
    }
    
    void OnTouchMotion(float x, float y) {
        if (active) {
            UpdatePosition(x, y);
        }
    }
    
    void Render() {
        // Render joystick background
        glColor4f(0.2f, 0.2f, 0.2f, 0.7f);
        RenderCircle(centerX, centerY, radius);
        
        // Render joystick handle
        glColor4f(0.8f, 0.8f, 0.8f, 0.9f);
        RenderCircle(currentX, currentY, 20.0f);
        
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    Vector2 GetDirection() {
        if (!active) return Vector2(0, 0);
        
        float dx = currentX - centerX;
        float dy = currentY - centerY;
        float length = sqrt(dx * dx + dy * dy);
        
        if (length > 0) {
            return Vector2(dx / length, dy / length);
        }
        return Vector2(0, 0);
    }
    
private:
    void UpdatePosition(float x, float y) {
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
    }
    
    void RenderCircle(float x, float y, float r) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int i = 0; i <= 32; i++) {
            float angle = 2.0f * M_PI * i / 32.0f;
            glVertex2f(x + r * cos(angle), y + r * sin(angle));
        }
        glEnd();
    }
};
```

### 3. Mobile UI System

#### MobileUI.cpp
```cpp
#include "MobileUI.h"

class MobileUI {
private:
    std::vector<TouchButton*> skillButtons;
    TouchButton* inventoryButton;
    TouchButton* characterButton;
    TouchButton* menuButton;
    
public:
    void Initialize() {
        // Create skill buttons (bottom-right)
        for (int i = 0; i < 4; i++) {
            TouchButton* button = new TouchButton();
            button->SetPosition(600 + i * 80, 400);
            button->SetSize(60, 60);
            button->SetText(std::to_string(i + 1));
            button->SetCallback([i]() { OnSkillButtonPressed(i); });
            skillButtons.push_back(button);
        }
        
        // Create menu buttons (top-right)
        inventoryButton = new TouchButton();
        inventoryButton->SetPosition(700, 50);
        inventoryButton->SetSize(50, 50);
        inventoryButton->SetText("I");
        inventoryButton->SetCallback([]() { ToggleInventory(); });
        
        characterButton = new TouchButton();
        characterButton->SetPosition(760, 50);
        characterButton->SetSize(50, 50);
        characterButton->SetText("C");
        characterButton->SetCallback([]() { ToggleCharacter(); });
        
        menuButton = new TouchButton();
        menuButton->SetPosition(820, 50);
        menuButton->SetSize(50, 50);
        menuButton->SetText("M");
        menuButton->SetCallback([]() { ToggleMenu(); });
    }
    
    void Render() {
        // Render skill buttons
        for (auto button : skillButtons) {
            button->Render();
        }
        
        // Render menu buttons
        inventoryButton->Render();
        characterButton->Render();
        menuButton->Render();
        
        // Render status bars
        RenderStatusBars();
    }
    
private:
    void RenderStatusBars() {
        // Health bar
        glColor4f(1.0f, 0.0f, 0.0f, 0.8f);
        RenderBar(50, 50, 200, 20, character->GetHealthPercent());
        
        // Mana bar
        glColor4f(0.0f, 0.0f, 1.0f, 0.8f);
        RenderBar(50, 80, 200, 20, character->GetManaPercent());
        
        // Experience bar
        glColor4f(0.0f, 1.0f, 0.0f, 0.8f);
        RenderBar(50, 110, 200, 15, character->GetExpPercent());
        
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    void RenderBar(float x, float y, float width, float height, float percent) {
        // Background
        glColor4f(0.2f, 0.2f, 0.2f, 0.8f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + width, y);
        glVertex2f(x + width, y + height);
        glVertex2f(x, y + height);
        glEnd();
        
        // Fill
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + width * percent, y);
        glVertex2f(x + width * percent, y + height);
        glVertex2f(x, y + height);
        glEnd();
    }
    
    void OnSkillButtonPressed(int skillId) {
        // Send skill packet to server
        SendSkillPacket(skillId);
    }
    
    void ToggleInventory() {
        // Toggle inventory UI
        inventoryUI->Toggle();
    }
    
    void ToggleCharacter() {
        // Toggle character UI
        characterUI->Toggle();
    }
    
    void ToggleMenu() {
        // Toggle main menu
        menuUI->Toggle();
    }
};
```

### 4. Platform-Specific Code

#### AndroidMain.cpp
```cpp
#include <jni.h>
#include <android/native_activity.h>
#include <SDL2/SDL.h>

extern "C" {
    JNIEXPORT int JNICALL SDL_main(int argc, char* argv[]) {
        // Initialize SDL2
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            return -1;
        }
        
        // Create window
        SDL_Window* window = SDL_CreateWindow(
            "MU Mobile",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            800, 600,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );
        
        if (!window) {
            return -1;
        }
        
        // Initialize game
        GameManager* game = new GameManager();
        game->Initialize();
        
        // Main game loop
        bool running = true;
        SDL_Event event;
        
        while (running) {
            // Handle events
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                
                // Handle touch input
                touchInput->HandleEvent(event);
            }
            
            // Update game logic
            game->Update();
            
            // Render
            game->Render();
            
            // Cap frame rate
            SDL_Delay(16); // ~60 FPS
        }
        
        // Cleanup
        delete game;
        SDL_DestroyWindow(window);
        SDL_Quit();
        
        return 0;
    }
}
```

#### iOSMain.cpp
```cpp
#include <SDL2/SDL.h>
#include <UIKit/UIKit.h>

int main(int argc, char* argv[]) {
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        return -1;
    }
    
    // Get screen size
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
        return -1;
    }
    
    // Initialize game
    GameManager* game = new GameManager();
    game->Initialize();
    
    // Main game loop
    bool running = true;
    SDL_Event event;
    
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            
            // Handle touch input
            touchInput->HandleEvent(event);
        }
        
        // Update game logic
        game->Update();
        
        // Render
        game->Render();
        
        // Cap frame rate
        SDL_Delay(16); // ~60 FPS
    }
    
    // Cleanup
    delete game;
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
```

## Build Configuration

### Android Build (CMake + Gradle)

#### app/build.gradle
```gradle
android {
    compileSdkVersion 33
    defaultConfig {
        applicationId "com.mumobile.client"
        minSdkVersion 21
        targetSdkVersion 33
        versionCode 1
        versionName "1.0"
        
        externalNativeBuild {
            cmake {
                cppFlags "-std=c++17"
                arguments "-DANDROID_STL=c++_shared"
            }
        }
    }
    
    externalNativeBuild {
        cmake {
            path "CMakeLists.txt"
        }
    }
    
    buildTypes {
        release {
            minifyEnabled false
            proguardFiles getDefaultProguardFile('proguard-android.txt'), 'proguard-rules.pro'
        }
    }
}

dependencies {
    implementation fileTree(dir: 'libs', include: ['*.jar'])
}
```

#### CMakeLists.txt (Android)
```cmake
cmake_minimum_required(VERSION 3.16)
project(MU_Mobile_Android)

# SDL2 for Android
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sdl2")
set(SDL2_INCLUDE_DIRS "${SDL2_DIR}/include")
set(SDL2_LIBRARIES "${SDL2_DIR}/lib/${ANDROID_ABI}/libSDL2.so")

# Source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/core/*.cpp"
    "src/renderer/*.cpp"
    "src/input/*.cpp"
    "src/ui/*.cpp"
)

# Create shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    "src"
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${SDL2_LIBRARIES}
    log
    android
    GLESv2
    EGL
)
```

### iOS Build (Xcode)

#### CMakeLists.txt (iOS)
```cmake
cmake_minimum_required(VERSION 3.16)
project(MU_Mobile_iOS)

# SDL2 for iOS
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sdl2")
set(SDL2_INCLUDE_DIRS "${SDL2_DIR}/include")
set(SDL2_LIBRARIES "${SDL2_DIR}/lib/libSDL2.a")

# Source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/core/*.cpp"
    "src/renderer/*.cpp"
    "src/input/*.cpp"
    "src/ui/*.cpp"
)

# Create static library
add_library(${PROJECT_NAME} STATIC ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    "src"
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${SDL2_LIBRARIES}
    "-framework Foundation"
    "-framework UIKit"
    "-framework OpenGLES"
    "-framework AudioToolbox"
    "-framework CoreAudio"
)
```

## Performance Optimization

### 1. Memory Management
```cpp
// Object pooling for frequently created objects
class ObjectPool {
private:
    std::queue<GameObject*> pool;
    std::function<GameObject*()> createFunc;
    
public:
    GameObject* GetObject() {
        if (pool.empty()) {
            return createFunc();
        }
        
        GameObject* obj = pool.front();
        pool.pop();
        obj->Reset();
        return obj;
    }
    
    void ReturnObject(GameObject* obj) {
        pool.push(obj);
    }
};
```

### 2. Texture Management
```cpp
// Texture atlas for mobile
class TextureAtlas {
private:
    GLuint textureId;
    std::map<std::string, TextureRegion> regions;
    
public:
    void LoadAtlas(const std::string& filename) {
        // Load texture atlas
        // Parse region definitions
    }
    
    void BindTexture() {
        glBindTexture(GL_TEXTURE_2D, textureId);
    }
    
    void RenderRegion(const std::string& name, float x, float y) {
        auto& region = regions[name];
        // Render specific region
    }
};
```

### 3. Rendering Optimization
```cpp
// Batch rendering for UI elements
class UIRenderer {
private:
    struct UIVertex {
        float x, y, u, v;
        uint32_t color;
    };
    
    std::vector<UIVertex> vertices;
    GLuint vbo;
    
public:
    void BeginBatch() {
        vertices.clear();
    }
    
    void AddQuad(float x, float y, float w, float h, float u, float v, float uw, float vh) {
        // Add quad vertices to batch
    }
    
    void EndBatch() {
        // Render all vertices at once
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(UIVertex), vertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, vertices.size());
    }
};
```

## Testing Strategy

### 1. Unit Tests
```cpp
#include <gtest/gtest.h>

TEST(CharacterTest, Movement) {
    Character character;
    character.SetPosition(0, 0);
    character.Move(Vector2(1, 0));
    EXPECT_EQ(character.GetPosition().x, 1.0f);
}

TEST(NetworkTest, PacketSerialization) {
    Packet packet;
    packet.WriteInt(123);
    packet.WriteString("test");
    
    EXPECT_EQ(packet.ReadInt(), 123);
    EXPECT_EQ(packet.ReadString(), "test");
}
```

### 2. Performance Tests
```cpp
class PerformanceTest {
public:
    void TestFrameRate() {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; i++) {
            game->Update();
            game->Render();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        float fps = 1000.0f / (duration.count() / 1000.0f);
        EXPECT_GT(fps, 30.0f); // Minimum 30 FPS
    }
};
```

## Deployment

### Android APK
```bash
# Build APK
./gradlew assembleRelease

# Sign APK
jarsigner -verbose -sigalg SHA1withRSA -digestalg SHA1 -keystore keystore.jks app-release-unsigned.apk alias_name

# Optimize APK
zipalign -v 4 app-release-unsigned.apk MU_Mobile.apk
```

### iOS App
```bash
# Build for device
xcodebuild -project MU_Mobile.xcodeproj -scheme MU_Mobile -configuration Release -destination generic/platform=iOS

# Archive for App Store
xcodebuild -project MU_Mobile.xcodeproj -scheme MU_Mobile -configuration Release archive -archivePath MU_Mobile.xcarchive
```

## Timeline & Milestones

### Phase 1: Foundation (2-3 tháng)
- [ ] Setup SDL2 development environment
- [ ] Create basic project structure
- [ ] Implement SDL2 rendering wrapper
- [ ] Test basic rendering

### Phase 2: Input System (1-2 tháng)
- [ ] Implement touch input handling
- [ ] Create virtual joystick
- [ ] Implement gesture recognition
- [ ] Test on real devices

### Phase 3: UI System (2-3 tháng)
- [ ] Create mobile UI framework
- [ ] Implement skill buttons
- [ ] Create inventory UI
- [ ] Add status bars

### Phase 4: Integration (2-3 tháng)
- [ ] Integrate with existing game logic
- [ ] Test network functionality
- [ ] Optimize performance
- [ ] Fix bugs

### Phase 5: Polish & Testing (1-2 tháng)
- [ ] Comprehensive testing
- [ ] Performance optimization
- [ ] UI polish
- [ ] Final builds

## Conclusion

Port MU Online Client sang mobile sử dụng SDL2 là lựa chọn tối ưu vì:

1. **Giữ nguyên code C++**: Tái sử dụng 90% logic hiện có
2. **Performance cao**: Native C++ performance
3. **Size nhỏ**: Chỉ thêm 2-3MB library
4. **Dễ maintain**: Ít thay đổi code
5. **Cross-platform**: Android và iOS

**Key Benefits:**
- ✅ Minimal code changes
- ✅ High performance
- ✅ Small app size
- ✅ Easy maintenance
- ✅ Proven technology

**Next Steps:**
1. Setup SDL2 development environment
2. Create basic rendering wrapper
3. Implement touch input system
4. Test với existing game logic
5. Optimize cho mobile performance

Với approach này, bạn có thể port MU Online sang mobile một cách hiệu quả và giữ nguyên được chất lượng game gốc! 🎮📱