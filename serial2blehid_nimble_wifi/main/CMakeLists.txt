set(COMPONENT_SRCS
		"main.c"
		"gatt_svr.c"
		"gatt_vars.c"
		"ble_func.c"
		"hid_func.c"
		"my_hid_key_map_jp.c"
		"my_httpd.c"
		"my_if_uart.c"
		"my_ring_buffer.c"
		"my_softap.c"
)
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
