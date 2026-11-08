cmake_minimum_required(VERSION 3.22)
project(SoapySdkConsumer LANGUAGES C)
# Upstream package versions include a build-info suffix; the executable checks
# the library's release and ABI as well as resolving the exported CMake target.
find_package(SoapySDR 0.8.1 CONFIG REQUIRED)
add_executable(soapy_sdk_consumer main.c)
target_link_libraries(soapy_sdk_consumer PRIVATE SoapySDR)
enable_testing()
add_test(NAME soapy-sdk-consumer COMMAND soapy_sdk_consumer)
