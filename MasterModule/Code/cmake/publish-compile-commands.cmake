file(READ "${INPUT}" database)
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" published_database)
    if(database STREQUAL published_database)
        return()
    endif()
endif()

# Publish atomically so clangd never reads a partial database.
string(RANDOM LENGTH 16 suffix)
set(temporary "${OUTPUT}.${suffix}.tmp")
file(WRITE "${temporary}" "${database}")
file(RENAME "${temporary}" "${OUTPUT}")
