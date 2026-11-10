# GD32 port: portable modules; hardware-dependent backtrace/storage/RTOS ports stay opt-in.
add_library(m_middlewares STATIC)
set(_mid_dirs utils framework log key_base protocol_tools algorithm algorithm/pll
    algorithm/controller algorithm/filter algorithm/math Third_Party/mpaland_printf)
foreach(_dir IN LISTS _mid_dirs)
    file(GLOB _sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}/*.c")
    target_sources(m_middlewares PRIVATE ${_sources})
    target_include_directories(m_middlewares PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}")
endforeach()
target_include_directories(m_middlewares PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(m_middlewares PUBLIC gd32f50x m)
target_compile_definitions(m_middlewares PRIVATE PRINTF_DISABLE_SUPPORT_FLOAT PRINTF_DISABLE_SUPPORT_EXPONENTIAL)
# Copied third-party sources are available for later GD32 ports. Do not link their
# Cortex-M4 fault handler, Flash port or RTOS hooks into this bare-metal M33 build.
