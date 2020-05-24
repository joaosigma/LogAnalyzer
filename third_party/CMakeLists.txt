file(
	GLOB_RECURSE _sources
	LIST_DIRECTORIES false
	"*.cc" "*.hpp" "*.h"
)

add_library(third_party STATIC ${_sources})

target_include_directories(third_party INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${_sources})
