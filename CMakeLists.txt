idf_component_register(
	SRCS "printfx.c"
	INCLUDE_DIRS "."
	PRIV_INCLUDE_DIRS
	REQUIRES common rtos-support
	PRIV_REQUIRES stringsX values x_sockets
)
