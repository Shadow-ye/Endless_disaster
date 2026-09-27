cmake_minimum_required(VERSION 3.21)
project(EndlessDisaster VERSION 0.2.0 LANGUAGES CXX)

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
    src/Platform.h
    src/Platform.cpp
)

if(UNIX AND NOT APPLE)
    set_target_properties(EndlessDisaster PROPERTIES OUTPUT_NAME endless-disaster)
endif()

target_include_directories(EndlessDisaster PRIVATE src)
target_link_libraries(EndlessDisaster PRIVATE Qt6::Widgets Qt6::Multimedia)

if(MSVC)
    target_compile_options(EndlessDisaster PRIVATE /utf-8)
else()
    target_compile_options(EndlessDisaster PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt
        HINTS C:/msys64/ucrt64/bin)
endif()

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

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    set(ED_DATA_DIR ${CMAKE_INSTALL_DATADIR}/endless-disaster)

    install(TARGETS EndlessDisaster RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY
        assets/hero_mage assets/hero_sword assets/hero_warrior
        assets/slime assets/skeleton assets/mushroom assets/flyer
        assets/tiles assets/sfx
        DESTINATION ${ED_DATA_DIR}/assets)
    install(FILES assets/ui/story_bg.png assets/ui/游戏图标.jpg
        DESTINATION ${ED_DATA_DIR}/assets/ui)
    install(FILES assets/ART_CREDIT.txt DESTINATION ${ED_DATA_DIR}/assets)
    install(DIRECTORY assets/character-img DESTINATION ${ED_DATA_DIR})
    install(DIRECTORY BGM DESTINATION ${ED_DATA_DIR})

    install(FILES packaging/linux/endless-disaster.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/linux/endless-disaster.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)

    set(CPACK_GENERATOR DEB)
    set(CPACK_PACKAGE_NAME endless-disaster)
    set(CPACK_PACKAGE_VENDOR "Shadow-ye")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "无尽之灾 Endless Disaster - 2D roguelike action survival game")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Shadow-ye/Endless_disaster")
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Shadow-ye <cym1569353578@qq.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION games)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins, gstreamer1.0-plugins-good, gstreamer1.0-libav")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fonts-noto-cjk")
    include(CPack)
endif()
