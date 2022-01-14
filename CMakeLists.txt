cmake_minimum_required (VERSION 2.6)
project (ffmpeg_test)

#include_directories(${CMAKE_SOURCE_DIR}/ffmpeg-4.4.1-full_build-shared/include)

#link_directories(${CMAKE_SOURCE_DIR}/ffmpeg-4.4.1-full_build-shared/lib)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-4.4.1-full_build-shared/include)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-4.4.1-full_build-shared/lib)

add_executable(ffmpeg_test main.cpp h264toyuv.cpp extractH264FromMP4.cpp extracAACFromMP4.cpp aactopcm.cpp enumAllDev.cpp writeCameaVideoStream.cpp pushRTMP.cpp)




target_link_libraries(ffmpeg_test avcodec.lib avdevice.lib avfilter.lib avformat.lib avutil.lib postproc.lib swresample.lib swscale.lib)