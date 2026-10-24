get_filename_component(ProjectId ${CMAKE_CURRENT_SOURCE_DIR} NAME)
add_server_component(${ProjectId})
link_rust_library(${ProjectId} omp-textlabels)
