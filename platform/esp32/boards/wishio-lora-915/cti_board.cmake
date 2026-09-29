function(cti_configure_board CTI_BOARD_DIR TARGET)
    target_sources(${TARGET} PRIVATE
        "${CTI_BOARD_DIR}/wishio_board.cpp"
    )
    target_include_directories(${TARGET} PRIVATE "${CTI_BOARD_DIR}")
endfunction()
