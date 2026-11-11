add_subdirectory(source-example)
add_subdirectory(demod-example)
add_subdirectory(devices)
add_subdirectory(device-example)

if(SDR_INSTALL_LINUX_PORTABLE)
  install(TARGETS sdr_source_example sdr_demod_example LIBRARY DESTINATION examples)
  install(FILES
    "$<TARGET_FILE_DIR:sdr_source_example>/source-example.json"
    "$<TARGET_FILE_DIR:sdr_demod_example>/demod-example.json"
    DESTINATION examples)
  install(TARGETS sdr_device_example LIBRARY DESTINATION examples/device)
  install(FILES "$<TARGET_FILE_DIR:sdr_device_example>/example-device.json"
    DESTINATION examples/device)
endif()
