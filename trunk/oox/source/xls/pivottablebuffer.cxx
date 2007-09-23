/*************************************************************************
 *
 *  OpenOffice.org - a multi-platform office productivity suite
 *
 *  $RCSfile: pivottablebuffer.cxx,v $
 *
 *  $Revision: 1.1.2.7 $
 *
 *  last change: $Author: dr $ $Date: 2007/09/05 12:31:23 $
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

#include "oox/xls/pivottablebuffer.hxx"
#include "oox/core/propertyset.hxx"
#include "oox/xls/addressconverter.hxx"
#include "oox/xls/worksheetbuffer.hxx"

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/container/XElementAccess.hpp>
#include <com/sun/star/container/XIndexAccess.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/sheet/DataPilotFieldOrientation.hpp>
#include <com/sun/star/sheet/GeneralFunction.hpp>
#include <com/sun/star/sheet/XCellRangeData.hpp>
#include <com/sun/star/sheet/XDataPilotDescriptor.hpp>
#include <com/sun/star/sheet/XDataPilotField.hpp>
#include <com/sun/star/sheet/XDataPilotTables.hpp>
#include <com/sun/star/sheet/XDataPilotTablesSupplier.hpp>
#include <com/sun/star/sheet/XSheetFilterDescriptor.hpp>
#include <com/sun/star/sheet/XSpreadsheet.hpp>
#include <com/sun/star/sheet/XSpreadsheetDocument.hpp>
#include <com/sun/star/table/CellAddress.hpp>
#include <com/sun/star/table/CellRangeAddress.hpp>
#include <com/sun/star/table/XCellRange.hpp>

using ::com::sun::star::container::XIndexAccess;
using ::com::sun::star::container::XNameAccess;
using ::com::sun::star::container::XNamed;
using ::com::sun::star::sheet::XCellRangeData;
using ::com::sun::star::sheet::XDataPilotDescriptor;
using ::com::sun::star::sheet::XDataPilotField;
using ::com::sun::star::sheet::XDataPilotTables;
using ::com::sun::star::sheet::XDataPilotTablesSupplier;
using ::com::sun::star::sheet::XSheetFilterDescriptor;
using ::com::sun::star::sheet::XSpreadsheet;
using ::com::sun::star::table::CellAddress;
using ::com::sun::star::table::CellRangeAddress;
using ::com::sun::star::table::XCellRange;
using ::com::sun::star::uno::Exception;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::UNO_QUERY;
using ::com::sun::star::uno::UNO_QUERY_THROW;
using ::oox::core::PropertySet;
using ::rtl::OUString;

namespace oox {
namespace xls {

// ============================================================================

PivotCacheData::PivotCacheData() :
    meSourceType( WORKSHEET ),
    mpSourceProp( static_cast<BaseSource*>(NULL) )
{
}

PivotCacheData::WorksheetSource* PivotCacheData::getWorksheetSource() const
{
    if ( meSourceType != WORKSHEET )
        return NULL;
    return static_cast<WorksheetSource*>( mpSourceProp.get() );
}

PivotCacheData::ExternalSource* PivotCacheData::getExternalSource() const
{
    if ( meSourceType != EXTERNAL )
        return NULL;
    return static_cast<ExternalSource*>( mpSourceProp.get() );
}

// ============================================================================

PivotTableField::PivotTableField() :
    meAxis( ROW ),
    mbDataField( false )
{
}

// ----------------------------------------------------------------------------

PivotTableData::PivotTableData()
{
}

// ----------------------------------------------------------------------------

PivotTableBuffer::PivotTableBuffer( const GlobalDataHelper& rGlobalData ) :
    GlobalDataHelper( rGlobalData )
{
}

const PivotCacheData* PivotTableBuffer::getPivotCache( sal_uInt32 nCacheId ) const
{
    PivotCacheMapType::const_iterator itr = maPivotCacheMap.find(nCacheId),
        itrEnd = maPivotCacheMap.end();

    if ( itr != itrEnd )
        return &itr->second;

    return NULL;
}

void PivotTableBuffer::setPivotCache( sal_uInt32 nCacheId, const PivotCacheData& aData )
{
    maPivotCacheMap.insert( PivotCacheMapType::value_type(nCacheId, aData) );
}

PivotTableData* PivotTableBuffer::getPivotTable( const OUString& aName )
{
    PivotTableMapType::iterator itr = maPivotTableMap.find(aName),
        itrEnd = maPivotTableMap.end();

    if ( itr != itrEnd )
        return &itr->second;

    return NULL;
}

void PivotTableBuffer::setPivotTable( const OUString& aName, const PivotTableData& aData )
{
    maPivotTableMap.insert( PivotTableMapType::value_type(aName, aData) );
    maCellRangeMap.addCellRange( aData.maRange );
}

bool PivotTableBuffer::isOverlapping( const CellAddress& aCellAddress ) const
{
    return maCellRangeMap.isOverlapping(aCellAddress);
}

void PivotTableBuffer::finalizeImport() const
{
    PivotTableMapType::const_iterator itr = maPivotTableMap.begin(), itrEnd = maPivotTableMap.end();
    for ( ; itr != itrEnd; ++itr )
        writePivotTable( itr->first, itr->second );
}

void PivotTableBuffer::writePivotTable( const OUString& aName, const PivotTableData& aData ) const
{
    using namespace ::com::sun::star::sheet;

    const PivotCacheData* pCache = getPivotCache( aData.mnCacheId );
    if ( !pCache )
    {
        OSL_ENSURE( false, "OoxPivotTableFragment::commit: pivot cache data not found" );
        return;
    }

    const CellRangeAddress& aRange = aData.maRange;
    Reference< XSpreadsheet > xSheet = getSheet( aRange.Sheet );
    if ( !xSheet.is() )
        return;

    try
    {
        Reference< XDataPilotTablesSupplier > xDPTSupplier( xSheet, UNO_QUERY_THROW );

        Reference< XDataPilotTables > xDPTables( xDPTSupplier->getDataPilotTables(), UNO_QUERY_THROW );
        Reference< XDataPilotDescriptor > xDPDesc( xDPTables->createDataPilotDescriptor(), UNO_QUERY_THROW );
        if ( pCache->meSourceType != PivotCacheData::WORKSHEET )
            return;

        PivotCacheData::WorksheetSource* pSrc = pCache->getWorksheetSource();
        if ( !pSrc )
            return;

        OUString sheetname = pSrc->maSheetName;
        OUString srcrange = pSrc->maSrcRange;

        CellRangeAddress aSrcRange;
        if ( !getSourceRange( pSrc->maSheetName, pSrc->maSrcRange, aSrcRange ) )
            return;

        xDPDesc->setSourceRange(aSrcRange);
        Reference< XIndexAccess > xIA = xDPDesc->getDataPilotFields();

        bool bPageAxisExists = false;

        // Go through all fields in pivot table, and register them.
        sal_Int32 nCount = ::std::min( xIA->getCount(), static_cast<sal_Int32>(aData.maFields.size()) );
        for ( sal_Int32 i = 0; i < nCount; ++i )
        {
            Reference< XDataPilotField > xField( xIA->getByIndex(i), UNO_QUERY_THROW );
            PropertySet aProp( xField );
            Reference< XNamed > xNamed( xField, UNO_QUERY_THROW );

            PivotTableField::AxisType eAxis = aData.maFields[i].meAxis;
            if ( aData.maFields[i].mbDataField )
                eAxis = PivotTableField::VALUES;

            switch ( eAxis )
            {
                case PivotTableField::COLUMN:
                    aProp.setProperty( CREATE_OUSTRING("Orientation"), DataPilotFieldOrientation_COLUMN );
                    break;
                case PivotTableField::ROW:
                    aProp.setProperty( CREATE_OUSTRING("Orientation"), DataPilotFieldOrientation_ROW );
                    break;
                case PivotTableField::PAGE:
                    bPageAxisExists = true;
                    aProp.setProperty( CREATE_OUSTRING("Orientation"), DataPilotFieldOrientation_PAGE );
                    break;
                case PivotTableField::VALUES:
                    aProp.setProperty( CREATE_OUSTRING("Orientation"), DataPilotFieldOrientation_DATA );
                    break;
                default:
                    OSL_ENSURE( false, "OoxPivotTableFragment::commit: unhandled case" );
            }
        }
        CellAddress aCell;
        aCell.Sheet = aData.maRange.Sheet;
        aCell.Column = aData.maRange.StartColumn;
        aCell.Row = aData.maRange.StartRow;
        if ( bPageAxisExists )
            aCell.Row -= 2;

        xDPTables->insertNewByName( aName, aCell, xDPDesc );
    }
    catch ( const Exception& )
    {
        OSL_ENSURE( false, "OoxPivotTableFragment::commit: exception thrown");
        return;
    }
}

bool PivotTableBuffer::getSourceRange( const OUString& aSheetName, const OUString& aRefName,
                                       CellRangeAddress& rRange ) const
{
    sal_Int32 nCount = getWorksheets().getInternalSheetCount();
    for ( sal_Int32 nSheet = 0; nSheet < nCount; ++nSheet )
    {
        Reference< XNamed > xNamed( getSheet( nSheet ), UNO_QUERY );
        if ( xNamed.is() && !aSheetName.compareTo( xNamed->getName() ) )
            return getAddressConverter().convertToCellRange(
                rRange, aRefName, static_cast< sal_Int16 >( nSheet ), true );
    }
    return false;
}

// ============================================================================

} // namespace xls
} // namespace oox
