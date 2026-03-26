# PRINTFX

if(ESP_PLATFORM)
	set( srcs "src/printfx.c" "src/report.c" "src/printfx_tests.c" )
	set( include_dirs "include" )
	set( priv_include_dirs )
	set( requires )
	set( priv_requires )

	idf_component_register(
		SRCS ${srcs}
		INCLUDE_DIRS ${include_dirs}
		PRIV_INCLUDE_DIRS ${priv_include_dirs}
		REQUIRES ${requires}
		PRIV_REQUIRES ${priv_requires}
	)
else()
	project( printfx )
	add_library( "src/printfx" "src/printfx.c" "src/report.c" )
	target_include_directories( printfx PUBLIC "." )
	target_link_libraries( printfx common )
endif()
