add_executable(lob_tests test_main.cpp test_orderbook.cpp)
target_link_libraries(lob_tests PRIVATE lob)
target_compile_options(lob_tests PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME lob_tests COMMAND lob_tests)
