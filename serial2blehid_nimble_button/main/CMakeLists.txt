set(COMPONENT_SRCS
		"main.c"
		"gatt_svr.c"
		"gatt_vars.c"
		"ble_func.c"
		"hid_func.c"
		"gpio_func.c"
		"my_hid_key_map_jp.c"
)
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
