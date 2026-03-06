add_executable(bot-benchmark
    "${CMAKE_SOURCE_DIR}/src/tools/bot_benchmark.cpp"
)
target_include_directories(bot-benchmark PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(bot-benchmark PRIVATE Qt6::Core nenoserpent_core nenoserpent_adapter)

nenoserpent_apply_project_options(
    bot-benchmark
)
