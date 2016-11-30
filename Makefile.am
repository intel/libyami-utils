#  Copyright 2016 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AUTOMAKE_OPTIONS = foreign

# Extra clean files so that maintainer-clean removes *everything*
DISTCLEANFILES = \
	aclocal.m4 autom4te.cache compile config.guess config.sub \
	configure config.h.in config.h.in~ depcomp install-sh ltmain.sh \
	Makefile.in missing

SUBDIRS = examples tests testscripts doc
