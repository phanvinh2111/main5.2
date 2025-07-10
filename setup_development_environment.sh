#!/bin/bash

# MU Mobile SDL2 Development Environment Setup Script
# This script sets up the development environment for porting MU Online to mobile using SDL2

echo "🚀 Setting up MU Mobile SDL2 Development Environment..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running on supported platform
check_platform() {
    print_status "Checking platform..."
    
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        PLATFORM="linux"
        print_success "Detected Linux platform"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        PLATFORM="macos"
        print_success "Detected macOS platform"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        PLATFORM="windows"
        print_success "Detected Windows platform"
    else
        print_error "Unsupported platform: $OSTYPE"
        exit 1
    fi
}

# Install system dependencies
install_system_deps() {
    print_status "Installing system dependencies..."
    
    if [[ "$PLATFORM" == "linux" ]]; then
        # Ubuntu/Debian
        if command -v apt-get &> /dev/null; then
            sudo apt-get update
            sudo apt-get install -y \
                build-essential \
                cmake \
                git \
                libsdl2-dev \
                libsdl2-image-dev \
                libsdl2-ttf-dev \
                libsdl2-mixer-dev \
                libopengl-dev \
                libglu1-mesa-dev \
                pkg-config \
                wget \
                unzip
        # CentOS/RHEL/Fedora
        elif command -v yum &> /dev/null; then
            sudo yum install -y \
                gcc-c++ \
                cmake \
                git \
                SDL2-devel \
                SDL2_image-devel \
                SDL2_ttf-devel \
                SDL2_mixer-devel \
                mesa-libGL-devel \
                mesa-libGLU-devel \
                pkg-config \
                wget \
                unzip
        else
            print_error "Unsupported Linux distribution"
            exit 1
        fi
    elif [[ "$PLATFORM" == "macos" ]]; then
        # Check if Homebrew is installed
        if ! command -v brew &> /dev/null; then
            print_status "Installing Homebrew..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        
        brew install \
            cmake \
            git \
            sdl2 \
            sdl2_image \
            sdl2_ttf \
            sdl2_mixer \
            pkg-config \
            wget
    elif [[ "$PLATFORM" == "windows" ]]; then
        print_warning "Please install the following manually on Windows:"
        echo "  - Visual Studio 2019/2022 with C++ support"
        echo "  - CMake"
        echo "  - Git"
        echo "  - SDL2 development libraries"
        echo "  - vcpkg (for package management)"
    fi
}

# Setup Android development environment
setup_android() {
    print_status "Setting up Android development environment..."
    
    # Check if Android SDK is installed
    if [[ -z "$ANDROID_HOME" ]]; then
        print_warning "ANDROID_HOME not set. Please install Android Studio and set ANDROID_HOME"
        print_status "Download Android Studio from: https://developer.android.com/studio"
        return 1
    fi
    
    # Check if Android NDK is installed
    if [[ ! -d "$ANDROID_HOME/ndk" ]]; then
        print_status "Installing Android NDK..."
        cd "$ANDROID_HOME"
        wget https://dl.google.com/android/repository/android-ndk-r25c-linux.zip
        unzip android-ndk-r25c-linux.zip
        mv android-ndk-r25c ndk
        rm android-ndk-r25c-linux.zip
    fi
    
    print_success "Android development environment ready"
}

# Setup iOS development environment
setup_ios() {
    print_status "Setting up iOS development environment..."
    
    if [[ "$PLATFORM" != "macos" ]]; then
        print_warning "iOS development requires macOS"
        return 1
    fi
    
    # Check if Xcode is installed
    if ! command -v xcodebuild &> /dev/null; then
        print_error "Xcode not found. Please install Xcode from the App Store"
        return 1
    fi
    
    # Check if Xcode command line tools are installed
    if ! command -v clang &> /dev/null; then
        print_status "Installing Xcode command line tools..."
        xcode-select --install
    fi
    
    print_success "iOS development environment ready"
}

