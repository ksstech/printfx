# PRINTFX

set( psd ${CMAKE_BINARY_DIR} )
string( LENGTH ${psd} len )
math( EXPR len "${len} - 12" )
string( SUBSTRING ${psd} ${len} 6 pn )

if( pn MATCHES irmacs )
	idf_component_register(
		SRCS "printfx.c"
		INCLUDE_DIRS "."
		REQUIRES common rtos-support
		PRIV_REQUIRES stringsX values x_sockets
	)
else()
#	set( REQUIRES common rtos-support )
#	set( PRIV_REQUIRES stringsX values x_sockets )
	project( printfx )
	add_library( printfx "printfx.c" )
	target_include_directories( printfx PUBLIC "." )
	target_link_libraries( printfx common )
endif()
