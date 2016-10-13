#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2016      Politecnico di Milano. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

AM_CPPFLAGS = $(ras_bbque_CPPFLAGS)

dist_ortedata_DATA = help-ras-bbque.txt

sources = \
        ras_bbque.h \
        ras_bbque_component.c \
        ras_bbque_module.c


# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orte_ras_bbque_DSO
lib =
lib_sources =
component = mca_ras_bbque.la
component_sources = $(sources)
else
lib = libmca_ras_bbque.la
lib_sources = $(sources)
component =
component_sources =
endif

mcacomponentdir = $(ortelibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_ras_bbque_la_SOURCES = $(component_sources)
mca_ras_bbque_la_LDFLAGS = -module -avoid-version $(ras_bbque_LDFLAGS)
mca_ras_bbque_la_LIBADD = $(ras_bbque_LIBS)

noinst_LTLIBRARIES = $(lib)
libmca_ras_bbque_la_SOURCES = $(lib_sources)
libmca_ras_bbque_la_LDFLAGS = -module -avoid-version $(ras_bbque_LDFLAGS)
libmca_ras_bbque_la_LIBADD = $(ras_bbque_LIBS)
