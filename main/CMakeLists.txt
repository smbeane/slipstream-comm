set(EXTERNAL_C_SRCS
    # Add external C source paths here, relative to this file.
)

idf_component_register(
    PRIV_REQUIRES
        bt
        esp_driver_dac
        nvs_flash
        esp_ringbuf
    SRCS
        "main.cpp"
        "dac/dac_audio.cpp"
        "bluetooth/bluetooth.cpp"
        "bluetooth/a2dp.cpp"
        "bluetooth/hfp.cpp"
        "controls/controls.cpp"
        ${EXTERNAL_C_SRCS}
    INCLUDE_DIRS
        "."

)