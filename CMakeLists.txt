cmake_minimum_required(VERSION 3.16)
project(YiHuanIM VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT Qt6_DIR AND NOT QT_PREFIX)
  # 常见安装位置，可按需覆盖： -DQT_PREFIX=C:/Qt/6.11.2/mingw_64
  set(QT_PREFIX "C:/Qt/6.11.2/mingw_64" CACHE PATH "Qt6 安装前缀")
endif()
list(APPEND CMAKE_PREFIX_PATH "${QT_PREFIX}")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# ---- 共享核心源码（服务端 + 客户端无界面部分共用）----
set(CORE_SOURCES
  src/net/wsutil.cpp
  src/net/wsserver.cpp
  src/core/config.cpp
  src/core/store.cpp
  src/core/ai.cpp
  src/core/bots.cpp
  src/core/discovery.cpp
  src/core/server.cpp
)

# ---- 完整桌面应用（GUI 文件就绪后自动纳入）----
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
add_executable(YiHuanIM
  src/main.cpp
  src/client/connection.cpp
  src/client/login.cpp
  src/client/mainwindow.cpp
  src/client/sidebar.cpp
  src/client/chatwidget.cpp
  src/client/messagebubble.cpp
  src/client/settingsdialog.cpp
  ${CORE_SOURCES}
)
target_include_directories(YiHuanIM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(YiHuanIM Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
# 隐藏启动时的黑色控制台窗口（GUI 子系统）
if(WIN32)
  set_target_properties(YiHuanIM PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Windows：嵌入 manifest，要求 requireAdministrator（首次双击会弹 UAC），
# 启动后自动写入 UDP 4799 入站放行规则，局域网发现即开即用
if(WIN32)
  set(YIHUAN_RC "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
  set(YIHUAN_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")
  if(EXISTS "${YIHUAN_RC}" AND EXISTS "${YIHUAN_MANIFEST}")
    find_program(WINDRES windres)
    if(WINDRES)
      set(YIHUAN_MANIFEST_OBJ "${CMAKE_CURRENT_BINARY_DIR}/yihuan_im_manifest.o")
      add_custom_command(
        OUTPUT "${YIHUAN_MANIFEST_OBJ}"
        COMMAND "${WINDRES}" -i "${YIHUAN_RC}" -O coff -o "${YIHUAN_MANIFEST_OBJ}"
        DEPENDS "${YIHUAN_RC}" "${YIHUAN_MANIFEST}"
        COMMENT "嵌入 UAC manifest (requireAdministrator)"
      )
      target_sources(YiHuanIM PRIVATE "${YIHUAN_MANIFEST_OBJ}")
    endif()
  endif()
endif()

if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
  find_program(WINDEPLOYQT windeployqt)
  if(WINDEPLOYQT)
    add_custom_command(TARGET YiHuanIM POST_BUILD
      COMMAND "${WINDEPLOYQT}" --no-translations --no-opengl-sw "$<TARGET_FILE:YiHuanIM>"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      COMMENT "收集 Qt 运行时依赖")
  endif()
endif()
endif()

# ---- 无头自测（不链接 Widgets）----
add_executable(yihuan_selftest
  tests/selftest.cpp
  ${CORE_SOURCES}
)
target_include_directories(yihuan_selftest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(yihuan_selftest Qt6::Core Qt6::Network)
