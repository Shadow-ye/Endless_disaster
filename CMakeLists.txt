cmake_minimum_required(VERSION 3.21)
project(EndlessDisaster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

add_executable(EndlessDisaster
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/GameWidget.h
    src/GameWidget.cpp
    src/Session.h
    src/Session.cpp
    src/TileMap.h
    src/TileMap.cpp
    src/PathField.h
    src/PathField.cpp
    src/Sprites.h
    src/Sprites.cpp
    src/Storage.h
    src/Storage.cpp
    src/Audio.h
    src/Audio.cpp
)

target_include_directories(EndlessDisaster PRIVATE src)
target_link_libraries(EndlessDisaster PRIVATE Qt6::Widgets Qt6::Multimedia)

if(MSVC)
    target_compile_options(EndlessDisaster PRIVATE /utf-8)
else()
    target_compile_options(EndlessDisaster PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt
    HINTS C:/msys64/ucrt64/bin)

add_custom_command(TARGET EndlessDisaster POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EndlessDisaster>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/hero_mage $<TARGET_FILE_DIR:EndlessDisaster>/assets/hero_mage
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/hero_sword $<TARGET_FILE_DIR:EndlessDisaster>/assets/hero_sword
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/hero_warrior $<TARGET_FILE_DIR:EndlessDisaster>/assets/hero_warrior
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/slime $<TARGET_FILE_DIR:EndlessDisaster>/assets/slime
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/skeleton $<TARGET_FILE_DIR:EndlessDisaster>/assets/skeleton
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/mushroom $<TARGET_FILE_DIR:EndlessDisaster>/assets/mushroom
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/flyer $<TARGET_FILE_DIR:EndlessDisaster>/assets/flyer
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/tiles $<TARGET_FILE_DIR:EndlessDisaster>/assets/tiles
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/sfx $<TARGET_FILE_DIR:EndlessDisaster>/assets/sfx
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EndlessDisaster>/assets/ui
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/ui/story_bg.png $<TARGET_FILE_DIR:EndlessDisaster>/assets/ui/story_bg.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/ui/story_bg.png $<TARGET_FILE_DIR:EndlessDisaster>/故事背景.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/ui/游戏图标.jpg $<TARGET_FILE_DIR:EndlessDisaster>/assets/ui/游戏图标.jpg
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/ui/游戏图标.jpg $<TARGET_FILE_DIR:EndlessDisaster>/游戏图标.jpg
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/character-img $<TARGET_FILE_DIR:EndlessDisaster>/character-img
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/BGM $<TARGET_FILE_DIR:EndlessDisaster>/BGM
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/ART_CREDIT.txt $<TARGET_FILE_DIR:EndlessDisaster>/assets/ART_CREDIT.txt
)

if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET EndlessDisaster POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --release --no-translations --no-compiler-runtime
                \"$<TARGET_FILE:EndlessDisaster>\"
        COMMENT "Deploying Qt runtime (incl. Multimedia)"
    )
endif()
