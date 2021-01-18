# PRINTFX

set( vBase "/Users/andremaree/Dropbox/Devs/ws/" )
string( LENGTH ${vBase} vBaseLen )

set( vIrmacs "esp/irmacs" )
string( LENGTH ${vIrmacs} vIrmacsLen )

set( vCompTest "osx/ComponentTest" )
string( LENGTH ${vCompTest} vCompTestLen )

set( vNow ${CMAKE_SOURCE_DIR} )
string( SUBSTRING ${vNow} ${vBaseLen} ${vIrmacsLen} vOpt1 )
string( SUBSTRING ${vNow} ${vBaseLen} ${vCompTestLen} vOpt2 )

if( vOpt1 MATCHES esp/irmacs )
	idf_component_register(
		SRCS "printfx.c"
		INCLUDE_DIRS "."
		REQUIRES common rtos-support
		PRIV_REQUIRES stringsX values socketsX
	)
elseif( vOpt2 MATCHES osx/ComponentTest )
	project( printfx )
	add_library( printfx "printfx.c" )
	target_include_directories( printfx PUBLIC "." )
	target_link_libraries( printfx common )

else()
	message( "vOpt1 : ${vOpt1}" )
	message( "vOpt2 : ${vOpt2}" )
endif()
