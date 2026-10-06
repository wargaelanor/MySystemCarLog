set(srcs)
set(include_dirs)

# IDF 4+ newlib does not provide strverscmp, so we always build it ourselves
list(APPEND srcs "src/strverscmp.c")
list(APPEND include_dirs "src")

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       WHOLE_ARCHIVE)
