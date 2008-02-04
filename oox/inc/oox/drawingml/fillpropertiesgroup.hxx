/*************************************************************************
 *
 *  OpenOffice.org - a multi-platform office productivity suite
 *
 *  $RCSfile: fillpropertiesgroup.hxx,v $
 *
 *  $Revision: 1.1.2.2 $
 *
 *  last change: $Author: dr $ $Date: 2007/03/12 12:14:17 $
 *
 *  The Contents of this file are made available subject to
 *  the terms of GNU Lesser General Public License Version 2.1.
 *
 *
 *    GNU Lesser General Public License Version 2.1
 *    =============================================
 *    Copyright 2005 by Sun Microsystems, Inc.
 *    901 San Antonio Road, Palo Alto, CA 94303, USA
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License version 2.1, as published by the Free Software Foundation.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with this library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *    MA  02111-1307  USA
 *
 ************************************************************************/

#ifndef OOX_DRAWINGML_FILLPROPERTIESGROUP_HPP
#define OOX_DRAWINGML_FILLPROPERTIESGROUP_HPP

#ifndef _COM_SUN_STAR_DRAWING_FILLSTYLE_HPP_
#include <com/sun/star/drawing/FillStyle.hpp>
#endif

#ifndef OOX_CORE_CONTEXT_HXX
#include "oox/core/context.hxx"
#endif

namespace oox { namespace core {
	class PropertyMap;
} }

namespace oox { namespace drawingml {


// ---------------------------------------------------------------------

class FillPropertiesGroupContext : public ::oox::core::Context
{
public:
	FillPropertiesGroupContext( const oox::core::FragmentHandlerRef& xHandler, ::com::sun::star::drawing::FillStyle eFillStyle, ::oox::core::PropertyMap& rProperties ) throw();

	static ::com::sun::star::uno::Reference< ::com::sun::star::xml::sax::XFastContextHandler > StaticCreateContext( const oox::core::FragmentHandlerRef& xHandler, ::sal_Int32 Element, const ::com::sun::star::uno::Reference< ::com::sun::star::xml::sax::XFastAttributeList >& Attribs, ::oox::core::PropertyMap& rProperties )  throw (::com::sun::star::xml::sax::SAXException, ::com::sun::star::uno::RuntimeException);

protected:
	::oox::core::PropertyMap& mrProperties;
};

} }

#endif // OOX_DRAWINGML_FILLPROPERTIESGROUPCONTEXT_HPP