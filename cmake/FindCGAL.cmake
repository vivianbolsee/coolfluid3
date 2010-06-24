SET(CF_HAVE_CGAL 0)

FIND_PATH(CF_CGAL_INCLUDE_DIR CGAL
	DOC "Directory where the CGAL header directory is located"
	)
MARK_AS_ADVANCED(CF_CGAL_INCLUDE_DIR)

FIND_LIBRARY(CF_CGAL_LIBRARY CGAL
	DOC "The CGAL library"
	)
MARK_AS_ADVANCED(CF_CGAL_LIBRARY)

FIND_LIBRARY(CF_MPFR_LIBRARY mpfr
	DOC "The mpfr library"
	)
MARK_AS_ADVANCED(CF_MPFR_LIBRARY)

FIND_LIBRARY(CF_GMP_LIBRARY gmp
	DOC "The GMP library"
	)
MARK_AS_ADVANCED(CF_GMP_LIBRARY)

IF(CF_CGAL_INCLUDE_DIR AND CF_CGAL_LIBRARY AND CF_MPFR_LIBRARY AND CF_GMP_LIBRARY)
	SET(CF_HAVE_CGAL 1)
ENDIF(CF_CGAL_INCLUDE_DIR AND CF_CGAL_LIBRARY AND CF_MPFR_LIBRARY AND CF_GMP_LIBRARY)

