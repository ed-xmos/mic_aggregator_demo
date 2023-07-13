#**********************
# Gather Sources
#**********************

set(APP_SRC_PATH src)


file(GLOB APP_SOURCES               ${APP_SRC_PATH}/*.c
                                    ${APP_SRC_PATH}/*.xc
                                    ${APP_SRC_PATH}/*.cpp
)


#**********************
# Flags
#**********************
set(APP_COMPILER_FLAGS
    -Os
    -g
    -report
    -fxscope
    # -mcmodel=large
    # -Werror
    -Wall
    -Wno-xcore-fptrgroup
)

set(APP_COMPILE_DEFINITIONS
    DEBUG_PRINT_ENABLE=1
    )

set(APP_LINK_OPTIONS
    -report
    -target=XCORE-AI-EXPLORER
)

set(APP_COMMON_LINK_LIBRARIES
    lib_i2s
    lib_i2c
    lib_mic_array
)

#*************************
# Create Target - Note just single tile as no RTOS
#*************************
set(TARGET_NAME mic_aggregator)
add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL )
target_sources(${TARGET_NAME} PUBLIC ${APP_SOURCES})

target_include_directories(${TARGET_NAME} PUBLIC ${APP_INCLUDES})
target_compile_definitions(${TARGET_NAME} PUBLIC ${APP_COMPILE_DEFINITIONS})
target_compile_options(${TARGET_NAME} PRIVATE ${APP_COMPILER_FLAGS})
target_link_libraries(${TARGET_NAME} PUBLIC ${APP_COMMON_LINK_LIBRARIES})
target_link_options(${TARGET_NAME} PRIVATE ${APP_LINK_OPTIONS})
unset(TARGET_NAME)


# Copy output to a handy location
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.xe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
