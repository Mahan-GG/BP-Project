cmake_minimum_required(VERSION 3.26)
project(Project)

set(CMAKE_CXX_STANDARD 17)

# اضافه کردن هر دو فایل موجود در پوشه src
set(SOURCES
        src/main.cpp
        src/FileMenu.cpp
        src/FileMenu.cpp
        src/FileMenu.h  # <--- این فایل حتماً باید اینجا باشد
)

add_executable(Project ${SOURCES})

# لینک کردن کتابخانه‌ها
target_link_libraries(${PROJECT_NAME}
        mingw32
        SDL2main
        SDL2
        SDL2_image
        SDL2_ttf
        SDL2_mixer
        "-Wl,--allow-multiple-definition"
)
