/*************************************************************************
 *
 *  OpenOffice.org - a multi-platform office productivity suite
 *
 *  $RCSfile: biffformatdetector.hxx,v $
 *
 *  $Revision: 1.1.2.7 $
 *
 *  last change: $Author: dr $ $Date: 2007/08/16 16:25:05 $
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

#ifndef OOX_XLS_BIFFFORMATDETECTOR_HXX
#define OOX_XLS_BIFFFORMATDETECTOR_HXX

#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/document/XExtendedFilterDetection.hpp>
#include <cppuhelper/implbase2.hxx>
#include "oox/core/storagebase.hxx"
#include "oox/xls/biffhelper.hxx"

namespace com { namespace sun { namespace star {
    namespace lang { class XMultiServiceFactory; }
    namespace beans { struct PropertyValue; }
} } }

namespace oox { namespace core {
    class BinaryInputStream;
} }

namespace oox {
namespace xls {

// ============================================================================

/** Service for BIFF file format detection. */
class BiffFormatDetector : public ::cppu::WeakImplHelper2<
            ::com::sun::star::lang::XServiceInfo,
            ::com::sun::star::document::XExtendedFilterDetection >
{
public:
    explicit            BiffFormatDetector(
                            const ::com::sun::star::uno::Reference< ::com::sun::star::lang::XMultiServiceFactory >& rxFactory );

    virtual             ~BiffFormatDetector();

    /** Detects the BIFF version of the passed stream. */
    static BiffType     detectStreamBiffVersion(
                            ::oox::core::BinaryInputStream& rInStream );

    /** Detects the BIFF version and workbook stream name of the passed storage. */
    BiffType            detectStorageBiffVersion(
                            ::rtl::OUString& orWorkbookStreamName,
                            ::oox::core::StorageRef xStorage ) const;

    // com.sun.star.lang.XServiceInfo interface -------------------------------

    virtual ::rtl::OUString SAL_CALL
                        getImplementationName() throw( ::com::sun::star::uno::RuntimeException );

    virtual sal_Bool SAL_CALL
                        supportsService( const ::rtl::OUString& rService )
                            throw( ::com::sun::star::uno::RuntimeException );

    virtual ::com::sun::star::uno::Sequence< ::rtl::OUString > SAL_CALL
                        getSupportedServiceNames() throw( ::com::sun::star::uno::RuntimeException );

    // com.sun.star.document.XExtendedFilterDetect interface ------------------

    virtual ::rtl::OUString SAL_CALL
                        detect( ::com::sun::star::uno::Sequence< ::com::sun::star::beans::PropertyValue >& orDescriptor )
                            throw( ::com::sun::star::uno::RuntimeException );

private:
    ::com::sun::star::uno::Reference< ::com::sun::star::lang::XMultiServiceFactory >
                        mxFactory;
    const ::rtl::OUString maWorkbookName;
    const ::rtl::OUString maBookName;
};

// ============================================================================

} // namespace xls
} // namespace oox

#endif

