/*************************************************************************
 *
 *  OpenOffice.org - a multi-platform office productivity suite
 *
 *  $RCSfile: slidemastertextstylescontext.cxx,v $
 *
 *  $Revision: 1.1.2.2 $
 *
 *  last change: $Author: hub $ $Date: 2007/06/19 19:50:46 $
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

#include "oox/drawingml/textliststyle.hxx"
#include "oox/drawingml/textliststylecontext.hxx"
#include "oox/ppt/slidemastertextstylescontext.hxx"
#include "oox/core/namespaces.hxx"
#include "tokens.hxx"

using rtl::OUString;
using namespace ::oox::core;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::xml::sax;

namespace oox { namespace ppt {

SlideMasterTextStylesContext::SlideMasterTextStylesContext( const ::oox::core::FragmentHandlerRef& xHandler, SlidePersistPtr pSlidePersistPtr )
: Context( xHandler )
, mpSlidePersistPtr( pSlidePersistPtr )
{
}

SlideMasterTextStylesContext::~SlideMasterTextStylesContext()
{
}

Reference< XFastContextHandler > SlideMasterTextStylesContext::createFastChildContext( sal_Int32 aElementToken, const Reference< XFastAttributeList >& /* xAttribs */ ) throw (SAXException, RuntimeException)
{
	oox::drawingml::TextListStylePtr aTextListStylePtr;
	Reference< XFastContextHandler > xRet;
	switch( aElementToken )
	{
		case NMSP_PPT|XML_titleStyle:
		{		
			aTextListStylePtr = mpSlidePersistPtr->getMasterTextStyle();
			break;
		}
		case NMSP_PPT|XML_bodyStyle:
		{
			aTextListStylePtr = mpSlidePersistPtr->getBodyTextStyle();
			break;
		}
		case NMSP_PPT|XML_notesStyle:
		{
			aTextListStylePtr = mpSlidePersistPtr->getNotesTextStyle();
			break;
		}
		case NMSP_PPT|XML_otherStyle:		
		{
			aTextListStylePtr = mpSlidePersistPtr->getOtherTextStyle();
			break;
		}
	}
	if ( aTextListStylePtr )
		xRet.set( new oox::drawingml::TextListStyleContext( getHandler(), aTextListStylePtr ) );
	if( !xRet.is() )
		xRet.set( this );

	return xRet;
}

} }
