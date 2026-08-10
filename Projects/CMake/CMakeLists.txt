cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(HatchStudio VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(STUDIO_SOURCES_DIR "${CMAKE_SOURCE_DIR}/../../Source/Editor")
set(STUDIO_HATCH_DIR "${CMAKE_SOURCE_DIR}/../../Source/Hatch")
set(STUDIO_INCLUDES_DIR "${CMAKE_SOURCE_DIR}/../../Include")

add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
  ${STUDIO_SOURCES_DIR}
  ${STUDIO_INCLUDES_DIR}
)

set(CMAKE_CXX_FLAGS "-Wno-changes-meaning")

target_sources(${PROJECT_NAME} PUBLIC
  ${STUDIO_SOURCES_DIR}/UI/Platforms/Common.cpp
  ${STUDIO_INCLUDES_DIR}/Hatch/Libraries/miniz.c
  ${STUDIO_SOURCES_DIR}/Libraries/clip.cpp
  ${STUDIO_SOURCES_DIR}/Libraries/image.cpp
  ${STUDIO_SOURCES_DIR}/Libraries/tinyfiledialogs.c
  ${STUDIO_SOURCES_DIR}/Studio/Editors/ResourceEditor.cpp
  ${STUDIO_SOURCES_DIR}/Studio/Editors/SceneEditor.cpp
  ${STUDIO_SOURCES_DIR}/Studio/Subcontrols/TileCollisionEditorPanel.cpp
  ${STUDIO_SOURCES_DIR}/Studio/Subcontrols/TileDrawingWidget.cpp
  ${STUDIO_SOURCES_DIR}/Studio/Subcontrols/TileSelector.cpp
  ${STUDIO_SOURCES_DIR}/Studio/Hashing/MD5.cpp
  ${STUDIO_SOURCES_DIR}/Studio/Hashing/Murmur.cpp
  ${STUDIO_SOURCES_DIR}/Studio/ImageFormats/GIF.cpp
  ${STUDIO_SOURCES_DIR}/Studio/Impl.cpp
  ${STUDIO_SOURCES_DIR}/Studio/IO/FileStream.cpp
  ${STUDIO_SOURCES_DIR}/Studio/IO/MemoryStream.cpp
  ${STUDIO_SOURCES_DIR}/Studio/IO/ResourceStream.cpp
  ${STUDIO_SOURCES_DIR}/Studio/IO/Stream.cpp
  ${STUDIO_SOURCES_DIR}/Studio/Memory.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Button.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ComboBox.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Container.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Control.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/DropDown.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Form.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Label.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ListView.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/MenuBar.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/NumericUpDownBox.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/PropertyGrid.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/RadialKnob.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ScrollBar.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ShowHideTabControl.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/SplitContainer.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/TabControls.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Textbox.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ToolStrip.cpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ToolTip.cpp
  ${STUDIO_SOURCES_DIR}/UI/Filesystem/Paths.cpp
  ${STUDIO_SOURCES_DIR}/UI/Graphics/Font.cpp
  ${STUDIO_SOURCES_DIR}/UI/Graphics/Renderer.cpp
  ${STUDIO_SOURCES_DIR}/UI/System/Application.cpp
  ${STUDIO_SOURCES_DIR}/UI/System/Clipboard.cpp
  ${STUDIO_SOURCES_DIR}/UI/System/Diagnostics.cpp
  ${STUDIO_SOURCES_DIR}/UI/System/Menu.cpp
  ${STUDIO_SOURCES_DIR}/UI/System/SystemDialog.cpp
  ${STUDIO_HATCH_DIR}/Math.cpp
  ${STUDIO_HATCH_DIR}/Resources.cpp
  ${STUDIO_HATCH_DIR}/Strings.cpp
  ${STUDIO_HATCH_DIR}/Services/Dummy/Services.cpp
  ${STUDIO_SOURCES_DIR}/Main.cpp
  ${STUDIO_INCLUDES_DIR}/Hatch/Libraries/ini.h
  ${STUDIO_INCLUDES_DIR}/Hatch/Libraries/miniz.h
  ${STUDIO_INCLUDES_DIR}/Hatch/Libraries/stb_truetype.h
  ${STUDIO_SOURCES_DIR}/Libraries/stb_image.h
  ${STUDIO_SOURCES_DIR}/Libraries/stb_image_write.h
  ${STUDIO_SOURCES_DIR}/Libraries/tinyfiledialogs.h
  ${STUDIO_SOURCES_DIR}/Studio/Editors/ResourceEditor.hpp
  ${STUDIO_SOURCES_DIR}/Studio/Editors/SceneEditor.hpp
  ${STUDIO_SOURCES_DIR}/Studio/Subcontrols/TileCollisionEditorPanel.hpp
  ${STUDIO_SOURCES_DIR}/Studio/Subcontrols/TileDrawingWidget.hpp
  ${STUDIO_SOURCES_DIR}/Studio/Subcontrols/TileSelector.hpp
  ${STUDIO_SOURCES_DIR}/Studio/Impl.hpp
  ${STUDIO_SOURCES_DIR}/UI/Components/Collections.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Button.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ComboBox.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Container.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Control.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Form.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Label.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ListView.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/NumericUpDownBox.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/PropertyGrid.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/RadialKnob.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ScrollBar.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ShowHideTabControl.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/SplitContainer.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/TabControls.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/Textbox.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ToolStrip.hpp
  ${STUDIO_SOURCES_DIR}/UI/Controls/ToolTip.hpp
  ${STUDIO_SOURCES_DIR}/UI/Filesystem/Paths.hpp
  ${STUDIO_SOURCES_DIR}/UI/Graphics/Font.hpp
  ${STUDIO_SOURCES_DIR}/UI/Graphics/Renderer.hpp
  ${STUDIO_SOURCES_DIR}/UI/System/Application.hpp
  ${STUDIO_SOURCES_DIR}/UI/System/Clipboard.hpp
  ${STUDIO_SOURCES_DIR}/UI/System/Diagnostics.hpp
  ${STUDIO_SOURCES_DIR}/UI/System/Menu.hpp
  ${STUDIO_SOURCES_DIR}/UI/System/SystemDialog.hpp
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
  PLATFORM_SETTINGS=<Studio/Settings.h>
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  target_compile_definitions(${PROJECT_NAME} PUBLIC
    WIN32 _WINDOWS -DUSE_NATIVE_MENU
  )

  target_include_directories(${PROJECT_NAME} PUBLIC
    win/include
    win/include/SDL2
  )

  target_sources(${PROJECT_NAME} PUBLIC
    ${STUDIO_SOURCES_DIR}/UI/Platforms/Win32.cpp
    ${STUDIO_SOURCES_DIR}/Libraries/clip_win.cpp)

  target_link_libraries(${PROJECT_NAME}
    ${CMAKE_SOURCE_DIR}/win/lib/${TOOLCHAIN_PREFIX}/libSDL2.dll.a
    ${CMAKE_SOURCE_DIR}/win/lib/${TOOLCHAIN_PREFIX}/libSDL2main.a
    -static -lmingw32 -mwindows -lshlwapi -lwindowscodecs
  )
else()
  target_sources(${PROJECT_NAME} PUBLIC
    ${STUDIO_SOURCES_DIR}/UI/Platforms/Linux.cpp
    ${STUDIO_SOURCES_DIR}/Libraries/clip_none.cpp)

  find_package(SDL2 REQUIRED)
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
endif()

add_definitions(-DSDL_MAIN_HANDLED)
