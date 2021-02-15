# PRINTFX

set( vBase "/Users/andremaree/Dropbox/devs/ws/" )
set( vNow ${CMAKE_SOURCE_DIR} )
string( LENGTH ${vBase} vBaseLen )

set( p1 "esp/irmacs" )
string( LENGTH ${p1} p1L )
string( SUBSTRING ${vNow} ${vBaseLen} ${p1L} p1S )

set( p2 "esp/test" )
string( LENGTH ${p2} p2L )
string( SUBSTRING ${vNow} ${vBaseLen} ${p2L} p2S )

set( p3 "osx/test1" )
string( LENGTH ${p3} p3L )
string( SUBSTRING ${vNow} ${vBaseLen} ${p3L} p3S )

set( p4 "xcode/test1/test1" )
string( LENGTH ${p4} p4L )
string( SUBSTRING ${vNow} ${vBaseLen} ${p4L} p4S )

if( p1S MATCHES esp/irmacs OR p2S MATCHES esp/test)
	idf_component_register(
		SRCS "printfx.c"
		INCLUDE_DIRS "."
		REQUIRES common rtos-support
		PRIV_REQUIRES stringsX values socketsX
	)
elseif( p3S MATCHES osx/test1 )
	project( printfx )
	add_library( printfx "printfx.c" )
	target_include_directories( printfx PUBLIC "." )
	target_link_libraries( printfx common )

elseif( p4S MATCHES xcode/test1/test1 )
	project( printfx )
	add_library( printfx "printfx.c" )
	target_include_directories( printfx PUBLIC "." )
	target_link_libraries( printfx common )

else()
	message( "p1 : ${p1L} / ${p1S}" )
	message( "p2 : ${p2L} / ${p2S}" )
	message( "p3 : ${p3L} / ${p3S}" )
	message( "p4 : ${p4L} / ${p4S}" )
endif()
