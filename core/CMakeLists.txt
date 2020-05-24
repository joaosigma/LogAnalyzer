file(
	GLOB_RECURSE _sources
	LIST_DIRECTORIES false
	"*.cpp" "*.hpp"
)

add_library(core STATIC ${_sources})
target_link_libraries(core PRIVATE third_party)

target_include_directories(core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${_sources})
