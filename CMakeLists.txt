cmake_minimum_required(VERSION 3.16)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

# Also migrate existing build directories away from their cached install path.
set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed"
    CACHE PATH "Project-local vcpkg dependency installation directory" FORCE)

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
find_package(WCDB CONFIG REQUIRED)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(PROJECT_SOURCES
        main.cpp
        src/ui/login.cpp
        src/ui/login.h
        src/ui/login.ui
)

file(GLOB_RECURSE PROTO_FILES "src/proto/*.*")

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
        src/application/applicationController.h src/application/applicationController.cpp
        src/application/clientSession.h src/application/clientSession.cpp
        src/network/webSocketSession.h src/network/webSocketSession.cpp
        src/controller/authController.h
        src/controller/authController.cpp
        src/model/loginResult.h
        src/model/authModel.h
        src/dao/remote/remoteAuthDao.h src/dao/remote/remoteAuthDao.cpp
        src/dao/remote/remoteRegDao.h src/dao/remote/remoteRegDao.cpp
        src/model/regResult.h
        src/model/regModel.h
        src/controller/regController.h src/controller/regController.cpp
        src/service/authService.h src/service/authService.cpp
        src/service/regService.h src/service/regService.cpp
        src/model/accountHistory.h
        src/dao/interfaces/accountHistoryDao.h
        src/dao/local/wcdb/wcdbAccountHistoryDao.h
        src/dao/local/wcdb/wcdbAccountHistoryDao.cpp
        src/storage/wcdb/wcdbHeaders.h
        src/storage/wcdb/localDatabase.h src/storage/wcdb/localDatabase.cpp
        src/storage/wcdb/accountHistoryRow.h src/storage/wcdb/accountHistoryRow.cpp
        src/service/accountHistoryService.h src/service/accountHistoryService.cpp
        src/ui/mainIM.h src/ui/mainIM.cpp src/ui/mainIM.ui
        src/model/chatMessage.h
        src/service/chatService.h src/service/chatService.cpp
        src/dao/local/wcdb/wcdbChatDao.h src/dao/local/wcdb/wcdbChatDao.cpp
        src/storage/wcdb/chatMessageRow.h src/storage/wcdb/chatMessageRow.cpp
        test/testtls.h test/testtls.cpp
        ${PROTO_FILES}



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

target_compile_definitions(IM-client PRIVATE NOMINMAX)

target_link_libraries(IM-client PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt6::Network
    Crypt32
    Qt6::WebSockets
    protobuf::libprotobuf
    gRPC::grpc++
    WCDB::WCDB
)

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

option(IM_CLIENT_BUILD_HISTORY_TESTS "Build local account history tests" OFF)
if(IM_CLIENT_BUILD_HISTORY_TESTS)
    enable_testing()
    add_executable(account-history-test
        test/accountHistoryTest.cpp
        src/storage/wcdb/localDatabase.cpp
        src/storage/wcdb/accountHistoryRow.cpp
        src/dao/local/wcdb/wcdbAccountHistoryDao.cpp
    )
    target_compile_definitions(account-history-test PRIVATE NOMINMAX)
    target_link_libraries(account-history-test PRIVATE Qt6::Core WCDB::WCDB)
    add_test(NAME account-history COMMAND account-history-test)
endif()
