/*************************************************************************
 *
 *  OpenOffice.org - a multi-platform office productivity suite
 *
 *  $RCSfile: hyperlinkcontext.cxx,v $
 *
 *  $Revision: 1.1.2.4 $
 *
 *  last change: $Author: hub $ $Date: 2007/08/30 18:07:06 $
 *
 *  The Contents of this file are made available subject to
 *  the terms of GNU Lesser General Public License Version 2.1.
 *
 *
 *    GNU Lesser General Public License Version 2.1
 *    =============================================
 *    Copyright 2007 by Sun Microsystems, Inc.
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

#include <rtl/ustring.hxx>

#include <com/sun/star/xml/sax/XFastContextHandler.hpp>

#include "oox/core/relations.hxx"
#include "oox/core/namespaces.hxx"
#include "oox/drawingml/embeddedwavaudiofile.hxx"
#include "hyperlinkcontext.hxx"
#include "tokens.hxx"


using ::rtl::OUString;
using namespace ::oox::core;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::xml::sax;

namespace oox { namespace drawingml {


	HyperLinkContext::HyperLinkContext( const FragmentHandlerRef& xParent,
																			const Reference< XFastAttributeList >& xAttributes,
																			PropertyMap& aProperties)
		: Context( xParent )
			, maProperties(aProperties)
	{			
		OUString sValue = xAttributes->getOptionalValue(	NMSP_RELATIONSHIPS|XML_id );
		OSL_TRACE("OOX: URI rId %s", ::rtl::OUStringToOString (sValue, RTL_TEXTENCODING_UTF8).pData->buffer);
		if( sValue.getLength() )
		{
			RelationPtr pRelation = getHandler()->getRelations()->getRelationById( sValue );
			if( pRelation )
			{
				OUString & sHref = pRelation->msTarget;
				OSL_TRACE("OOX: URI href %s", ::rtl::OUStringToOString (sHref, RTL_TEXTENCODING_UTF8).pData->buffer);
				const rtl::OUString sURL( CREATE_OUSTRING( "URL" ) );
				maProperties[ sURL ] <<= sHref;
				sValue = xAttributes->getOptionalValue(	NMSP_RELATIONSHIPS|XML_tooltip );
				const rtl::OUString sRepresentation( CREATE_OUSTRING( "Representation" ) );
				maProperties[ sRepresentation ] <<= sValue;
				
				sValue = xAttributes->getOptionalValue(	NMSP_RELATIONSHIPS|XML_tgtFrame );
				if( sValue.getLength() )
				{
					const rtl::OUString sTargetFrame( CREATE_OUSTRING( "TargetFrame" ) );
					maProperties[ sTargetFrame ] <<= sValue;
				}
				
//				sValue = OUString( RTL_CONSTASCII_USTRINGPARAM( "" ) );
//				const rtl::OUString sUnvisitedCharStyleName( CREATE_OUSTRING( "UnvisitedCharStyleName" ) );
//				maProperties[ sUnvisitedCharStyleName ] <<= sValue;
//				const rtl::OUString sVisitedCharStyleName( CREATE_OUSTRING( "VisitedCharStyleName" ) );
//				maProperties[ sVisitedCharStyleName ] <<= sValue;
				
			}
		}
		// TODO unhandled
		// XML_invalidUrl
		// XML_history 
		// XML_highlightClick
		// XML_endSnd 
		// XML_action
	}

	HyperLinkContext::~HyperLinkContext()
	{
	}
	
	Reference< XFastContextHandler > HyperLinkContext::createFastChildContext( ::sal_Int32 aElement, 
																																						 const Reference< XFastAttributeList >& xAttribs ) 
		throw (SAXException, RuntimeException)
	{
		Reference< XFastContextHandler > xRet;
		switch( aElement )
		{
		case NMSP_DRAWINGML|XML_extLst:
			break;
		case NMSP_DRAWINGML|XML_snd:
			EmbeddedWAVAudioFile aAudio;
			getEmbeddedWAVAudioFile( getHandler(), xAttribs, aAudio );
			break;
		}
		if ( !xRet.is() )
			xRet.set( this );
		return xRet;
	}

} }