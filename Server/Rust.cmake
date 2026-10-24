# Support for components being ported to Rust. A ported component keeps a thin C++ shim
# implementing the SDK interfaces and links its logic from a Rust static library in the Cargo
# workspace at the repository root.

find_program(CARGO_EXECUTABLE cargo
	HINTS "$ENV{CARGO_HOME}/bin" "$ENV{HOME}/.cargo/bin" "$ENV{USERPROFILE}/.cargo/bin"
)

set(RUST_TARGET "" CACHE STRING "Rust target triple to build for; detected from the C++ target when empty")

function(detect_rust_target out)
	if(RUST_TARGET)
		set(${out} ${RUST_TARGET} PARENT_SCOPE)
		return()
	endif()

	if(WIN32)
		if(CMAKE_GENERATOR_PLATFORM MATCHES "^(ARM64|arm64)$")
			set(target aarch64-pc-windows-msvc)
		elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
			set(target i686-pc-windows-msvc)
		else()
			set(target x86_64-pc-windows-msvc)
		endif()
	elseif(APPLE)
		if(TARGET_BUILD_ARCH MATCHES "(armv8|arm64)")
			set(target aarch64-apple-darwin)
		else()
			set(target x86_64-apple-darwin)
		endif()
	else()
		# Mirrors the TARGET_BUILD_ARCH handling in the root CMakeLists.txt.
		if(TARGET_BUILD_ARCH MATCHES "(x64|x86_64|amd64)")
			set(target x86_64-unknown-linux-gnu)
		elseif(TARGET_BUILD_ARCH MATCHES "(i[3-6]86|x86)")
			set(target i686-unknown-linux-gnu)
		elseif(TARGET_BUILD_ARCH MATCHES "^armv8")
			set(target aarch64-unknown-linux-gnu)
		elseif(TARGET_BUILD_ARCH MATCHES "^armv7")
			set(target armv7-unknown-linux-gnueabihf)
		else()
			message(FATAL_ERROR "Can't map TARGET_BUILD_ARCH '${TARGET_BUILD_ARCH}' to a Rust target, set RUST_TARGET")
		endif()
	endif()

	set(${out} ${target} PARENT_SCOPE)
endfunction()

# Build the Cargo package `package` as a static library and link it into `target`.
function(link_rust_library target package)
	if(NOT CARGO_EXECUTABLE)
		message(FATAL_ERROR "cargo not found; install Rust from https://rustup.rs to build ${target}")
	endif()

	detect_rust_target(rust_target)
	string(REPLACE "-" "_" lib_name ${package})
	set(cargo_target_dir "${CMAKE_BINARY_DIR}/cargo")

	if(WIN32)
		set(lib_file "${lib_name}.lib")
	else()
		set(lib_file "lib${lib_name}.a")
	endif()

	# Cargo has its own dependency tracking, so always run it and let it decide what's stale.
	get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
	if(multi_config)
		set(profile_dir "$<IF:$<CONFIG:Debug>,debug,release>")
		set(release_flag "$<$<NOT:$<CONFIG:Debug>>:--release>")
		set(lib_path "${cargo_target_dir}/${rust_target}/${profile_dir}/${lib_file}")
		set(byproducts "")
	else()
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			set(profile_dir debug)
			set(release_flag "")
		else()
			set(profile_dir release)
			set(release_flag --release)
		endif()
		set(lib_path "${cargo_target_dir}/${rust_target}/${profile_dir}/${lib_file}")
		set(byproducts BYPRODUCTS "${lib_path}")
	endif()

	add_custom_target(${target}-rust
		COMMAND "${CARGO_EXECUTABLE}" build
			--package ${package}
			--lib
			--locked
			--target ${rust_target}
			--target-dir "${cargo_target_dir}"
			${release_flag}
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		${byproducts}
		USES_TERMINAL
		COMMENT "Building Rust package ${package} for ${rust_target}"
		VERBATIM
	)
	set_property(TARGET ${target}-rust PROPERTY FOLDER "Server/Components")
	add_dependencies(${target} ${target}-rust)

	target_link_libraries(${target} PRIVATE "${lib_path}")

	# Native libraries the Rust standard library needs (`cargo rustc -- --print native-static-libs`).
	if(WIN32)
		target_link_libraries(${target} PRIVATE kernel32 ntdll userenv ws2_32 dbghelp)
	elseif(APPLE)
		target_link_libraries(${target} PRIVATE iconv System c m)
	else()
		target_link_libraries(${target} PRIVATE gcc_s util rt pthread m dl)
		# Keep the static library's symbols (including Rust's std) private to this component.
		target_link_options(${target} PRIVATE "LINKER:--exclude-libs,ALL")
	endif()
endfunction()
