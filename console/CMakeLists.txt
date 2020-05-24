file(
	GLOB_RECURSE _sources
	LIST_DIRECTORIES false
	"*.cpp" "*.hpp"
)

add_executable(console ${_sources})

target_link_libraries(console PRIVATE core)
target_link_libraries(console PRIVATE third_party)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${_sources})
