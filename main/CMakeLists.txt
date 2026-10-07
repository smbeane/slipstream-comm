set(EXTERNAL_C_SRCS
    # Add external C source paths here, relative to this file.
)

idf_component_register(
    PRIV_REQUIRES 
        bt
    SRCS
        "main.cpp"
        "dac_audio.cpp"
        ${EXTERNAL_C_SRCS}
    INCLUDE_DIRS
        "."

)