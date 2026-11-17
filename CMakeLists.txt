cmake_minimum_required(VERSION 3.16)
project(brp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # para o clangd/IDE enxergar -std=c++17
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Todos os executáveis ficam direto na pasta de build.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Pasta padrão de resultados: <raiz do projeto>/results/<método>.
add_compile_definitions(BRP_RAIZ_PROJETO="${CMAKE_SOURCE_DIR}")

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------- heurísticas
# Não dependem do Gurobi.
add_executable(min_max src/min_max.cpp)
add_executable(ri src/ri.cpp)

# --------------------------------------------------------------------- testes
# Também não dependem do Gurobi.
add_executable(bfs tests/bfs.cpp)
add_executable(modelo_lp tests/modelo_lp.cpp)
add_executable(verificar_resultados tests/verificar_resultados.cpp)

# --------------------------------------------------------------- exato (Gurobi)
# Localizado por GUROBI_HOME (variável de ambiente ou -DGUROBI_HOME=...).
# Se não for encontrado, o alvo `solver` é pulado com um aviso.
if(NOT GUROBI_HOME)
  set(GUROBI_HOME "$ENV{GUROBI_HOME}")
endif()

set(GUROBI_ENCONTRADO FALSE)
if(GUROBI_HOME)
  find_path(GUROBI_INCLUDE_DIR gurobi_c++.h
            HINTS "${GUROBI_HOME}/include" NO_DEFAULT_PATH)
  find_library(GUROBI_CXX_LIB NAMES gurobi_c++ gurobi_c++md2019 gurobi_c++md2017
               HINTS "${GUROBI_HOME}/lib" NO_DEFAULT_PATH)
  # A biblioteca C tem a versão no nome (libgurobi110.so, libgurobi120.dylib, gurobi110.lib ...).
  file(GLOB GUROBI_C_CANDIDATAS
       "${GUROBI_HOME}/lib/libgurobi[0-9]*.so"
       "${GUROBI_HOME}/lib/libgurobi[0-9]*.dylib"
       "${GUROBI_HOME}/lib/gurobi[0-9]*.lib")
  if(GUROBI_C_CANDIDATAS)
    list(SORT GUROBI_C_CANDIDATAS)
    list(GET GUROBI_C_CANDIDATAS -1 GUROBI_C_LIB)
  endif()
  if(GUROBI_INCLUDE_DIR AND GUROBI_CXX_LIB AND GUROBI_C_LIB)
    set(GUROBI_ENCONTRADO TRUE)
  endif()
endif()

if(GUROBI_ENCONTRADO)
  message(STATUS "Gurobi encontrado em ${GUROBI_HOME} (${GUROBI_C_LIB})")
  add_executable(solver src/solver.cpp)
  target_include_directories(solver PRIVATE "${GUROBI_INCLUDE_DIR}")
  target_link_libraries(solver PRIVATE "${GUROBI_CXX_LIB}" "${GUROBI_C_LIB}")
  set_target_properties(solver PROPERTIES
    BUILD_RPATH "${GUROBI_HOME}/lib"
    INSTALL_RPATH "${GUROBI_HOME}/lib")
else()
  if(GUROBI_HOME)
    message(WARNING "GUROBI_HOME=${GUROBI_HOME} não contém include/gurobi_c++.h e as bibliotecas em lib/. "
                    "O alvo `solver` NÃO será compilado.")
  else()
    message(WARNING "GUROBI_HOME não definido: o alvo `solver` (método exato) NÃO será compilado. "
                    "Os alvos min_max, ri e os testes não dependem do Gurobi.")
  endif()
endif()
