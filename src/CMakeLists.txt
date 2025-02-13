 include(CTest)

qt_add_executable(QTNdiRecv
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        NDIDeleters.h
)

add_executable(DeletersTest
        NDIDeleters.h
        TestNDIDeleters.cpp
)

target_include_directories(QTNdiRecv PUBLIC "D:/Program Files/NDI/NDI 6 SDK/Include")
target_link_libraries(QTNdiRecv PRIVATE Qt6::Widgets Qt6::Concurrent Qt6::Multimedia)

target_link_libraries(QTNdiRecv PRIVATE "D:/Program Files/NDI/NDI 6 SDK/Lib/x64/Processing.NDI.Lib.x64.lib")

# enable testing functionality
enable_testing()
add_test(
  NAME deletersTest
  COMMAND $<TARGET_FILE:DeletersTest>
  )


set_target_properties(QTNdiRecv PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)