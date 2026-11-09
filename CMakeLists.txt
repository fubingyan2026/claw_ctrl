cmake_minimum_required(VERSION 3.24)
project(claw_ctrl LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FW ${CMAKE_CURRENT_SOURCE_DIR}/Drivers)
set(CMSIS ${FW}/CMSIS/GD/GD32F50x)
set(SPL ${FW}/GD32F50x_standard_peripheral)
add_library(gd32f50x STATIC
    ${CMSIS}/Source/system_gd32f50x.c
    ${SPL}/Source/gd32f50x_rcu.c
    ${SPL}/Source/gd32f50x_gpio.c
    ${SPL}/Source/gd32f50x_misc.c
    ${SPL}/Source/gd32f50x_can.c
    ${SPL}/Source/gd32f50x_timer.c
    ${SPL}/Source/gd32f50x_dma.c
    ${SPL}/Source/gd32f50x_usart.c)
target_include_directories(gd32f50x PUBLIC Core/Inc ${FW}/CMSIS ${CMSIS}/Include ${SPL}/Include)
target_compile_definitions(gd32f50x PUBLIC GD32F50X GD32F503 USE_STDPERIPH_DRIVER HXTAL_VALUE=8000000)
add_executable(claw_ctrl
    Core/Src/main.c Core/Src/board.c Core/Src/syscalls.c
    ${CMSIS}/Source/GCC/startup_gd32f50x.S
    device_drivers/drv_can.c device_drivers/drv_uart.c
    device_drivers/drv_rgb_led.c device_drivers/drv_fill_light.c
    service/srv_juxie.c service/srv_indicator.c
    tasks/indicator_task.c tasks/log_task.c
    m_middlewares/utils/kfifo.c m_middlewares/utils/msg_fifo.c
    m_middlewares/log/log.c m_middlewares/Third_Party/mpaland_printf/printf.c)
set_target_properties(claw_ctrl PROPERTIES SUFFIX .elf)
target_include_directories(claw_ctrl PRIVATE device_drivers service tasks
    m_middlewares/utils m_middlewares/log m_middlewares/Third_Party/mpaland_printf)
target_compile_definitions(claw_ctrl PRIVATE PRINTF_DISABLE_SUPPORT_FLOAT PRINTF_DISABLE_SUPPORT_EXPONENTIAL)
target_compile_options(claw_ctrl PRIVATE -Wall -Wextra -Wshadow)
target_link_libraries(claw_ctrl PRIVATE gd32f50x)
target_link_options(claw_ctrl PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/gd32f503xE_flash.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/claw_ctrl.map
    -Wl,--gc-sections,--print-memory-usage --specs=nano.specs --specs=nosys.specs)
set_property(TARGET claw_ctrl APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gd32f503xE_flash.ld)
add_custom_command(TARGET claw_ctrl POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE_NAME:claw_ctrl> claw_ctrl.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE_NAME:claw_ctrl> claw_ctrl.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE_NAME:claw_ctrl>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} VERBATIM)
add_custom_target(memory
    COMMAND ${CMAKE_COMMAND} "-DSIZE_TOOL=${CMAKE_SIZE}"
        "-DELF_FILE=$<TARGET_FILE_NAME:claw_ctrl>"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/report-memory.cmake
    DEPENDS claw_ctrl
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Reporting GD32 memory usage" VERBATIM)
