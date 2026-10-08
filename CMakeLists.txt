cmake_minimum_required(VERSION 3.16)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

project(IM-client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)
find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(PROJECT_SOURCES
        main.cpp
        src/ui/login.cpp
        src/ui/login.h
        src/ui/login.ui
)

file(GLOB_RECURSE PROTOFILES "src/proto/*.*")

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(IM-client
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        src/ui/avatarLabel.h
        src/ui/avatarLabel.cpp
        res.qrc
        src/ui/accountDropdown.h
        src/ui/accountDropdown.cpp
        src/ui/reg.h src/ui/reg.cpp src/ui/reg.ui
        src/ui/loginFailed.h src/ui/loginFailed.cpp src/ui/loginFailed.ui
        src/ui/regFailed.h src/ui/regFailed.cpp src/ui/regFailed.ui
        src/controller/authController.h
        src/controller/authController.cpp
        src/model/loginResult.h
        src/model/authModel.h src/model/authModel.cpp
        src/dao/remoteAuthDao.h src/dao/remoteAuthDao.cpp
        src/dao/remoteregdao.h src/dao/remoteregdao.cpp
        src/model/regResult.h
        test/testtls.h test/testtls.cpp
        ${PROTOFILES}




    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET IM-client APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(IM-client SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(IM-client
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(IM-client PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt6::Network Crypt32 Qt6::WebSockets protobuf::libprotobuf
                            gRPC::grpc++)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.IM-client)
endif()
set_target_properties(IM-client PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS IM-client
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(IM-client)
endif()
