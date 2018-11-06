include(FetchContent)

FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY  "https://github.com/google/flatbuffers"
  # The fix for https://github.com/google/flatbuffers/issues/4741 is not
  # released yet, so I picked a recent commit that passed CI.
  # TODO(anoyes): use the next release when that comes out.
  GIT_TAG 73304367131766aa8dca9c495f75c802bc7991ec
  )

FetchContent_GetProperties(flatbuffers)
if(NOT flatbuffers_POPULATED)
  FetchContent_Populate(flatbuffers)
  add_subdirectory(${flatbuffers_SOURCE_DIR} ${flatbuffers_BINARY_DIR})
endif()
