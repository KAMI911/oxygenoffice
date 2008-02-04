/*************************************************************************
 *
 *  OpenOffice.org - a multi-platform office productivity suite
 *
 *  $RCSfile: worksheetbuffer.cxx,v $
 *
 *  $Revision: 1.1.2.1 $
 *
 *  last change: $Author: dr $ $Date: 2007/09/05 12:31:23 $
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

#include "oox/xls/worksheetbuffer.hxx"
#include <rtl/ustrbuf.hxx>
#include <com/sun/star/container/XIndexAccess.hpp>
#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/i18n/XCharacterClassification.hpp>
#include <com/sun/star/i18n/KParseTokens.hpp>
#include <com/sun/star/i18n/KParseType.hpp>
#include <com/sun/star/sheet/XSpreadsheetDocument.hpp>
#include <com/sun/star/sheet/XSpreadsheet.hpp>
#include <comphelper/processfactory.hxx>
#include "oox/core/containerhelper.hxx"
#include "oox/xls/biffinputstream.hxx"
#include "oox/xls/contexthelper.hxx"

using ::rtl::OUString;
using ::rtl::OUStringBuffer;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Exception;
using ::com::sun::star::uno::UNO_QUERY;
using ::com::sun::star::uno::UNO_QUERY_THROW;
using ::com::sun::star::container::XIndexAccess;
using ::com::sun::star::container::XNameAccess;
using ::com::sun::star::container::XNamed;
using ::com::sun::star::lang::Locale;
using ::com::sun::star::lang::XMultiServiceFactory;
using ::com::sun::star::i18n::ParseResult;
using ::com::sun::star::sheet::XSpreadsheetDocument;
using ::com::sun::star::sheet::XSpreadsheets;
using ::com::sun::star::sheet::XSpreadsheet;
using ::oox::core::AttributeList;
using ::oox::core::ContainerHelper;

namespace oox {
namespace xls {

// ============================================================================

const sal_uInt16 BIFF_BOUNDSHEET_HIDDEN     = 0x0001;
const sal_uInt16 BIFF_BOUNDSHEET_VERYHIDDEN = 0x0002;
const sal_uInt16 BIFF_BOUNDSHEET_ANY_HIDDEN = BIFF_BOUNDSHEET_HIDDEN | BIFF_BOUNDSHEET_VERYHIDDEN;

// ============================================================================

OoxSheetInfo::OoxSheetInfo() :
    mbVisible( true )
{
}

// ============================================================================

WorksheetBuffer::WorksheetBuffer( const GlobalDataHelper& rGlobalData ) :
    GlobalDataHelper( rGlobalData )
{
    // character classification service for conversion to valid sheet names
    Reference< XMultiServiceFactory > xFactory = ::comphelper::getProcessServiceFactory();
    mxCharClass.set( xFactory->createInstance( CREATE_OUSTRING( "com.sun.star.i18n.CharacterClassification" ) ), UNO_QUERY );
    OSL_ENSURE( mxCharClass.is(), "WorksheetBuffer::WorksheetBuffer - no character classification service" );
}

void WorksheetBuffer::importSheet( const AttributeList& rAttribs )
{
    OoxSheetInfo aSheetInfo;
    aSheetInfo.maId = rAttribs.getString( R_TOKEN( id ) );
    aSheetInfo.maName = rAttribs.getString( XML_name );
    insertSheet( aSheetInfo );
}

void WorksheetBuffer::importBoundSheet( BiffInputStream& rStrm )
{
    sal_uInt16 nFlags = 0;
    if( getBiff() >= BIFF5 )
    {
        rStrm.ignore( 4 );
        rStrm >> nFlags;
    }

    OoxSheetInfo aSheetInfo;
    aSheetInfo.maName = (getBiff() == BIFF8) ?
        rStrm.readUniString( rStrm.readuInt8() ) :
        rStrm.readByteString( false, getTextEncoding() );
    aSheetInfo.mbVisible = !getFlag( nFlags, BIFF_BOUNDSHEET_ANY_HIDDEN );
    insertSheet( aSheetInfo );
}

sal_Int32 WorksheetBuffer::insertExternalSheet( const OUString& rTargetUrl, const OUString& rSheetName )
{
    // try to find existing external sheet (needed for BIFF4W and BIFF5)
    ExternalSheetName aExtSheet( rTargetUrl, rSheetName );
    ExternalSheetMap::iterator aIt = maExternalSheets.find( aExtSheet );
    if( aIt != maExternalSheets.end() )
        return aIt->second;

    // create a new external sheet
    sal_Int16& rnSheet = maExternalSheets[ aExtSheet ];
    rnSheet = getTotalSheetCount();
    OUString aSheetName = OUStringBuffer( rTargetUrl ).append( sal_Unicode( '#' ) ).append( rSheetName ).makeStringAndClear();
    insertSheet( aSheetName, rnSheet, false );
    return rnSheet;
}

sal_Int32 WorksheetBuffer::getInternalSheetCount() const
{
    return static_cast< sal_Int32 >( maSheetInfos.size() );
}

OUString WorksheetBuffer::getSheetRelId( sal_Int32 nSheet ) const
{
    OUString aRelId;
    if( const OoxSheetInfo* pInfo = getSheetInfo( nSheet ) )
        aRelId = pInfo->maId;
    return aRelId;
}

OUString WorksheetBuffer::getFinalSheetName( sal_Int32 nSheet ) const
{
    OUString aName;
    if( const OoxSheetInfo* pInfo = getSheetInfo( nSheet ) )
        aName = pInfo->maFinalName;
    return aName;
}

OUString WorksheetBuffer::getFinalSheetName( const OUString& rName ) const
{
    for( SheetInfoVec::const_iterator aIt = maSheetInfos.begin(), aEnd = maSheetInfos.end(); aIt != aEnd; ++aIt )
        // TODO: handle encoded characters
        if( aIt->maName.equalsIgnoreAsciiCase( rName ) )
            return aIt->maFinalName;
    return OUString();
}

sal_Int32 WorksheetBuffer::getFinalSheetIndex( const OUString& rName ) const
{
    for( SheetInfoVec::const_iterator aIt = maSheetInfos.begin(), aEnd = maSheetInfos.end(); aIt != aEnd; ++aIt )
        // TODO: handle encoded characters
        if( aIt->maName.equalsIgnoreAsciiCase( rName ) )
            return static_cast< sal_Int32 >( aIt - maSheetInfos.begin() );
    return -1;
}

// private --------------------------------------------------------------------

sal_Int16 WorksheetBuffer::getTotalSheetCount() const
{
    try
    {
        Reference< XIndexAccess > xSheetsIA( getDocument()->getSheets(), UNO_QUERY_THROW );
        return static_cast< sal_Int16 >( xSheetsIA->getCount() );
    }
    catch( Exception& )
    {
        OSL_ENSURE( false, "WorksheetBuffer::getTotalSheetCount - cannot get sheet count" );
    }
    return 1;
}

const OoxSheetInfo* WorksheetBuffer::getSheetInfo( sal_Int32 nSheet ) const
{
    return ((0 <= nSheet) && (static_cast< size_t >( nSheet ) < maSheetInfos.size())) ?
        &maSheetInfos[ static_cast< size_t >( nSheet ) ] : 0;
}

OUString WorksheetBuffer::convertToValidSheetName( const OUString& rName, sal_Unicode cReplaceChar ) const
{
    if( !mxCharClass.is() ) return rName;

    using namespace ::com::sun::star::i18n::KParseTokens;
    using namespace ::com::sun::star::i18n::KParseType;

    OUStringBuffer aFinalName( rName );
    Locale aLocale( CREATE_OUSTRING( "en" ), CREATE_OUSTRING( "US" ), OUString() );
    sal_Int32 nStartFlags = ANY_LETTER_OR_NUMBER | ASC_UNDERSCORE;
    sal_Int32 nContFlags = nStartFlags;
    OUString aStartChars;
    OUString aContChars( sal_Unicode( ' ' ) );
    sal_Int32 nStartPos = 0;
    while( nStartPos < aFinalName.getLength() )
    {
        ParseResult aRes = mxCharClass->parsePredefinedToken(
            IDENTNAME, rName, nStartPos, aLocale, nStartFlags, aStartChars, nContFlags, aContChars );
        if( aRes.EndPos < aFinalName.getLength() )
        {
            aFinalName.setCharAt( aRes.EndPos, cReplaceChar );
            nStartFlags = nContFlags;
            aStartChars = aContChars;
        }
        nStartPos = aRes.EndPos + 1;
    }
    return aFinalName.makeStringAndClear();
}

OUString WorksheetBuffer::insertSheet( const OUString& rName, sal_Int16 nSheet, bool /*bVisible*/ )
{
    OUString aFinalName = (rName.getLength() == 0) ? CREATE_OUSTRING( "Sheet" ) : convertToValidSheetName( rName, '_' );
    try
    {
        Reference< XSpreadsheets > xSheets( getDocument()->getSheets(), UNO_QUERY_THROW );
        Reference< XIndexAccess > xSheetsIA( xSheets, UNO_QUERY_THROW );
        Reference< XNameAccess > xSheetsNA( xSheets, UNO_QUERY_THROW );
        if( nSheet < xSheetsIA->getCount() )
        {
            // existing sheet - try to rename
            Reference< XNamed > xSheetName( xSheetsIA->getByIndex( nSheet ), UNO_QUERY_THROW );
            if( xSheetName->getName() != aFinalName )
            {
                aFinalName = ContainerHelper::getUnusedName( xSheetsNA, aFinalName, ' ' );
                xSheetName->setName( aFinalName );
            }
        }
        else
        {
            // new sheet - insert with unused name
            aFinalName = ContainerHelper::getUnusedName( xSheetsNA, aFinalName, ' ' );
            xSheets->insertNewByName( aFinalName, nSheet );
        }
    }
    catch( Exception& )
    {
        OSL_ENSURE( false, "WorksheetBuffer::insertSheet - cannot insert or rename worksheet" );
    }
    return aFinalName;
}

void WorksheetBuffer::insertSheet( const OoxSheetInfo& rSheetInfo )
{
    sal_Int16 nSheet = static_cast< sal_Int16 >( maSheetInfos.size() );
    maSheetInfos.push_back( rSheetInfo );
    maSheetInfos.back().maFinalName = insertSheet( rSheetInfo.maName, nSheet, rSheetInfo.mbVisible );
}

// ============================================================================

} // namespace xls
} // namespace oox
