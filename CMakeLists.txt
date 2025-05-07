# PRINTFX

if( ESP_PLATFORM )
	set( srcs "printfx_v0.c" "report.c" "printfx_tests.c" )
	set( include_dirs "." )
	set( priv_include_dirs )
	set( requires "buffers socketsX" )
	set( priv_requires )

	idf_component_register(
		SRCS ${srcs}
		INCLUDE_DIRS ${include_dirs}
		PRIV_INCLUDE_DIRS ${priv_include_dirs}
		REQUIRES ${requires}
		PRIV_REQUIRES ${priv_requires}
	)
else( )
	project( printfx )
	add_library( printfx "printfx.c" "report.c" )
	target_include_directories( printfx PUBLIC "." )
	target_link_libraries( printfx common )
endif()
