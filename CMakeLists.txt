add_executable(PicoRapidX
    PicoRapidX.c
)

include_directories(header)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(
    PicoRapidX
    pico_stdlib
    hardware_flash
    hardware_sync
    pico_multicore
    hardware_i2c
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(PicoRapidX)
