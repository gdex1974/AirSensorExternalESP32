if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/AppConfig.cpp")
    file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/AppConfig.cpp.example"
            "${CMAKE_CURRENT_LIST_DIR}/AppConfig.cpp")
endif()

idf_component_register(
        SRCS
        "AppConfig.cpp"
        "AppMain.cpp"
        "DustMonitorController.cpp"
        "EspNowTransport.cpp"
        "PTHProvider.cpp"
        "SPS30DataProvider.cpp"
        INCLUDE_DIRS
        "."
)
