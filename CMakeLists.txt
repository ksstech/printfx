# PRINTFX

set( vBase "/Users/andremaree/Dropbox/devs/ws/" )
string( LENGTH ${vBase} vBaseLen )

set( vO1 "esp/irmacs" )
string( LENGTH ${vO1} vO1L )

set( vO2 "osx/test1" )
string( LENGTH ${vO2} vO2L )

set( vO3 "xcode/test1/test1" )
string( LENGTH ${vO3} vO3L )

set( vNow ${CMAKE_SOURCE_DIR} )
string( SUBSTRING ${vNow} ${vBaseLen} ${vO1L} vO1S )
string( SUBSTRING ${vNow} ${vBaseLen} ${vO2L} vO2S )
string( SUBSTRING ${vNow} ${vBaseLen} ${vO3L} vO3S )

if( vO1S MATCHES esp/irmacs )
	idf_component_register(
		SRCS "printfx.c"
		INCLUDE_DIRS "."
		REQUIRES common rtos-support
		PRIV_REQUIRES stringsX values socketsX
	)
elseif( vO2S MATCHES osx/test1 )
	project( printfx )
	add_library( printfx "printfx.c" )
	target_include_directories( printfx PUBLIC "." )
	target_link_libraries( printfx common )

elseif( vO3S MATCHES xcode/test1/test1 )
	project( printfx )
	add_library( printfx "printfx.c" )
	target_include_directories( printfx PUBLIC "." )
	target_link_libraries( printfx common )

else()
	message( "vO1S : ${vO1S}" )
	message( "vO2S : ${vO2S}" )
	message( "vO3S : ${vO3S}" )
endif()
