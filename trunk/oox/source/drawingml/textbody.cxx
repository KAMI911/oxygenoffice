/*************************************************************************
 *
 *  OpenOffice.org - a multi-platform office productivity suite
 *
 *  $RCSfile: textbody.cxx,v $
 *
 *  $Revision: 1.1.2.7 $
 *
 *  last change: $Author: hub $ $Date: 2007/08/20 23:40:51 $
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

#include <algorithm>
#include <boost/bind.hpp>

#include "oox/drawingml/textbody.hxx"

using ::rtl::OUString;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::text;
using namespace ::com::sun::star::frame;

namespace oox { namespace drawingml {


	TextBody::TextBody()
		: mpTextListStyle( new TextListStyle() )
	{
	}


	TextBody::~TextBody()
	{
	}


	
	void TextBody::insertAt( const Reference < XText > & xText,
													 const Reference < XTextCursor > & xAt, const Reference < XModel > &xModel )
	{
		std::vector< TextParagraphPtr >::iterator begin( maParagraphs.begin() );
		std::vector< TextParagraphPtr >::iterator end( maParagraphs.end() );
		// apparently if there is no paragraph, it crashes. this is sort of the 
		// expected behavior.
		if( begin != end )
		{
			std::for_each( begin, end, 
										 boost::bind( &TextParagraph::insertAt, _1,
																	xText, xAt, xModel,
																	// determine whether it is the first paragraph of not
																	boost::bind( std::equal_to<TextParagraphPtr>(), _1, 
																							 *maParagraphs.begin() ) ) );
		}
	}


} }
