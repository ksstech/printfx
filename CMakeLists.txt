set(srcs "printfx.c" )
set(include_dirs "." )
#set(priv_include_dirs "." )
set(requires common )
set(priv_requires stringsX common values x_sockets hal_esp32 )
idf_component_register( SRCS ${srcs}
						INCLUDE_DIRS ${include_dirs} 
#						PRIV_INCLUDE_DIRS ${priv_include_dirs} 
						REQUIRES ${requires} 
						PRIV_REQUIRES ${priv_requires}
						)
