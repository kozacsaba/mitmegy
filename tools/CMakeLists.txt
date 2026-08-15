# Tools

add_custom_target(all_tools)

function(add_tool tool_name)
  add_executable(${tool_name}
    ${tool_name}.cpp
  )

  target_link_libraries(${tool_name}
    PRIVATE
      libmitmegy
  )

  add_dependencies(all_tools ${tool_name})
endfunction()

add_tool(bridge)
