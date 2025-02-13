# NDIReceive

A simple, QT6 based application for receiving and playing NDI video and audio.

# Building
## Prerequisites

QT6 dev files installed (can be done via the QT site; the GPL version is absolutely free to download and use).
NDISDK installed. This can be reached via https://ndi.video

## Build System and local edits for you to make
This comes with a CMake file that should enable you to create a project of the format you desire. Testing currently only on Win10, with target VS2022. There are a couple of lines in the CMake file that need editing to match your local system.

The NDISDK installation directory has some include files that are needed. The location is hard-coded into a CMakeLists.txt file:

    target_include_directories(QTNdiRecv PUBLIC "D:/Program Files/NDI/NDI 6 SDK/Include")

This must be edited to match your system.
Likewise the location of the library file (under Linux, the name of the file would also change):

    target_link_libraries(QTNdiRecv PRIVATE "D:/Program Files/NDI/NDI 6 SDK/Lib/x64/Processing.NDI.Lib.x64.lib")
  
## Building

Running CMake fresh for me looks like this (I run from a build directory that I created, because I like to have my build files and all that mess outside the source tree):

> PS D:\repos\NDIReceive\build> cmake ..
-- Building for: Visual Studio 17 2022
-- Selecting Windows SDK version 10.0.22621.0 to target Windows 10.0.19045.
-- The CXX compiler identification is MSVC 19.42.34436.0
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.42.34433/bin/Hostx64/x64/cl.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Failed
-- Looking for pthread_create in pthreads
-- Looking for pthread_create in pthreads - not found
-- Looking for pthread_create in pthread
-- Looking for pthread_create in pthread - not found
-- Found Threads: TRUE
-- Performing Test HAVE_STDATOMIC
-- Performing Test HAVE_STDATOMIC - Success
-- Found WrapAtomic: TRUE
-- Could NOT find WrapVulkanHeaders (missing: Vulkan_INCLUDE_DIR)
-- Configuring done (20.8s)
-- Generating done (1.2s)
-- Build files have been written to: D:/repos/NDIReceive/build

That creates a standard VS2022 solution file that I can open and then build.

## Running
The running executable needs to be able to find the NDISDK library (on windows, Processing.NDI.Lib.x64.dll). This could be in your environment path, or you can copy it to the build directory (next to the generated executable).

## Usage
Upon running, the default audio device is detected. This will be used for audio output. It is identified in the Log window.

![Image of NDRReceiver application](https://github.com/HowlsMovingCast/NDIReceive/blob/main/readmeImages/overview.jpg?raw=true)


Find NDI streams by clicking "Scan for streams." 

Select one of the found streams and either "Begin playback" in the "Video playback" tab, or "Capture Video Frame" in the "Video Frame Capture" tab. The selected stream will be interrogated for the relevant data which will then be displayed or streamed.
