# Cria um driver static target Drivers_Concentrador
add_library(
  cat_comedor_source
  STATIC
  get_times.c
  )

# Adciona ao target o include necessario, quando alguem usar externamente esse target, esse include sera visto
target_include_directories(
  cat_comedor_source
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cat_comedor_source PRIVATE zephyr_interface)
target_link_libraries(cat_comedor_source PRIVATE)