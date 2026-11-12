cmake_minimum_required(VERSION 3.24)
project(claw_ctrl VERSION 1.0.0 LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FW ${CMAKE_CURRENT_SOURCE_DIR}/Drivers)
set(CMSIS ${FW}/CMSIS/GD/GD32F50x)
set(SPL ${FW}/GD32F50x_standard_peripheral)
set(FLASH_BASE 0x08000000)
set(FLASH_SIZE 0x80000)
set(FLASH_PAGE_SIZE 0x800)
set(BOOT_SIZE 0x8000)
set(METADATA_SIZE 0x2000)
set(APP_HEADER_OFFSET 0x200)
set(APP_MAGIC 0x41505031)
set(APP_TARGET 0x434C4157)
set(APP_VERSION_MAGIC 0x56524531)
foreach(component IN ITEMS MAJOR MINOR PATCH)
    if(PROJECT_VERSION_${component} GREATER 255)
        message(FATAL_ERROR "Each firmware version component must fit in one byte")
    endif()
endforeach()
math(EXPR APP_VERSION "(${PROJECT_VERSION_MAJOR} << 16) | (${PROJECT_VERSION_MINOR} << 8) | ${PROJECT_VERSION_PATCH}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR APP_SIZE "(${FLASH_SIZE} - ${BOOT_SIZE} - ${METADATA_SIZE}) / 2" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR APP_A "${FLASH_BASE} + ${BOOT_SIZE}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR APP_B "${APP_A} + ${APP_SIZE}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR METADATA_BASE "${APP_B} + ${APP_SIZE}" OUTPUT_FORMAT HEXADECIMAL)
configure_file(cmake/flash_layout.h.in flash_layout.h @ONLY)

add_library(gd32f50x STATIC
    ${CMSIS}/Source/system_gd32f50x.c
    ${SPL}/Source/gd32f50x_rcu.c
    ${SPL}/Source/gd32f50x_gpio.c
    ${SPL}/Source/gd32f50x_misc.c
    ${SPL}/Source/gd32f50x_can.c
    ${SPL}/Source/gd32f50x_fmc.c
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
    service/srv_juxie.c service/srv_indicator.c service/srv_board_protocol.c service/srv_fill_light.c
    service/srv_upgrade.c
    tasks/indicator_task.c tasks/log_task.c)
target_include_directories(claw_ctrl PRIVATE device_drivers service tasks
    m_middlewares/utils m_middlewares/log m_middlewares/Third_Party/mpaland_printf)
target_compile_definitions(claw_ctrl PRIVATE PRINTF_DISABLE_SUPPORT_FLOAT PRINTF_DISABLE_SUPPORT_EXPONENTIAL)
target_compile_options(claw_ctrl PRIVATE -Wall -Wextra -Wshadow)
add_subdirectory(m_middlewares)
set(RS m_middlewares/Third_Party/ring_storage)
add_library(upgrade_storage STATIC
    boot/upgrade_storage.c device_drivers/drv_flash.c
    ${RS}/ring_storage.c ${RS}/rs_crc32.c)
target_include_directories(upgrade_storage PUBLIC boot ${RS} ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE device_drivers)
target_link_libraries(upgrade_storage PUBLIC gd32f50x PRIVATE m_middlewares)
target_compile_options(upgrade_storage PRIVATE -Wall -Wextra -Wshadow)
target_link_libraries(claw_ctrl PRIVATE m_middlewares upgrade_storage)

add_executable(claw_boot boot/boot_main.c Core/Src/syscalls.c
    device_drivers/drv_fill_light.c ${CMSIS}/Source/GCC/startup_gd32f50x.S)
target_include_directories(claw_boot PRIVATE device_drivers)
target_link_libraries(claw_boot PRIVATE upgrade_storage)
target_compile_options(claw_boot PRIVATE -Wall -Wextra -Wshadow)

function(configure_image target base size)
    set_target_properties(${target} PROPERTIES SUFFIX .elf)
    set(linker_script ${CMAKE_CURRENT_SOURCE_DIR}/gd32f503xE_flash.ld)
    target_link_options(${target} PRIVATE
        -Wl,--wrap=SystemInit
        -Wl,--defsym=__flash_base=${base},--defsym=__flash_size=${size}
        -Wl,--defsym=__app_header_offset=${APP_HEADER_OFFSET}
        -T${linker_script} -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${target}.map
        -Wl,--gc-sections,--print-memory-usage --specs=nano.specs --specs=nosys.specs)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${linker_script})
    add_custom_command(OUTPUT ${target}.bin ${target}.hex
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${target}> ${target}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> ${target}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${target}>
        DEPENDS ${target} VERBATIM)
endfunction()
configure_image(claw_ctrl ${APP_A} ${APP_SIZE})
configure_image(claw_boot ${FLASH_BASE} ${BOOT_SIZE})

find_program(PACK_SHELL NAMES powershell pwsh REQUIRED)
set(pack_script ${CMAKE_CURRENT_SOURCE_DIR}/scripts/pack-firmware.ps1)
add_custom_command(OUTPUT claw_factory.bin claw_factory.hex claw_factory.jlink
    COMMAND ${PACK_SHELL} -NoProfile -ExecutionPolicy Bypass -File ${pack_script}
        -Objcopy ${CMAKE_OBJCOPY} -FlashBase ${FLASH_BASE} -FlashSize ${FLASH_SIZE}
        -AppBase ${APP_A} -AppSize ${APP_SIZE} -MetadataBase ${METADATA_BASE}
        -HeaderOffset ${APP_HEADER_OFFSET} -AppMagic ${APP_MAGIC} -AppTarget ${APP_TARGET}
        -VersionMagic ${APP_VERSION_MAGIC}
    DEPENDS claw_boot.bin claw_ctrl.bin ${pack_script} CMakeLists.txt VERBATIM)
add_custom_target(firmware ALL DEPENDS claw_factory.bin claw_factory.hex claw_factory.jlink)
math(EXPR BOOT_CAPACITY "${BOOT_SIZE}")
math(EXPR APP_CAPACITY "${APP_SIZE}")
add_custom_target(memory
    COMMAND ${CMAKE_COMMAND} "-DSIZE_TOOL=${CMAKE_SIZE}"
        "-DELF_FILE=$<TARGET_FILE_NAME:claw_boot>" "-DFLASH_CAPACITY=${BOOT_CAPACITY}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/report-memory.cmake
    COMMAND ${CMAKE_COMMAND} "-DSIZE_TOOL=${CMAKE_SIZE}"
        "-DELF_FILE=$<TARGET_FILE_NAME:claw_ctrl>" "-DFLASH_CAPACITY=${APP_CAPACITY}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/report-memory.cmake
    DEPENDS claw_boot claw_ctrl
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Reporting GD32 memory usage" VERBATIM)
