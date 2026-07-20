cmake_minimum_required(VERSION 3.15)
project(Labyrinth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KISSFFT_TEST OFF CACHE BOOL "Disable KissFFT tests" FORCE)

set(KISSFFT_STATIC ON CACHE BOOL "Build static library" FORCE)

set(FE_INCLUDE_OPENVR OFF CACHE BOOL "Include OpenVR" FORCE)


# set(FE_EXCLUDE_JOLT OFF CACHE BOOL "Disable JOLT" FORCE)

set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../FenixEngine")

# add_subdirectory(${CMAKE_SOURCE_DIR}/../FenixEngine/src/Engine ${CMAKE_BINARY_DIR}/out/Engine)

# if(NOT TARGET Jolt)
#     add_subdirectory(${ENGINE_DIR}/external/JoltPhysics/Build ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/jolt)
# endif()

set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use dynamic MSVC runtime to match rest of project" FORCE)

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

# if(NOT TARGET Jolt)
#     add_subdirectory(${ENGINE_DIR}/external/JoltPhysics/Build ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/jolt)
#     set_target_properties(Jolt PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
# endif()

if(NOT TARGET Engine)
    message(STATUS "Engine target not found. Looking for engine source...")
    
    if(EXISTS "${ENGINE_DIR}/CMakeLists.txt")
        add_subdirectory("${ENGINE_DIR}" FenixEngine)
    # elif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/FenixEngine/CMakeLists.txt")
    #     add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/FenixEngine" FenixEngine)
    else()
        message(FATAL_ERROR "FenixEngine not found!")
    endif()
endif()

set(IMGUI_DIR ../FenixEngine/external/imgui)

file(GLOB_RECURSE IMGUI_SOURCE_FILES
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_draw.cpp
)

add_executable(Labyrinth
	WIN32
	${IMGUI_SOURCE_FILES}
	main.cpp
)

target_include_directories(Labyrinth PRIVATE
	include
	${ENGINE_DIR}
	${ENGINE_DIR}/include
	${ENGINE_DIR}/external
	${ENGINE_DIR}/external/imgui
	${ENGINE_DIR}/external/kissfft
	${ENGINE_DIR}/external/SDL/include
	${ENGINE_DIR}/src/Engine
)

target_link_libraries(Labyrinth PRIVATE
	Engine
	# Jolt::Jolt
)

if(NOT WIN32)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
target_include_directories(Labyrinth PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
target_link_libraries(Labyrinth PRIVATE ${PIPEWIRE_LIBRARIES})
target_compile_options(Labyrinth PRIVATE ${PIPEWIRE_CFLAGS_OTHER})

find_package(X11 REQUIRED)

target_link_libraries(Labyrinth PRIVATE X11::X11)
endif()

set_property(TARGET Labyrinth PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	
if(MSVC)
	target_compile_options(Labyrinth PRIVATE $<$<CONFIG:Debug>:/Zi>)

	target_link_options(Labyrinth PRIVATE $<$<CONFIG:Debug>:/DEBUG>)

	add_compile_options(/RTC1)
endif()

# ---------------------------------------------------------------
# Copy all runtime resources (glsl shaders, textures) to build output
# ---------------------------------------------------------------
set(RESOURCE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(RESOURCE_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/resources)

add_custom_target(CopyResources_Labyrinth ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${RESOURCE_SRC_DIR} ${RESOURCE_OUT_DIR}
	COMMENT "Copying resources to ${RESOURCE_OUT_DIR}"
)

# ---------------------------------------------------------------
# GLSL -> SPIR-V shader compilation
# ---------------------------------------------------------------
find_program(GLSLC glslc HINTS
	$ENV{VULKAN_SDK}/Bin
	$ENV{VULKAN_SDK}/bin
)

if(GLSLC)
	message(STATUS "Found glslc: ${GLSLC}")

	set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders)
	set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/resources/shaders)
	file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

	set(SHADER_SOURCES
		${SHADER_SRC_DIR}/VertexShader_vk.glsl
		${SHADER_SRC_DIR}/FragmentShader_vk.glsl
	)

	foreach(SHADER ${SHADER_SOURCES})
		get_filename_component(SHADER_NAME ${SHADER} NAME_WE)

		if(${SHADER_NAME} MATCHES ".*Fragment.*")
			set(SHADER_STAGE frag)
		else()
			set(SHADER_STAGE vert)
		endif()

		set(SPIRV_OUTPUT ${SHADER_OUT_DIR}/${SHADER_NAME}.spv)

		add_custom_command(
			OUTPUT ${SPIRV_OUTPUT}
			COMMAND ${GLSLC} -fshader-stage=${SHADER_STAGE} --target-env=vulkan1.0 -DFE_VULKAN ${SHADER} -o ${SPIRV_OUTPUT}
			DEPENDS ${SHADER}
			COMMENT "Compiling ${SHADER_NAME} -> ${SHADER_NAME}.spv"
			VERBATIM
		)

		list(APPEND SPIRV_OUTPUTS ${SPIRV_OUTPUT})
	endforeach()

	add_custom_target(SPIRV_Shaders_Labyrinth ALL DEPENDS ${SPIRV_OUTPUTS})
	add_dependencies(SPIRV_Shaders_Labyrinth CopyResources_Labyrinth)
	add_dependencies(Labyrinth SPIRV_Shaders_Labyrinth)
else()
	message(WARNING "glslc not found — shader compilation skipped. Set VULKAN_SDK environment variable.")
endif()
