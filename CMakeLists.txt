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

# ------------------------------------------------------------ exato por busca
# A* (busca informada); também não depende de solver de PLIM.
add_executable(astar src/astar.cpp)

# --------------------------------------------------------------------- testes
# Também não dependem do Gurobi.
add_executable(bfs tests/bfs.cpp)
add_executable(modelo_lp tests/modelo_lp.cpp)
add_executable(verificar_resultados tests/verificar_resultados.cpp)

# ------------------------------------------------------------------- exato
# O alvo `solver` é compilado com todos os solvers de PLIM encontrados:
#   Gurobi  GUROBI_HOME (variável de ambiente ou -DGUROBI_HOME=...)
#   CPLEX   CPLEX_HOME  (variável de ambiente ou -DCPLEX_HOME=...): a pasta "cplex" do
#           CPLEX Optimization Studio, ou a raiz do Studio
#   HiGHS   find_package(highs); use -DCMAKE_PREFIX_PATH=... se estiver fora do padrão
# Se nenhum for encontrado, o alvo é pulado com um aviso.
set(SOLVERS_ENCONTRADOS "")
set(SOLVER_DEFINICOES "")
set(SOLVER_INCLUDES "")
set(SOLVER_BIBLIOTECAS "")
set(SOLVER_RPATH "")

# ---- Gurobi
if(NOT GUROBI_HOME)
  set(GUROBI_HOME "$ENV{GUROBI_HOME}")
endif()
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
    message(STATUS "Gurobi encontrado em ${GUROBI_HOME} (${GUROBI_C_LIB})")
    list(APPEND SOLVERS_ENCONTRADOS gurobi)
    list(APPEND SOLVER_DEFINICOES BRP2_COM_GUROBI)
    list(APPEND SOLVER_INCLUDES "${GUROBI_INCLUDE_DIR}")
    list(APPEND SOLVER_BIBLIOTECAS "${GUROBI_CXX_LIB}" "${GUROBI_C_LIB}")
    list(APPEND SOLVER_RPATH "${GUROBI_HOME}/lib")
  else()
    message(WARNING "GUROBI_HOME=${GUROBI_HOME} não contém include/gurobi_c++.h e as bibliotecas em lib/.")
  endif()
endif()

# ---- CPLEX (biblioteca C)
if(NOT CPLEX_HOME)
  set(CPLEX_HOME "$ENV{CPLEX_HOME}")
endif()
if(CPLEX_HOME)
  find_path(CPLEX_INCLUDE_DIR ilcplex/cplex.h
            HINTS "${CPLEX_HOME}/include" "${CPLEX_HOME}/cplex/include" NO_DEFAULT_PATH)
  # A biblioteca fica em lib/<arquitetura>/static_pic (ex.: arm64_osx, x86-64_linux).
  file(GLOB CPLEX_CANDIDATAS
       "${CPLEX_HOME}/lib/*/static_pic/libcplex.a"
       "${CPLEX_HOME}/cplex/lib/*/static_pic/libcplex.a"
       "${CPLEX_HOME}/lib/*/*/cplex*.lib"
       "${CPLEX_HOME}/cplex/lib/*/*/cplex*.lib")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    list(FILTER CPLEX_CANDIDATAS EXCLUDE REGEX "x86-64")
  else()
    list(FILTER CPLEX_CANDIDATAS EXCLUDE REGEX "arm64")
  endif()
  if(CPLEX_CANDIDATAS)
    list(GET CPLEX_CANDIDATAS 0 CPLEX_LIB)
  endif()
  if(CPLEX_INCLUDE_DIR AND CPLEX_LIB)
    message(STATUS "CPLEX encontrado em ${CPLEX_HOME} (${CPLEX_LIB})")
    find_package(Threads REQUIRED)
    list(APPEND SOLVERS_ENCONTRADOS cplex)
    list(APPEND SOLVER_DEFINICOES BRP2_COM_CPLEX)
    list(APPEND SOLVER_INCLUDES "${CPLEX_INCLUDE_DIR}")
    list(APPEND SOLVER_BIBLIOTECAS "${CPLEX_LIB}" Threads::Threads ${CMAKE_DL_LIBS})
  else()
    message(WARNING "CPLEX_HOME=${CPLEX_HOME} não contém include/ilcplex/cplex.h e lib/<arquitetura>/static_pic/libcplex.a.")
  endif()
endif()

# ---- HiGHS
find_package(highs CONFIG QUIET)
if(TARGET highs::highs)
  message(STATUS "HiGHS encontrado (${highs_DIR})")
  list(APPEND SOLVERS_ENCONTRADOS highs)
  list(APPEND SOLVER_DEFINICOES BRP2_COM_HIGHS)
  list(APPEND SOLVER_BIBLIOTECAS highs::highs)
endif()

if(SOLVERS_ENCONTRADOS)
  message(STATUS "Alvo `solver` com: ${SOLVERS_ENCONTRADOS}")
  add_executable(solver src/solver.cpp)
  target_compile_definitions(solver PRIVATE ${SOLVER_DEFINICOES})
  target_include_directories(solver PRIVATE ${SOLVER_INCLUDES})
  target_link_libraries(solver PRIVATE ${SOLVER_BIBLIOTECAS})
  if(SOLVER_RPATH)
    set_target_properties(solver PROPERTIES BUILD_RPATH "${SOLVER_RPATH}" INSTALL_RPATH "${SOLVER_RPATH}")
  endif()
else()
  message(WARNING "Nenhum solver de PLIM encontrado (Gurobi, CPLEX ou HiGHS): o alvo `solver` "
                  "(método exato) NÃO será compilado. Os alvos min_max, ri e os testes não dependem deles.")
endif()
