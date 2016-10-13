# -*- shell-script -*-
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
# Copyright (c) 2009-2016 Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2016      Los Alamos National Security, LLC. All rights
#                         reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# ORTE_CHECK_BBQUE(prefix, [action-if-found], [action-if-not-found])
# --------------------------------------------------------
AC_DEFUN([ORTE_CHECK_BBQUE],[
    if test -z "$orte_check_bbque_happy" ; then
	AC_ARG_WITH([bbque],
           [AC_HELP_STRING([--with-bbque],
                           [Build BarbequeRTRM resource allocator component (default: yes)])])

	if test "$with_bbque" = "yes" ; then
            orte_check_bbque_happy="yes"
        else
            orte_check_bbque_happy="no"
        fi

        AS_IF([test "$orte_check_bbque_happy" = "yes"],
              [AC_CHECK_FUNC([fork],
                             [orte_check_bbque_happy="yes"],
                             [orte_check_bbque_happy="no"])])

        AS_IF([test "$orte_check_bbque_happy" = "yes"],
              [AC_CHECK_FUNC([execve],
                             [orte_check_bbque_happy="yes"],
                             [orte_check_bbque_happy="no"])])

        AS_IF([test "$orte_check_bbque_happy" = "yes"],
              [AC_CHECK_FUNC([setpgid],
                             [orte_check_bbque_happy="yes"],
                             [orte_check_bbque_happy="no"])])

        OPAL_SUMMARY_ADD([[Resource Managers]],[[BarbequeRTRM]],[$1],[$orte_check_bbque_happy])
    fi

    AS_IF([test "$orte_check_bbque_happy" = "yes"],
          [$2],
          [$3])
])
