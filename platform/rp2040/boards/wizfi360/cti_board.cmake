message ("\n-- Configuring mdns-test build --")
add_executable(mdns-test)

target_sources(mdns-test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/firmware/mdns-test.cpp
)

target_link_libraries(mdns-test
    pico_stdlib
    hardware_i2c
)

#tell the pico-sdk to generate the additional Pi Pico output files (uf2)
#for the created executable build spec target
pico_add_extra_outputs(mdns-test)

message("\n")