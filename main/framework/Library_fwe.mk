#*************************************************************************
#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
# 
# Copyright 2000, 2011 Oracle and/or its affiliates.
#
# OpenOffice.org - a multi-platform office productivity suite
#
# This file is part of OpenOffice.org.
#
# OpenOffice.org is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 3
# only, as published by the Free Software Foundation.
#
# OpenOffice.org is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License version 3 for more details
# (a copy is included in the LICENSE file that accompanied this code).
#
# You should have received a copy of the GNU Lesser General Public License
# version 3 along with OpenOffice.org.  If not, see
# <http://www.openoffice.org/license.html>
# for a copy of the LGPLv3 License.
#
#*************************************************************************

$(eval $(call gb_Library_Library,fwe))

$(eval $(call gb_Library_set_include,fwe,\
	-I$(SRCDIR)/framework/inc/pch \
	-I$(SRCDIR)/framework/source/inc \
	-I$(SRCDIR)/framework/inc \
	-I$(WORKDIR)/inc/framework/ \
	$$(INCLUDE) \
	-I$(OUTDIR)/inc/framework \
	-I$(OUTDIR)/inc/offuh \
))

$(eval $(call gb_Library_set_defs,fwe,\
	$$(DEFS) \
	-DFWE_DLLIMPLEMENTATION\
))

$(eval $(call gb_Library_add_linked_libs,fwe,\
	comphelper \
	cppu \
	cppuhelper \
	fwi \
	sal \
	stl \
	svl \
	svt \
	tl \
	utl \
	vcl \
	vos3 \
	$(gb_STDLIBS) \
))

$(eval $(call gb_Library_add_exception_objects,fwe,\
	framework/source/fwe/classes/actiontriggercontainer \
	framework/source/fwe/classes/actiontriggerpropertyset \
	framework/source/fwe/classes/actiontriggerseparatorpropertyset \
	framework/source/fwe/classes/addonmenu \
	framework/source/fwe/classes/addonsoptions \
	framework/source/fwe/classes/bmkmenu \
	framework/source/fwe/classes/framelistanalyzer \
	framework/source/fwe/classes/fwkresid \
	framework/source/fwe/classes/imagewrapper \
	framework/source/fwe/classes/menuextensionsupplier \
	framework/source/fwe/classes/rootactiontriggercontainer \
	framework/source/fwe/classes/sfxhelperfunctions \
	framework/source/fwe/dispatch/interaction \
	framework/source/fwe/helper/acceleratorinfo \
	framework/source/fwe/helper/actiontriggerhelper \
	framework/source/fwe/helper/configimporter \
	framework/source/fwe/helper/imageproducer \
	framework/source/fwe/helper/propertysetcontainer \
	framework/source/fwe/helper/titlehelper \
	framework/source/fwe/helper/documentundoguard \
	framework/source/fwe/helper/undomanagerhelper \
	framework/source/fwe/interaction/preventduplicateinteraction \
	framework/source/fwe/xml/eventsconfiguration \
	framework/source/fwe/xml/eventsdocumenthandler \
	framework/source/fwe/xml/menuconfiguration \
	framework/source/fwe/xml/menudocumenthandler \
	framework/source/fwe/xml/saxnamespacefilter \
	framework/source/fwe/xml/statusbarconfiguration \
	framework/source/fwe/xml/statusbardocumenthandler \
	framework/source/fwe/xml/toolboxconfiguration \
	framework/source/fwe/xml/toolboxdocumenthandler \
	framework/source/fwe/xml/xmlnamespaces \
))

#todo: ImageListDescriptor can't be exported completely without exporting everything
ifeq ($(OS),LINUX)
$(eval $(call gb_Library_set_cxxflags,fwe,$$(filter-out -fvisibility=hidden,$$(CXXFLAGS))))
endif
ifeq ($(OS),FREEBSD)
$(eval $(call gb_Library_set_cxxflags,fwe,$$(filter-out -fvisibility=hidden,$$(CXXFLAGS))))
endif

# vim: set noet sw=4 ts=4:
