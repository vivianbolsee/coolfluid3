
coolfluid_add_test( UTEST utest-function-parser
                    CPP   utest-function-parser.cpp
                    LIBS  coolfluid_math )


coolfluid_add_test( UTEST utest-vector-operations
                    CPP   utest-vector-operations.cpp
                    LIBS  coolfluid_math )


coolfluid_add_test( UTEST utest-math-checks
                    CPP   utest-math-checks.cpp
                    LIBS  coolfluid_math )


coolfluid_add_test( UTEST utest-matrix-inverter
                    CPP   utest-matrix-inverter.cpp
                    LIBS  coolfluid_math )

# performance test
add_definitions( -DNDEBUG -DEIGEN_NO_DEBUG )
coolfluid_add_test( PTEST ptest-eigen-vs-matrixt
                    CPP   ptest-eigen-vs-matrixt.cpp
                    LIBS  coolfluid_math )


coolfluid_add_test( UTEST utest-math-variablesdescriptor
                    CPP   utest-math-variablesdescriptor.cpp
                    LIBS  coolfluid_math )


coolfluid_add_test( UTEST utest-math-variablemanager
                    CPP   utest-math-variablemanager.cpp
                    LIBS  coolfluid_math )

coolfluid_add_test( UTEST utest-math-integrate
                    CPP   utest-math-integrate.cpp
                    LIBS  coolfluid_math )

coolfluid_add_test( UTEST utest-math-hilbert
                    CPP   utest-math-hilbert.cpp
                    LIBS  coolfluid_math )

################################################################################


coolfluid_add_test( UTEST utest-lss-system-emptylss
                    CPP   utest-lss-system-emptylss.cpp
                    LIBS  coolfluid_math_lss coolfluid_math
                    MPI   1 )

coolfluid_add_test( UTEST utest-lss-atomic-fevbr
                    CPP   utest-lss-atomic.cpp
                    LIBS  coolfluid_math_lss coolfluid_math
                    CONDITION CF3_HAVE_TRILINOS
                    ARGUMENTS cf3.math.LSS.TrilinosFEVbrMatrix
                    MPI   2)

coolfluid_add_test( UTEST utest-lss-atomic-csr
                    CPP   utest-lss-atomic.cpp
                    LIBS  coolfluid_math_lss coolfluid_math
                    CONDITION CF3_HAVE_TRILINOS
                    ARGUMENTS cf3.math.LSS.TrilinosCrsMatrix
                    MPI   2)

coolfluid_add_test( UTEST utest-lss-distributed-matrix-febvbr
                    CPP   utest-lss-distributed-matrix.cpp utest-lss-test-matrix.hpp
                    LIBS  coolfluid_math_lss coolfluid_math
                    CONDITION CF3_HAVE_TRILINOS
                    ARGUMENTS cf3.math.LSS.TrilinosFEVbrMatrix
                    MPI   4)

coolfluid_add_test( UTEST utest-lss-distributed-matrix-csr
                    CPP   utest-lss-distributed-matrix.cpp utest-lss-test-matrix.hpp
                    LIBS  coolfluid_math_lss coolfluid_math
                    CONDITION CF3_HAVE_TRILINOS
                    ARGUMENTS cf3.math.LSS.TrilinosCrsMatrix
                    MPI   4)

################################################################################

#if( CMAKE_COMPILER_IS_GNUCC )
#    set_source_files_properties( utest-eigen-vs-matrixt.cpp
#      PROPERTIES COMPILE_FLAGS "-O2 -msse2 -msse3 -mssse3 -march=core2" )
#endif()
