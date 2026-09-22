# Unit tests

enable_testing()

add_custom_target(all_tests)

function(add_unit_test test_name lib)
  add_executable(${test_name}
    ${test_name}.cpp
  )

  target_link_libraries(${test_name}
    PRIVATE
      ${lib}
  )

  add_test(
    NAME ${test_name}
    COMMAND ${test_name}
  )

  add_dependencies(all_tests ${test_name})
endfunction()

add_unit_test(AudioFifoTest libmitmegy)
add_unit_test(ProtocolMessageTest protocol)
