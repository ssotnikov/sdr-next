cmake_minimum_required(VERSION 3.22)
project(sdr_demod_example LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(SDR_SDK_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/../../sdk/include" CACHE PATH "SDR SDK headers")
add_library(sdr_demod_example SHARED demod.cpp)
target_include_directories(sdr_demod_example PRIVATE "${SDR_SDK_INCLUDE}")
set_target_properties(sdr_demod_example PROPERTIES PREFIX "")
file(GENERATE OUTPUT "$<TARGET_FILE_DIR:sdr_demod_example>/demod-example.json" CONTENT
"{\"id\":\"org.sdr.example.demod\",\"name\":\"Example AM demodulator\",\"version\":\"0.1.0\",\"api_major\":1,\"api_minor\":0,\"kind\":\"demodulator\",\"platform\":\"${CMAKE_SYSTEM_NAME}\",\"arch\":\"x86_64\",\"license\":\"MIT\",\"library\":\"$<TARGET_FILE_NAME:sdr_demod_example>\"}")
