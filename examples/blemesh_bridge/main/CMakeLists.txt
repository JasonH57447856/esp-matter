

set(PRIV_REQUIRES_LIST device esp_matter app_bridge)

idf_component_register(SRC_DIRS          "."
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/."
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/."
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/aes/."
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/des/."
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/base64/."
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/struct2json/."
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/struct2json/cJSON/."
                      PRIV_INCLUDE_DIRS  "."
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c"
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/aes"
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/des"
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/base64"
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/struct2json"
					 "${CMAKE_CURRENT_LIST_DIR}/lockly_c/struct2json/cJSON"
                                         "${ESP_MATTER_PATH}/examples/common/blemesh_platform"
                      PRIV_REQUIRES      ${PRIV_REQUIRES_LIST}
		      REQUIRES mqtt json )

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 14)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
