function(add_import_library libname)
  add_library(libname SHARED IMPORTED)
  if(NOT TARGET libname)
    message(FATAL "unable to find library ${libname}")
  endif()
endfunction()
