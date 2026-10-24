# Loads the built components and drives them through the SDK with a mocked core. See README.md.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(tested_components
	Checkpoints
	GangZones
	Pickups
	Timers
	Variables
)

set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(mocks_gen "${generated_dir}/mocks_gen.hpp")
add_custom_command(
	OUTPUT "${mocks_gen}"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${generated_dir}"
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/genmock.py" "${CMAKE_SOURCE_DIR}" "${mocks_gen}"
	DEPENDS
		genmock.py
		"${CMAKE_SOURCE_DIR}/SDK/include/core.hpp"
		"${CMAKE_SOURCE_DIR}/SDK/include/entity.hpp"
		"${CMAKE_SOURCE_DIR}/SDK/include/network.hpp"
		"${CMAKE_SOURCE_DIR}/SDK/include/player.hpp"
	COMMENT "Generating SDK mocks"
	VERBATIM
)

add_executable(component-tests
	main.cpp
	checkpoints.cpp
	gangzones.cpp
	pickups.cpp
	timers.cpp
	variables.cpp
	harness.hpp
	mocks.hpp
	"${mocks_gen}"
)

target_include_directories(component-tests PRIVATE "${generated_dir}")
target_link_libraries(component-tests PRIVATE OMP-SDK OMP-NetCode ${CMAKE_DL_LIBS})
target_compile_definitions(component-tests PRIVATE
	COMPONENTS_DIR="$<TARGET_FILE_DIR:Timers>"
	COMPONENT_SUFFIX="${CMAKE_SHARED_LIBRARY_SUFFIX}"
)
add_dependencies(component-tests ${tested_components})
set_property(TARGET component-tests PROPERTY FOLDER "Server/Tests")
# Keep it out of the server output directory, which is what gets packaged.
set_property(TARGET component-tests PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")

add_test(NAME component-tests COMMAND component-tests)