# Download and setup SDL2
setup_sdl2() {
    print_status "Setting up SDL2..."
    
    # Create external directory
    mkdir -p external
    cd external
    
    # Download SDL2 source
    if [[ ! -d "sdl2" ]]; then
        print_status "Downloading SDL2 source..."
        git clone https://github.com/libsdl-org/SDL.git sdl2
        cd sdl2
        git checkout release-2.28.5
        cd ..
    fi
    
    # Build SDL2 for desktop
    if [[ "$PLATFORM" != "windows" ]]; then
        print_status "Building SDL2 for desktop..."
        cd sdl2
        
        mkdir -p build
        cd build
        
        cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DSDL_STATIC=OFF \
            -DSDL_SHARED=ON \
            -DSDL_TEST=OFF
        
        make -j$(nproc)
        sudo make install
        
        cd ../..
    fi
    
    # Build SDL2 for Android
    if [[ -n "$ANDROID_HOME" ]]; then
        print_status "Building SDL2 for Android..."
        cd sdl2
        
        # Create Android project structure
        mkdir -p android-project
        cd android-project
        
        # Copy Android project files
        cp -r ../../../android/* .
        
        # Build for different ABIs
        for ABI in armeabi-v7a arm64-v8a x86 x86_64; do
            print_status "Building for $ABI..."
            
            mkdir -p build/$ABI
            cd build/$ABI
            
            cmake ../../.. \
                -DCMAKE_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/build/cmake/android.toolchain.cmake" \
                -DANDROID_ABI=$ABI \
                -DANDROID_PLATFORM=android-21 \
                -DSDL_STATIC=ON \
                -DSDL_SHARED=OFF \
                -DSDL_TEST=OFF
            
            make -j$(nproc)
            
            cd ../..
        done
        
        cd ../..
    fi
    
    # Build SDL2 for iOS
    if [[ "$PLATFORM" == "macos" ]]; then
        print_status "Building SDL2 for iOS..."
        cd sdl2
        
        # Create iOS project structure
        mkdir -p ios-project
        cd ios-project
        
        # Copy iOS project files
        cp -r ../../../ios/* .
        
        # Build for iOS
        cmake .. \
            -DCMAKE_TOOLCHAIN_FILE=ios.toolchain.cmake \
            -DPLATFORM=OS64 \
            -DSDL_STATIC=ON \
            -DSDL_SHARED=OFF \
            -DSDL_TEST=OFF
        
        make -j$(nproc)
        
        cd ../..
    fi
    
    cd ..
    print_success "SDL2 setup complete"
}

# Create project structure
create_project_structure() {
    print_status "Creating project structure..."
    
    # Create directories
    mkdir -p src/{core,renderer,input,ui,platform}
    mkdir -p assets/{textures,models,sounds,fonts}
    mkdir -p android/{app/src/main/{java/com/mumobile/client,res,assets}}
    mkdir -p ios/MU_Mobile
    mkdir -p build
    mkdir -p tests
    mkdir -p docs
    
    # Create basic files
    touch CMakeLists.txt
    touch README.md
    touch .gitignore
    touch build.sh
    touch run.sh
    
    print_success "Project structure created"
}

# Create .gitignore
create_gitignore() {
    print_status "Creating .gitignore..."
    
    cat > .gitignore << 'EOF'
# Build directories
build/
bin/
lib/
obj/

# IDE files
.vscode/
.idea/
*.swp
*.swo
*~

# OS files
.DS_Store
Thumbs.db

# CMake files
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile

# Android
android/app/build/
android/.gradle/
android/local.properties
*.apk
*.aab

# iOS
ios/build/
ios/DerivedData/
*.ipa
*.app

# Dependencies
external/sdl2/

# Logs
*.log

# Temporary files
*.tmp
*.temp
EOF

    print_success ".gitignore created"
}

# Create build script
create_build_script() {
    print_status "Creating build script..."
    
    cat > build.sh << 'EOF'
#!/bin/bash

# MU Mobile Build Script

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse arguments
PLATFORM=""
CONFIG="Release"
CLEAN=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --config)
            CONFIG="$2"
            shift 2
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Validate platform
if [[ -z "$PLATFORM" ]]; then
    print_error "Please specify platform: --platform [desktop|android|ios]"
    exit 1
fi

# Clean build directory
if [[ "$CLEAN" == "true" ]]; then
    print_status "Cleaning build directory..."
    rm -rf build/$PLATFORM
fi

# Create build directory
mkdir -p build/$PLATFORM
cd build/$PLATFORM

# Configure and build
case $PLATFORM in
    desktop)
        print_status "Building for desktop..."
        cmake ../.. \
            -DCMAKE_BUILD_TYPE=$CONFIG \
            -DPLATFORM_DESKTOP=ON
        make -j$(nproc)
        ;;
    android)
        if [[ -z "$ANDROID_HOME" ]]; then
            print_error "ANDROID_HOME not set"
            exit 1
        fi
        
        print_status "Building for Android..."
        cmake ../.. \
            -DCMAKE_BUILD_TYPE=$CONFIG \
            -DPLATFORM_ANDROID=ON \
            -DCMAKE_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI=arm64-v8a \
            -DANDROID_PLATFORM=android-21
        make -j$(nproc)
        ;;
    ios)
        if [[ "$OSTYPE" != "darwin"* ]]; then
            print_error "iOS build requires macOS"
            exit 1
        fi
        
        print_status "Building for iOS..."
        cmake ../.. \
            -DCMAKE_BUILD_TYPE=$CONFIG \
            -DPLATFORM_IOS=ON \
            -DCMAKE_TOOLCHAIN_FILE=../../ios.toolchain.cmake \
            -DPLATFORM=OS64
        make -j$(nproc)
        ;;
    *)
        print_error "Unsupported platform: $PLATFORM"
        exit 1
        ;;
esac

print_success "Build completed successfully!"
EOF

    chmod +x build.sh
    print_success "Build script created"
}

# Create run script
create_run_script() {
    print_status "Creating run script..."
    
    cat > run.sh << 'EOF'
#!/bin/bash

# MU Mobile Run Script

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse arguments
PLATFORM="desktop"

while [[ $# -gt 0 ]]; do
    case $1 in
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Run based on platform
case $PLATFORM in
    desktop)
        print_status "Running desktop version..."
        ./build/desktop/MU_Mobile_SDL2
        ;;
    android)
        print_status "Installing and running Android version..."
        adb install -r android/app/build/outputs/apk/debug/app-debug.apk
        adb shell am start -n com.mumobile.client/.MainActivity
        ;;
    ios)
        print_error "iOS simulator run not implemented yet"
        exit 1
        ;;
    *)
        print_error "Unsupported platform: $PLATFORM"
        exit 1
        ;;
esac

print_success "Application started!"
EOF

    chmod +x run.sh
    print_success "Run script created"
}

# Create README
create_readme() {
    print_status "Creating README..."
    
    cat > README.md << 'EOF'
# MU Mobile SDL2

MU Online Client ported to mobile platforms using SDL2.

## Features

- Cross-platform support (Android, iOS, Desktop)
- Native C++ performance
- Touch input system
- Virtual joystick
- Mobile-optimized UI
- Network multiplayer support

## Requirements

### Desktop Development
- CMake 3.16+
- C++17 compiler
- SDL2 development libraries

### Android Development
- Android Studio
- Android NDK
- Android SDK (API 21+)

### iOS Development
- macOS
- Xcode
- iOS SDK (12.0+)

## Setup

1. Run the setup script:
```bash
chmod +x setup_development_environment.sh
./setup_development_environment.sh
```

2. Build the project:
```bash
# Desktop
./build.sh --platform desktop

# Android
./build.sh --platform android

# iOS
./build.sh --platform ios
```

3. Run the application:
```bash
# Desktop
./run.sh --platform desktop

# Android
./run.sh --platform android
```

## Project Structure

```
MU_Mobile_SDL2/
├── src/
│   ├── core/           # Original C++ game logic
│   ├── renderer/       # SDL2 rendering wrapper
│   ├── input/          # Touch input system
│   ├── ui/             # Mobile UI system
│   └── platform/       # Platform-specific code
├── assets/             # Game assets
├── android/            # Android project files
├── ios/                # iOS project files
├── build/              # Build output
├── tests/              # Unit tests
└── docs/               # Documentation
```

## Development

### Adding New Features

1. Core game logic goes in `src/core/`
2. Platform-specific code goes in `src/platform/`
3. UI components go in `src/ui/`
4. Input handling goes in `src/input/`

### Testing

Run unit tests:
```bash
cd build/desktop
make test
```

### Debugging

For Android debugging:
```bash
adb logcat | grep MU_Mobile
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## Support

For support and questions, please open an issue on GitHub.
EOF

    print_success "README created"
}

# Main setup function
main() {
    print_status "Starting MU Mobile SDL2 development environment setup..."
    
    # Check platform
    check_platform
    
    # Install system dependencies
    install_system_deps
    
    # Setup mobile development environments
    setup_android
    setup_ios
    
    # Setup SDL2
    setup_sdl2
    
    # Create project structure
    create_project_structure
    
    # Create configuration files
    create_gitignore
    create_build_script
    create_run_script
    create_readme
    
    print_success "Development environment setup complete!"
    print_status ""
    print_status "Next steps:"
    print_status "1. Copy your existing C++ files to src/core/"
    print_status "2. Implement SDL2 rendering wrapper in src/renderer/"
    print_status "3. Add touch input system in src/input/"
    print_status "4. Create mobile UI in src/ui/"
    print_status "5. Test with: ./build.sh --platform desktop"
    print_status ""
    print_status "Happy coding! 🎮📱"
}

# Run main function
main "$@"