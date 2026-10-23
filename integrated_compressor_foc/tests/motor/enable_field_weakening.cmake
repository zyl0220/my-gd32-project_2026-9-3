# COMPILE TEST ONLY: synthetic motor/board values; never flash this image.
message(WARNING "Synthetic field-weakening compile test: DO NOT FLASH")
add_compile_options("$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_LIST_DIR}/field_weakening_parameters.h>")
