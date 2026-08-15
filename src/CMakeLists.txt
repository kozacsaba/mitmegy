# app

add_executable(wasapi_test
    main.cpp
)

target_compile_features(wasapi_test PRIVATE cxx_std_20)

target_link_libraries(wasapi_test PRIVATE
    ole32
)
