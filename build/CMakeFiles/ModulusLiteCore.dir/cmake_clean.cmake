file(REMOVE_RECURSE
  "libModulusLiteCore.a"
  "libModulusLiteCore.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/ModulusLiteCore.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
