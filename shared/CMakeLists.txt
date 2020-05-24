file(
	GLOB_RECURSE _sources
	LIST_DIRECTORIES false
	"*.cpp" "*.hpp" "*.h"
)

add_library(shared SHARED ${_sources})

target_compile_definitions(shared PRIVATE LA_BUILDING_SHARED)

target_link_libraries(shared PRIVATE core)
target_link_libraries(shared PRIVATE third_party)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${_sources})
