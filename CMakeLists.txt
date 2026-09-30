cmake_minimum_required(VERSION 3.21)
project(EndlessDisaster VERSION 0.5.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

qt_add_executable(EndlessDisaster
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/GameWidget.h
    src/GameWidget.cpp
    src/Session.h
    src/Session.cpp
    src/TileMap.h
    src/TileMap.cpp
    src/MazeRuin.h
    src/MazeRuin.cpp
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

if(UNIX AND NOT APPLE AND NOT ANDROID)
    set_target_properties(EndlessDisaster PROPERTIES OUTPUT_NAME endless-disaster)
endif()

target_include_directories(EndlessDisaster PRIVATE src)
target_compile_definitions(EndlessDisaster PRIVATE ED_VERSION="${PROJECT_VERSION}")
target_link_libraries(EndlessDisaster PRIVATE Qt6::Widgets Qt6::Multimedia)

if(WIN32)
    # 可执行文件图标：windres 把 .rc 编进 exe，资源管理器 / 任务栏与文件属性都用它。
    # .rc 由模板生成，括号里填 ico 的绝对路径，省得 windres 相对路径找错地方。
    enable_language(RC)
    set(ED_WIN_ICON ${CMAKE_SOURCE_DIR}/assets/ui/EndlessDisaster.ico)
    if(EXISTS ${ED_WIN_ICON})
        set(ED_WIN_ICON_RC ${CMAKE_BINARY_DIR}/EndlessDisaster.rc)
        configure_file(packaging/windows/EndlessDisaster.rc.in ${ED_WIN_ICON_RC} @ONLY)
        target_sources(EndlessDisaster PRIVATE ${ED_WIN_ICON_RC})
        # 换了 ico 也要重编 .rc：CMake 默认只跟踪 .rc 自己
        set_property(SOURCE ${ED_WIN_ICON_RC} PROPERTY OBJECT_DEPENDS ${ED_WIN_ICON})
    else()
        message(WARNING "Windows 图标不存在，跳过：${ED_WIN_ICON}")
    endif()
endif()

if(MSVC)
    target_compile_options(EndlessDisaster PRIVATE /utf-8)
else()
    target_compile_options(EndlessDisaster PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

if(WIN32)
    # 必须与链接的 Qt 同一套：官方 Qt 的 windeployqt 会连同精简版 FFmpeg DLL 一起部署
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt
        PATHS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}" NO_DEFAULT_PATH)
endif()

if(NOT ANDROID)
add_custom_command(TARGET EndlessDisaster POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EndlessDisaster>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/hero_mage $<TARGET_FILE_DIR:EndlessDisaster>/assets/hero_mage
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/hero_sword $<TARGET_FILE_DIR:EndlessDisaster>/assets/hero_sword
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/hero_warrior $<TARGET_FILE_DIR:EndlessDisaster>/assets/hero_warrior
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/hero_robot $<TARGET_FILE_DIR:EndlessDisaster>/assets/hero_robot
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/killbot $<TARGET_FILE_DIR:EndlessDisaster>/assets/killbot
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/drone $<TARGET_FILE_DIR:EndlessDisaster>/assets/drone
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
endif()

if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET EndlessDisaster POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --release --no-translations --no-opengl-sw
                \"$<TARGET_FILE:EndlessDisaster>\"
        COMMENT "Deploying Qt runtime (incl. Multimedia)"
    )
endif()

if(ANDROID)
    # 资源编进 qrc，程序里按 ":/assets/..." 读取
    file(GLOB_RECURSE ED_ANDROID_DATA RELATIVE ${CMAKE_SOURCE_DIR} CONFIGURE_DEPENDS
        assets/hero_mage/* assets/hero_sword/* assets/hero_warrior/* assets/hero_robot/*
        assets/killbot/* assets/drone/* assets/slime/* assets/skeleton/* assets/mushroom/* assets/flyer/*
        assets/tiles/* assets/sfx/* assets/character-img/*.png BGM/*)
    list(APPEND ED_ANDROID_DATA assets/ui/story_bg.png assets/ui/游戏图标.jpg)
    qt_add_resources(EndlessDisaster gamedata PREFIX / BIG_RESOURCES FILES ${ED_ANDROID_DATA})

    # 覆盖安装要求 versionCode 不减小：由版本号推出（0.3.1 -> 301），本地与 CI 同版本得到相同的值
    set(ED_ANDROID_VERSION_CODE "" CACHE STRING "Override Android versionCode (empty: derived from project version)")
    if(ED_ANDROID_VERSION_CODE)
        set(ED_VERSION_CODE ${ED_ANDROID_VERSION_CODE})
    else()
        math(EXPR ED_VERSION_CODE
            "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")
    endif()
    message(STATUS "Android versionName ${PROJECT_VERSION}, versionCode ${ED_VERSION_CODE}")

    set_target_properties(EndlessDisaster PROPERTIES
        QT_ANDROID_PACKAGE_NAME io.github.shadowye.endlessdisaster
        QT_ANDROID_VERSION_NAME ${PROJECT_VERSION}
        QT_ANDROID_VERSION_CODE ${ED_VERSION_CODE}
        QT_ANDROID_TARGET_SDK_VERSION 34)

    # 设置了 QT_ANDROID_KEYSTORE_PATH 等环境变量时自动用正式密钥签名，
    # 否则 APK 带调试签名，无法覆盖安装正式包（见 docs/android-signing.md）
    if(NOT DEFINED QT_ANDROID_SIGN_APK AND DEFINED ENV{QT_ANDROID_KEYSTORE_PATH})
        set(QT_ANDROID_SIGN_APK ON)
    endif()
    if(QT_ANDROID_SIGN_APK)
        message(STATUS "Android APK will be signed with $ENV{QT_ANDROID_KEYSTORE_PATH}")
    else()
        message(STATUS "Android APK uses the debug signature (QT_ANDROID_KEYSTORE_PATH not set)")
    endif()

    # 基于当前 Qt 自带的清单模板：锁定横屏、应用名与图标
    set(ED_ANDROID_DIR ${CMAKE_BINARY_DIR}/android-package)
    set(ED_MANIFEST_TEMPLATE ${QT6_INSTALL_PREFIX}/src/android/templates/AndroidManifest.xml)
    if(EXISTS ${ED_MANIFEST_TEMPLATE})
        file(READ ${ED_MANIFEST_TEMPLATE} ED_MANIFEST)
        string(REPLACE "-- %%INSERT_APP_NAME%% --" "无尽之灾" ED_MANIFEST "${ED_MANIFEST}")
        if(ED_MANIFEST MATCHES "android:screenOrientation=")
            string(REGEX REPLACE "android:screenOrientation=\"[^\"]*\"" "android:screenOrientation=\"sensorLandscape\""
                ED_MANIFEST "${ED_MANIFEST}")
        else()
            string(REPLACE "<activity" "<activity android:screenOrientation=\"sensorLandscape\"" ED_MANIFEST "${ED_MANIFEST}")
        endif()
        if(ED_MANIFEST MATCHES "android:icon=")
            string(REGEX REPLACE "android:icon=\"[^\"]*\"" "android:icon=\"@drawable/icon\"" ED_MANIFEST "${ED_MANIFEST}")
        else()
            string(REPLACE "<application" "<application android:icon=\"@drawable/icon\"" ED_MANIFEST "${ED_MANIFEST}")
        endif()
        file(WRITE ${ED_ANDROID_DIR}/AndroidManifest.xml "${ED_MANIFEST}")
        configure_file(packaging/linux/endless-disaster.png ${ED_ANDROID_DIR}/res/drawable/icon.png COPYONLY)
        set_property(TARGET EndlessDisaster PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR ${ED_ANDROID_DIR})
    else()
        message(WARNING "Qt Android manifest template not found at ${ED_MANIFEST_TEMPLATE}; using Qt defaults")
    endif()
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
    include(GNUInstallDirs)
    set(ED_DATA_DIR ${CMAKE_INSTALL_DATADIR}/endless-disaster)

    install(TARGETS EndlessDisaster RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY
        assets/hero_mage assets/hero_sword assets/hero_warrior assets/hero_robot
        assets/killbot assets/drone assets/slime assets/skeleton assets/mushroom assets/flyer
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
