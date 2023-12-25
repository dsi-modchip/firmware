set(PROJECT "dsihaxfw")

option(PICO_NO_FLASH "Disable writing the compiled program to flash, and only load it to RAM. Useful for testing, but not much else (OFF by default)." OFF)
option(PICO_COPY_TO_RAM "Run all code in RAM, while the program is also stored on flash. On bootup, everything will be copied to RAM (OFF by default)." OFF)
option(HAXMODE_DEV_FEATURES "Enable extra features used when developing the hardware (eg. I2C, PWSW control, no JTAG) (OFF by default)" OFF)
option(PASSIVE_TEST "Do not assert RESET, PWSW or inject glitches. Useful only for testing. (OFF by default)" OFF)

cmake_minimum_required(VERSION 3.11)

if(NOT HAXMODE_OVERRIDE)
	set(HAXMODE_OVERRIDE "")
endif()
set(HAXMODE_OVERRIDE "${HAXMODE_OVERRIDE}" CACHE STRING "Compile-time override for runtime mode detection ('flash', 'attack' or empty)")

if(NOT PICO_BOARD)
	message(SEND_ERROR "No board selected!")
endif()
message(STATUS "NOTE: selected board is '${PICO_BOARD}'.")
set(PICO_BOARD "${PICO_BOARD}" CACHE STRING "RP2040 board used ('pico', 'haxboard_r1', 'haxboard_r2', 'seeed_xiao_rp2040', 'waveshare_rp2040_one', 'waveshare_rp2040_zero', 'adafruit_itsybitsy_rp2040')")

# choose default boot2
if(NOT PICO_DEFAULT_BOOT_STAGE2 AND NOT PICO_DEFAULT_BOOT_STAGE2_FILE)
	if("${PICO_BOARD}" STREQUAL "haxboard_r1")
		set(PICO_DEFAULT_BOOT_STAGE2 "boot2_w25q080")
		#set(PICO_DEFAULT_BOOT_STAGE2 "boot2_is25lp080")
		#set(PICO_DEFAULT_BOOT_STAGE2 "boot2_generic_03h")
	elseif("${PICO_BOARD}" STREQUAL "haxboard_r2")
		set(PICO_DEFAULT_BOOT_STAGE2 "boot2_w25q080")
	elseif("${HAXMODE_OVERRIDE}" STREQUAL "")
		message(SEND_ERROR "Using a non-custom board needs HAXMODE_OVERRIDE to be defined!")
	endif()
endif()

message(STATUS "NOTE: selected stage2 is '${PICO_DEFAULT_BOOT_STAGE2}'.")

if("${HAXMODE_OVERRIDE}" STREQUAL "")
else()
	add_compile_definitions(HAXMODE_OVERRIDE=haxmode_${HAXMODE_OVERRIDE})
endif()

if(HAXMODE_DEV_FEATURES)
	add_compile_definitions(HAXMODE_DEV_FEATURES)
endif()
if(PASSIVE_TEST)
	add_compile_definitions(PASSIVE_TEST)
endif()

message(STATUS "HAXMODE_OVERRIDE=${HAXMODE_OVERRIDE}, HAXMODE_DEV_FEATURES=${HAXMODE_DEV_FEATURES}, PASSIVE_TEST=${PASSIVE_TEST}")

# always use stack guards
# BORKED IN SDK LMAO
# FIXME: reevaluate in 1.4.0 and 1.5.0
# eh, by now my version is better imo
#add_compile_definitions(PICO_USE_STACK_GUARDS 1)

list(APPEND PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src/boards/")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/pico_sdk_import.cmake)

project(${PROJECT})

pico_sdk_init()

add_executable(${PROJECT})

# we use our own stdio impl
pico_enable_stdio_uart(${PROJECT} 0)
pico_enable_stdio_usb(${PROJECT} 0)

target_sources(${PROJECT} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/libco/libco.S

	${CMAKE_CURRENT_SOURCE_DIR}/lib/glitch/glitch.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/glitch/glitch_param.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/spiperi/spiperi.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/stdio/stdio_uart_dma.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/tusb/reset_interface.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/tusb/tusb_comms.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/hardfault.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/thread_base.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/util.c

	${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/thread.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/ledctl.c

	${CMAKE_CURRENT_SOURCE_DIR}/src/twlitf/csmux.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/twlitf/spiflash.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/twlitf/spictl.c

	${CMAKE_CURRENT_SOURCE_DIR}/src/twlitf/mcenv.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/twlitf/glitchitf.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/twlitf/boothax.c

	${CMAKE_CURRENT_SOURCE_DIR}/src/tusb/dfu.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/tusb/usb_descriptors.c
)

target_include_directories(${PROJECT} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/libco/

	${CMAKE_CURRENT_SOURCE_DIR}/lib/glitch/
	${CMAKE_CURRENT_SOURCE_DIR}/lib/spiperi/
	${CMAKE_CURRENT_SOURCE_DIR}/lib/stdio/
	${CMAKE_CURRENT_SOURCE_DIR}/lib/tusb/
	${CMAKE_CURRENT_SOURCE_DIR}/lib/

	${CMAKE_CURRENT_SOURCE_DIR}/src/
	${CMAKE_CURRENT_SOURCE_DIR}/src/boards/
	${CMAKE_CURRENT_SOURCE_DIR}/src/tusb/
	${CMAKE_CURRENT_SOURCE_DIR}/src/twlitf/
)

target_link_libraries(${PROJECT}
	pico_stdlib pico_unique_id hardware_pio hardware_dma hardware_uart
	hardware_spi hardware_i2c hardware_timer hardware_pwm
	cmsis_core pico_time pico_multicore tinyusb_device_unmarked
	pico_usb_reset_interface_headers pico_unique_id
)

if(NOT PICO_NO_FLASH)
	if(PICO_COPY_TO_RAM)
		pico_set_linker_script(${PROJECT} ${CMAKE_CURRENT_SOURCE_DIR}/link/mmap_c2r_payload.ld)
	else()
		pico_set_linker_script(${PROJECT} ${CMAKE_CURRENT_SOURCE_DIR}/link/mmap_def_payload.ld)
	endif()
endif()

pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_SOURCE_DIR}/lib/glitch/trigctl.pio)
pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_SOURCE_DIR}/lib/spiperi/spiperi.pio)
pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_SOURCE_DIR}/src/twlitf/csmux.pio)
pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_SOURCE_DIR}/src/ws2812.pio)

pico_add_extra_outputs(${PROJECT})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror=implicit-function-declaration -Werror=return-type -Werror=aggressive-loop-optimizations")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--cref")

