# sync_examples.cmake — Copy only changed files, log each action.
# Usage: cmake -DSRC_DIR=... -DDST_DIR=... -P sync_examples.cmake

file(GLOB_RECURSE _src_files RELATIVE "${SRC_DIR}" "${SRC_DIR}/*")

set(_updated 0)
set(_skipped 0)
set(_new 0)

foreach(_file IN LISTS _src_files)
    set(_src "${SRC_DIR}/${_file}")
    set(_dst "${DST_DIR}/${_file}")

    if(EXISTS "${_dst}")
        file(MD5 "${_src}" _src_hash)
        file(MD5 "${_dst}" _dst_hash)
        if(_src_hash STREQUAL _dst_hash)
            math(EXPR _skipped "${_skipped} + 1")
            continue()
        endif()
        message(STATUS "  Updated: ${_file}")
        math(EXPR _updated "${_updated} + 1")
    else()
        message(STATUS "  New:     ${_file}")
        math(EXPR _new "${_new} + 1")
    endif()

    get_filename_component(_dst_dir "${_dst}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dst_dir}")
    file(COPY_FILE "${_src}" "${_dst}")
endforeach()

message(STATUS "Examples sync: ${_new} new, ${_updated} updated, ${_skipped} unchanged")
