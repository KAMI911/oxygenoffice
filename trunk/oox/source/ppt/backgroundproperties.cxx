/*************************************************************************
 *
 *  OpenOffice.org - a multi-platform office productivity suite
 *
 *  $RCSfile: backgroundproperties.cxx,v $
 *
 *  $Revision: 1.1.2.1 $
 *
 *  last change: $Author: cl $ $Date: 2007/01/24 14:14:36 $
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

#include <comphelper/propertysethelper.hxx>

#include <oox/ppt/backgroundproperties.hxx>
#include "oox/drawingml/fillpropertiesgroup.hxx"
#include "oox/drawingml/drawingmltypes.hxx"
#include "oox/core/propertymap.hxx"
#include "oox/core/namespaces.hxx"
#include "tokens.hxx"

using ::rtl::OUString;
using ::com::sun::star::beans::NamedValue;
using ::com::sun::star::beans::PropertyValue;
using ::com::sun::star::beans::XPropertySet;
using namespace ::oox::core;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::xml::sax;

namespace oox { namespace drawingml {
// ---------------------------------------------------------------------

BackgroundPropertiesContext::BackgroundPropertiesContext( const FragmentHandlerRef& xHandler, PropertyMap& rProperties ) throw()
: Context( xHandler )
, mrProperties( rProperties )
{
}

void BackgroundPropertiesContext::endFastElement( sal_Int32 ) throw (SAXException, RuntimeException)
{
	if( !maBackgroundProperties.empty() ) try
	{
		static const OUString sBackground( RTL_CONSTASCII_USTRINGPARAM( "Background" ) );
		mrProperties[ sBackground ] <<= maBackgroundProperties.makePropertySet();
	}
	catch( Exception )
	{
	}
}

Reference< XFastContextHandler > BackgroundPropertiesContext::createFastChildContext( sal_Int32 aElementToken, const Reference< XFastAttributeList >& xAttribs ) throw (SAXException, RuntimeException)
{
	Reference< XFastContextHandler > xRet;

	switch( aElementToken )
	{
	case NMSP_PPT|XML_fill:	// a:CT_FillEffect
		break;
	}

	// FillPropertiesGroup
	if( !xRet.is() )
		xRet = FillPropertiesGroupContext::StaticCreateContext( getHandler(), aElementToken, xAttribs, maBackgroundProperties );

	return xRet;
}

} }