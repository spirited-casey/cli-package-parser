# CMakeList.txt : CMake project for cli-package-parser, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (cli-package-parser  "main.cxx"  )

#target_include_directories(cli-package-parser PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(cli-package-parser PRIVATE Boost::program_options)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET cli-package-parser PROPERTY CXX_STANDARD 17)
endif()

# TODO: Add tests and install targets if needed.
