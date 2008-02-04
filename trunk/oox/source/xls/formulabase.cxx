/*************************************************************************
 *
 *  OpenOffice.org - a multi-platform office productivity suite
 *
 *  $RCSfile: formulabase.cxx,v $
 *
 *  $Revision: 1.1.2.26 $
 *
 *  last change: $Author: dr $ $Date: 2007/08/31 13:56:30 $
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

#include "oox/xls/formulabase.hxx"
#include <map>
#include <rtl/strbuf.hxx>
#include <com/sun/star/sheet/ReferenceFlags.hpp>
#include <com/sun/star/sheet/SingleReference.hpp>
#include <com/sun/star/sheet/ComplexReference.hpp>
#include <com/sun/star/sheet/FormulaLanguage.hpp>
#include <com/sun/star/sheet/FormulaMapGroup.hpp>
#include <com/sun/star/sheet/FormulaMapGroupSpecialOffset.hpp>
#include <com/sun/star/sheet/XFormulaOpCodeMapper.hpp>
#include "oox/core/containerhelper.hxx"
#include "oox/xls/addressconverter.hxx"

using ::rtl::OString;
using ::rtl::OStringBuffer;
using ::rtl::OUString;
using ::rtl::OStringToOUString;
using ::rtl::OUStringToOString;
using ::com::sun::star::uno::Any;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Sequence;
using ::com::sun::star::uno::Exception;
using ::com::sun::star::uno::UNO_QUERY_THROW;
using ::com::sun::star::lang::XMultiServiceFactory;
using ::com::sun::star::table::CellAddress;
using ::com::sun::star::table::CellRangeAddress;
using ::com::sun::star::sheet::SingleReference;
using ::com::sun::star::sheet::ComplexReference;
using ::com::sun::star::sheet::FormulaToken;
using ::com::sun::star::sheet::FormulaOpCodeMapEntry;
using ::com::sun::star::sheet::XSpreadsheetDocument;
using ::com::sun::star::sheet::XFormulaOpCodeMapper;
using ::oox::core::RefMap;
using ::oox::core::ContainerHelper;
using namespace ::com::sun::star::sheet::ReferenceFlags;

namespace oox {
namespace xls {

// function data ==============================================================

namespace {

const size_t FUNCINFO_CLASSCOUNT            = 5;        /// Number of token class entries.

const sal_uInt8 FUNCFLAG_VOLATILE           = 0x01;     /// Result is volatile (e.g. NOW() function).
const sal_uInt8 FUNCFLAG_IMPORTONLY         = 0x02;     /// Only used in import filter.
const sal_uInt8 FUNCFLAG_EXPORTONLY         = 0x04;     /// Only used in export filter.
const sal_uInt8 FUNCFLAG_MACROCALL          = 0x08;     /// Function is simulated by macro call in Excel.
const sal_uInt8 FUNCFLAG_EXTERNAL           = 0x10;     /// Function is external in Calc.

typedef ::boost::shared_ptr< FunctionInfo > FunctionInfoRef;

struct FunctionData
{
    const sal_Char*     mpcOdfFuncName;     /// ODF function name.
    const sal_Char*     mpcOoxFuncName;     /// OOX function name.
    sal_uInt16          mnBiffFuncId;       /// BIFF function identifier.
    sal_uInt8           mnMinParamCount;    /// Minimum number of parameters.
    sal_uInt8           mnMaxParamCount;    /// Maximum number of parameters.
    sal_uInt8           mnRetClass;         /// BIFF token class of the return value.
    sal_uInt8           mpnParamClass[ FUNCINFO_CLASSCOUNT ]; /// Expected BIFF token classes of parameters.
    sal_uInt8           mnFlags;            /// Additional flags.

    inline bool         isSupported( bool bImportFilter ) const;
};

inline bool FunctionData::isSupported( bool bImportFilter ) const
{
    /*  For import filters: the FUNCFLAG_EXPORTONLY flag must not be set,
        for export filters: the FUNCFLAG_IMPORTONLY flag must not be set. */
    return !getFlag( mnFlags, bImportFilter ? FUNCFLAG_EXPORTONLY : FUNCFLAG_IMPORTONLY );
}

const sal_uInt8 R = BIFF_TOKCLASS_REF;
const sal_uInt8 V = BIFF_TOKCLASS_VAL;
const sal_uInt8 A = BIFF_TOKCLASS_ARR;
const sal_uInt8 ER = FUNCINFO_PARAM_EXCELONLY | BIFF_TOKCLASS_REF;
const sal_uInt8 EV = FUNCINFO_PARAM_EXCELONLY | BIFF_TOKCLASS_VAL;
const sal_uInt8 EA = FUNCINFO_PARAM_EXCELONLY | BIFF_TOKCLASS_ARR;
const sal_uInt8 C = FUNCINFO_PARAM_CALCONLY;
const sal_uInt8 I = FUNCINFO_PARAM_INVALID;
const sal_uInt16 NOID = BIFF_FUNC_INVALID;

/** Functions new in BIFF2. */
static const FunctionData saFuncTableBiff2[] =
{
    { "COUNT",                  "COUNT",            0,      0,  30, V, { R }, 0 },
    { "IF",                     "IF",               1,      2,  3,  R, { V, R }, 0 },
    { "ISNA",                   "ISNA",             2,      1,  1,  V, { V }, 0 },
    { "ISERROR",                "ISERROR",          3,      1,  1,  V, { V }, 0 },
    { "SUM",                    "SUM",              4,      0,  30, V, { R }, 0 },
    { "AVERAGE",                "AVERAGE",          5,      1,  30, V, { R }, 0 },
    { "MIN",                    "MIN",              6,      1,  30, V, { R }, 0 },
    { "MAX",                    "MAX",              7,      1,  30, V, { R }, 0 },
    { "ROW",                    "ROW",              8,      0,  1,  V, { R }, 0 },
    { "COLUMN",                 "COLUMN",           9,      0,  1,  V, { R }, 0 },
    { "NA",                     "NA",               10,     0,  0,  V, {}, 0 },
    { "NPV",                    "NPV",              11,     2,  30, V, { V, R }, 0 },
    { "STDEV",                  "STDEV",            12,     1,  30, V, { R }, 0 },
    { "DOLLAR",                 "DOLLAR",           13,     1,  2,  V, { V }, 0 },
    { "FIXED",                  "FIXED",            14,     1,  2,  V, { V, V, C, I }, 0 },
    { "SIN",                    "SIN",              15,     1,  1,  V, { V }, 0 },
    { "COS",                    "COS",              16,     1,  1,  V, { V }, 0 },
    { "TAN",                    "TAN",              17,     1,  1,  V, { V }, 0 },
    { "COT",                    "TAN",              17,     1,  1,  V, { V }, FUNCFLAG_EXPORTONLY },
    { "ATAN",                   "ATAN",             18,     1,  1,  V, { V }, 0 },
    { "ACOT",                   "ATAN",             18,     1,  1,  V, { V }, FUNCFLAG_EXPORTONLY },
    { "PI",                     "PI",               19,     0,  0,  V, {}, 0 },
    { "SQRT",                   "SQRT",             20,     1,  1,  V, { V }, 0 },
    { "EXP",                    "EXP",              21,     1,  1,  V, { V }, 0 },
    { "LN",                     "LN",               22,     1,  1,  V, { V }, 0 },
    { "LOG10",                  "LOG10",            23,     1,  1,  V, { V }, 0 },
    { "ABS",                    "ABS",              24,     1,  1,  V, { V }, 0 },
    { "INT",                    "INT",              25,     1,  1,  V, { V }, 0 },
    { "SIGN",                   "SIGN",             26,     1,  1,  V, { V }, 0 },
    { "ROUND",                  "ROUND",            27,     2,  2,  V, { V }, 0 },
    { "LOOKUP",                 "LOOKUP",           28,     2,  3,  V, { V, R }, 0 },
    { "INDEX",                  "INDEX",            29,     2,  4,  R, { R, V }, 0 },
    { "REPT",                   "REPT",             30,     2,  2,  V, { V }, 0 },
    { "MID",                    "MID",              31,     3,  3,  V, { V }, 0 },
    { "LEN",                    "LEN",              32,     1,  1,  V, { V }, 0 },
    { "VALUE",                  "VALUE",            33,     1,  1,  V, { V }, 0 },
    { "TRUE",                   "TRUE",             34,     0,  0,  V, {}, 0 },
    { "FALSE",                  "FALSE",            35,     0,  0,  V, {}, 0 },
    { "AND",                    "AND",              36,     1,  30, V, { R }, 0 },
    { "OR",                     "OR",               37,     1,  30, V, { R }, 0 },
    { "NOT",                    "NOT",              38,     1,  1,  V, { V }, 0 },
    { "MOD",                    "MOD",              39,     2,  2,  V, { V }, 0 },
    { "DCOUNT",                 "DCOUNT",           40,     3,  3,  V, { R }, 0 },
    { "DSUM",                   "DSUM",             41,     3,  3,  V, { R }, 0 },
    { "DAVERAGE",               "DAVERAGE",         42,     3,  3,  V, { R }, 0 },
    { "DMIN",                   "DMIN",             43,     3,  3,  V, { R }, 0 },
    { "DMAX",                   "DMAX",             44,     3,  3,  V, { R }, 0 },
    { "DSTDEV",                 "DSTDEV",           45,     3,  3,  V, { R }, 0 },
    { "VAR",                    "VAR",              46,     1,  30, V, { R }, 0 },
    { "DVAR",                   "DVAR",             47,     3,  3,  V, { R }, 0 },
    { "TEXT",                   "TEXT",             48,     2,  2,  V, { V }, 0 },
    { "LINEST",                 "LINEST",           49,     1,  2,  A, { R, R, C, C, I }, 0 },
    { "TREND",                  "TREND",            50,     1,  3,  A, { R, R, R, C, I }, 0 },
    { "LOGEST",                 "LOGEST",           51,     1,  2,  A, { R, R, C, C, I }, 0 },
    { "GROWTH",                 "GROWTH",           52,     1,  3,  A, { R, R, R, C, I }, 0 },
    { "PV",                     "PV",               56,     3,  5,  V, { V }, 0 },
    { "FV",                     "FV",               57,     3,  5,  V, { V }, 0 },
    { "NPER",                   "NPER",             58,     3,  5,  V, { V }, 0 },
    { "PMT",                    "PMT",              59,     3,  5,  V, { V }, 0 },
    { "RATE",                   "RATE",             60,     3,  6,  V, { V }, 0 },
    { "MIRR",                   "MIRR",             61,     3,  3,  V, { R, V }, 0 },
    { "IRR",                    "IRR",              62,     1,  2,  V, { R, V }, 0 },
    { "RAND",                   "RAND",             63,     0,  0,  V, {}, FUNCFLAG_VOLATILE },
    { "MATCH",                  "MATCH",            64,     2,  3,  V, { V, R }, 0 },
    { "DATE",                   "DATE",             65,     3,  3,  V, { V }, 0 },
    { "TIME",                   "TIME",             66,     3,  3,  V, { V }, 0 },
    { "DAY",                    "DAY",              67,     1,  1,  V, { V }, 0 },
    { "MONTH",                  "MONTH",            68,     1,  1,  V, { V }, 0 },
    { "YEAR",                   "YEAR",             69,     1,  1,  V, { V }, 0 },
    { "WEEKDAY",                "WEEKDAY",          70,     1,  1,  V, { V, C, I }, 0 },
    { "HOUR",                   "HOUR",             71,     1,  1,  V, { V }, 0 },
    { "MINUTE",                 "MINUTE",           72,     1,  1,  V, { V }, 0 },
    { "SECOND",                 "SECOND",           73,     1,  1,  V, { V }, 0 },
    { "NOW",                    "NOW",              74,     0,  0,  V, {}, FUNCFLAG_VOLATILE },
    { "AREAS",                  "AREAS",            75,     1,  1,  V, { R }, 0 },
    { "ROWS",                   "ROWS",             76,     1,  1,  V, { R }, 0 },
    { "COLUMNS",                "COLUMNS",          77,     1,  1,  V, { R }, 0 },
    { "OFFSET",                 "OFFSET",           78,     3,  5,  R, { R, V }, FUNCFLAG_VOLATILE },
    { "SEARCH",                 "SEARCH",           82,     2,  3,  V, { V }, 0 },
    { "TRANSPOSE",              "TRANSPOSE",        83,     1,  1,  A, { A }, 0 },
    { "TYPE",                   "TYPE",             86,     1,  1,  V, { V }, 0 },
    { "ATAN2",                  "ATAN2",            97,     2,  2,  V, { V }, 0 },
    { "ASIN",                   "ASIN",             98,     1,  1,  V, { V }, 0 },
    { "ACOS",                   "ACOS",             99,     1,  1,  V, { V }, 0 },
    { "CHOOSE",                 "CHOOSE",           100,    2,  30, R, { V, R }, 0 },
    { "HLOOKUP",                "HLOOKUP",          101,    3,  3,  V, { V, R, R, C, I }, 0 },
    { "VLOOKUP",                "VLOOKUP",          102,    3,  3,  V, { V, R, R, C, I }, 0 },
    { "ISREF",                  "ISREF",            105,    1,  1,  V, { R }, 0 },
    { "LOG",                    "LOG",              109,    1,  2,  V, { V }, 0 },
    { "CHAR",                   "CHAR",             111,    1,  1,  V, { V }, 0 },
    { "LOWER",                  "LOWER",            112,    1,  1,  V, { V }, 0 },
    { "UPPER",                  "UPPER",            113,    1,  1,  V, { V }, 0 },
    { "PROPER",                 "PROPER",           114,    1,  1,  V, { V }, 0 },
    { "LEFT",                   "LEFT",             115,    1,  2,  V, { V }, 0 },
    { "RIGHT",                  "RIGHT",            116,    1,  2,  V, { V }, 0 },
    { "EXACT",                  "EXACT",            117,    2,  2,  V, { V }, 0 },
    { "TRIM",                   "TRIM",             118,    1,  1,  V, { V }, 0 },
    { "REPLACE",                "REPLACE",          119,    4,  4,  V, { V }, 0 },
    { "SUBSTITUTE",             "SUBSTITUTE",       120,    3,  4,  V, { V }, 0 },
    { "CODE",                   "CODE",             121,    1,  1,  V, { V }, 0 },
    { "FIND",                   "FIND",             124,    2,  3,  V, { V }, 0 },
    { "CELL",                   "CELL",             125,    1,  2,  V, { V, R }, FUNCFLAG_VOLATILE },
    { "ISERR",                  "ISERR",            126,    1,  1,  V, { V }, 0 },
    { "ISTEXT",                 "ISTEXT",           127,    1,  1,  V, { V }, 0 },
    { "ISNUMBER",               "ISNUMBER",         128,    1,  1,  V, { V }, 0 },
    { "ISBLANK",                "ISBLANK",          129,    1,  1,  V, { V }, 0 },
    { "T",                      "T",                130,    1,  1,  V, { R }, 0 },
    { "N",                      "N",                131,    1,  1,  V, { R }, 0 },
    { "DATEVALUE",              "DATEVALUE",        140,    1,  1,  V, { V }, 0 },
    { "TIMEVALUE",              "TIMEVALUE",        141,    1,  1,  V, { V }, 0 },
    { "SLN",                    "SLN",              142,    3,  3,  V, { V }, 0 },
    { "SYD",                    "SYD",              143,    4,  4,  V, { V }, 0 },
    { "DDB",                    "DDB",              144,    4,  5,  V, { V }, 0 },
    { "INDIRECT",               "INDIRECT",         148,    1,  2,  R, { V, EV, I }, FUNCFLAG_VOLATILE },
    { "CLEAN",                  "CLEAN",            162,    1,  1,  V, { V }, 0 },
    { "MDETERM",                "MDETERM",          163,    1,  1,  V, { A }, 0 },
    { "MINVERSE",               "MINVERSE",         164,    1,  1,  A, { A }, 0 },
    { "MMULT",                  "MMULT",            165,    2,  2,  A, { A }, 0 },
    { "IPMT",                   "IPMT",             167,    4,  6,  V, { V }, 0 },
    { "PPMT",                   "PPMT",             168,    4,  6,  V, { V }, 0 },
    { "COUNTA",                 "COUNTA",           169,    0,  30, V, { R }, 0 },
    { "PRODUCT",                "PRODUCT",          183,    0,  30, V, { R }, 0 },
    { "FACT",                   "FACT",             184,    1,  1,  V, { V }, 0 },
    { "DPRODUCT",               "DPRODUCT",         189,    3,  3,  V, { R }, 0 },
    { "ISNONTEXT",              "ISNONTEXT",        190,    1,  1,  V, { V }, 0 },
    { "STDEVP",                 "STDEVP",           193,    1,  30, V, { R }, 0 },
    { "VARP",                   "VARP",             194,    1,  30, V, { R }, 0 },
    { "DSTDEVP",                "DSTDEVP",          195,    3,  3,  V, { R }, 0 },
    { "DVARP",                  "DVARP",            196,    3,  3,  V, { R }, 0 },
    { "TRUNC",                  "TRUNC",            197,    1,  1,  V, { V, C, I }, 0 },
    { "ISLOGICAL",              "ISLOGICAL",        198,    1,  1,  V, { V }, 0 },
    { "DCOUNTA",                "DCOUNTA",          199,    3,  3,  V, { R }, 0 },
    { 0,                        0,                  255,    1,  30, R, { ER, R }, FUNCFLAG_IMPORTONLY }  // EXTERNAL
};

/** Functions new in BIFF3. */
static const FunctionData saFuncTableBiff3[] =
{
    { "LINEST",                 "LINEST",           49,     1,  4,  A, { R, R, V, V }, 0 },  // BIFF2: 1-2, BIFF3: 1-4,
    { "TREND",                  "TREND",            50,     1,  4,  A, { R, R, R, V }, 0 },  // BIFF2: 1-3, BIFF3: 1-4
    { "LOGEST",                 "LOGEST",           51,     1,  4,  A, { R, R, V, V }, 0 },  // BIFF2: 1-2, BIFF3: 1-4,
    { "GROWTH",                 "GROWTH",           52,     1,  4,  A, { R, R, R, V }, 0 },  // BIFF2: 1-3, BIFF3: 1-4
    { "TRUNC",                  "TRUNC",            197,    1,  2,  V, { V }, 0 },           // BIFF2: 1,   BIFF3: 1-2
    { "DOLLAR",                 "USDOLLAR",         204,    1,  2,  V, { V }, FUNCFLAG_IMPORTONLY },
    { 0/*"FIND"*/,              "FINDB",            205,    2,  3,  V, { V }, 0 },
    { 0/*"SEARCH"*/,            "SEARCHB",          206,    2,  3,  V, { V }, 0 },
    { 0/*"REPLACE"*/,           "REPLACEB",         207,    4,  4,  V, { V }, 0 },
    { 0/*"LEFT"*/,              "LEFTB",            208,    1,  2,  V, { V }, 0 },
    { 0/*"RIGHT"*/,             "RIGHTB",           209,    1,  2,  V, { V }, 0 },
    { 0/*"MID"*/,               "MIDB",             210,    3,  3,  V, { V }, 0 },
    { 0/*"LEN"*/,               "LENB",             211,    1,  1,  V, { V }, 0 },
    { "ROUNDUP",                "ROUNDUP",          212,    2,  2,  V, { V }, 0 },
    { "ROUNDDOWN",              "ROUNDDOWN",        213,    2,  2,  V, { V }, 0 },
    { "ASC",                    "ASC",              214,    1,  1,  V, { V }, 0 },
    { "JIS",                    "DBCS",             215,    1,  1,  V, { V }, 0 },
    { "ADDRESS",                "ADDRESS",          219,    2,  5,  V, { V, V, V, EV, V }, 0 },
    { "DAYS360",                "DAYS360",          220,    2,  2,  V, { V, V, C, I }, 0 },
    { "TODAY",                  "TODAY",            221,    0,  0,  V, {}, FUNCFLAG_VOLATILE },
    { "VDB",                    "VDB",              222,    5,  7,  V, { V }, 0 },
    { "MEDIAN",                 "MEDIAN",           227,    1,  30, V, { R }, 0 },
    { "SUMPRODUCT",             "SUMPRODUCT",       228,    1,  30, V, { A }, 0 },
    { "SINH",                   "SINH",             229,    1,  1,  V, { V }, 0 },
    { "COSH",                   "COSH",             230,    1,  1,  V, { V }, 0 },
    { "TANH",                   "TANH",             231,    1,  1,  V, { V }, 0 },
    { "COTH",                   "TANH",             231,    1,  1,  V, { V }, FUNCFLAG_EXPORTONLY },
    { "ASINH",                  "ASINH",            232,    1,  1,  V, { V }, 0 },
    { "ACOSH",                  "ACOSH",            233,    1,  1,  V, { V }, 0 },
    { "ATANH",                  "ATANH",            234,    1,  1,  V, { V }, 0 },
    { "ACOTH",                  "ATANH",            234,    1,  1,  V, { V }, FUNCFLAG_EXPORTONLY },
    { "DGET",                   "DGET",             235,    3,  3,  V, { R }, 0 },
    { "INFO",                   "INFO",             244,    1,  1,  V, { V }, FUNCFLAG_VOLATILE }
};

/** Functions new in BIFF4. */
static const FunctionData saFuncTableBiff4[] =
{
    { "FIXED",                  "FIXED",            14,     1,  3,  V, { V }, 0 },       // BIFF2-3: 1-2, BIFF4: 1-3
    { "RANK",                   "RANK",             216,    2,  3,  V, { V, R, V }, 0 },
    { "DB",                     "DB",               247,    4,  5,  V, { V }, 0 },
    { "FREQUENCY",              "FREQUENCY",        252,    2,  2,  A, { R }, 0 },
    { "ERROR.TYPE",             "ERROR.TYPE",       261,    1,  1,  V, { V }, 0 },
    { "AVEDEV",                 "AVEDEV",           269,    1,  30, V, { R }, 0 },
    { "BETADIST",               "BETADIST",         270,    3,  5,  V, { V }, 0 },
    { "GAMMALN",                "GAMMALN",          271,    1,  1,  V, { V }, 0 },
    { "BETAINV",                "BETAINV",          272,    3,  5,  V, { V }, 0 },
    { "BINOMDIST",              "BINOMDIST",        273,    4,  4,  V, { V }, 0 },
    { "LEGACY.CHIDIST",         "CHIDIST",          274,    2,  2,  V, { V }, 0 },
    { "LEGACY.CHIINV",          "CHIINV",           275,    2,  2,  V, { V }, 0 },
    { "COMBIN",                 "COMBIN",           276,    2,  2,  V, { V }, 0 },
    { "CONFIDENCE",             "CONFIDENCE",       277,    3,  3,  V, { V }, 0 },
    { "CRITBINOM",              "CRITBINOM",        278,    3,  3,  V, { V }, 0 },
    { "EVEN",                   "EVEN",             279,    1,  1,  V, { V }, 0 },
    { "EXPONDIST",              "EXPONDIST",        280,    3,  3,  V, { V }, 0 },
    { "LEGACY.FDIST",           "FDIST",            281,    3,  3,  V, { V }, 0 },
    { "LEGACY.FINV",            "FINV",             282,    3,  3,  V, { V }, 0 },
    { "FISHER",                 "FISHER",           283,    1,  1,  V, { V }, 0 },
    { "FISHERINV",              "FISHERINV",        284,    1,  1,  V, { V }, 0 },
    { "FLOOR",                  "FLOOR",            285,    2,  2,  V, { V, V, C, I }, 0 },
    { "GAMMADIST",              "GAMMADIST",        286,    4,  4,  V, { V }, 0 },
    { "GAMMAINV",               "GAMMAINV",         287,    3,  3,  V, { V }, 0 },
    { "CEILING",                "CEILING",          288,    2,  2,  V, { V, V, C, I }, 0 },
    { "HYPGEOMDIST",            "HYPGEOMDIST",      289,    4,  4,  V, { V }, 0 },
    { "LOGNORMDIST",            "LOGNORMDIST",      290,    3,  3,  V, { V }, 0 },
    { "LOGINV",                 "LOGINV",           291,    3,  3,  V, { V }, 0 },
    { "NEGBINOMDIST",           "NEGBINOMDIST",     292,    3,  3,  V, { V }, 0 },
    { "NORMDIST",               "NORMDIST",         293,    4,  4,  V, { V }, 0 },
    { "LEGACY.NORMSDIST",       "NORMSDIST",        294,    1,  1,  V, { V }, 0 },
    { "NORMINV",                "NORMINV",          295,    3,  3,  V, { V }, 0 },
    { "LEGACY.NORMSINV",        "NORMSINV",         296,    1,  1,  V, { V }, 0 },
    { "STANDARDIZE",            "STANDARDIZE",      297,    3,  3,  V, { V }, 0 },
    { "ODD",                    "ODD",              298,    1,  1,  V, { V }, 0 },
    { "PERMUT",                 "PERMUT",           299,    2,  2,  V, { V }, 0 },
    { "POISSON",                "POISSON",          300,    3,  3,  V, { V }, 0 },
    { "TDIST",                  "TDIST",            301,    3,  3,  V, { V }, 0 },
    { "WEIBULL",                "WEIBULL",          302,    4,  4,  V, { V }, 0 },
    { "SUMXMY2",                "SUMXMY2",          303,    2,  2,  V, { A }, 0 },
    { "SUMX2MY2",               "SUMX2MY2",         304,    2,  2,  V, { A }, 0 },
    { "SUMX2PY2",               "SUMX2PY2",         305,    2,  2,  V, { A }, 0 },
    { "LEGACY.CHITEST",         "CHITEST",          306,    2,  2,  V, { A }, 0 },
    { "CORREL",                 "CORREL",           307,    2,  2,  V, { A }, 0 },
    { "COVAR",                  "COVAR",            308,    2,  2,  V, { A }, 0 },
    { "FORECAST",               "FORECAST",         309,    3,  3,  V, { V, A }, 0 },
    { "FTEST",                  "FTEST",            310,    2,  2,  V, { A }, 0 },
    { "INTERCEPT",              "INTERCEPT",        311,    2,  2,  V, { A }, 0 },
    { "PEARSON",                "PEARSON",          312,    2,  2,  V, { A }, 0 },
    { "RSQ",                    "RSQ",              313,    2,  2,  V, { A }, 0 },
    { "STEYX",                  "STEYX",            314,    2,  2,  V, { A }, 0 },
    { "SLOPE",                  "SLOPE",            315,    2,  2,  V, { A }, 0 },
    { "TTEST",                  "TTEST",            316,    4,  4,  V, { A, A, V }, 0 },
    { "PROB",                   "PROB",             317,    3,  4,  V, { A, A, V }, 0 },
    { "DEVSQ",                  "DEVSQ",            318,    1,  30, V, { R }, 0 },
    { "GEOMEAN",                "GEOMEAN",          319,    1,  30, V, { R }, 0 },
    { "HARMEAN",                "HARMEAN",          320,    1,  30, V, { R }, 0 },
    { "SUMSQ",                  "SUMSQ",            321,    0,  30, V, { R }, 0 },
    { "KURT",                   "KURT",             322,    1,  30, V, { R }, 0 },
    { "SKEW",                   "SKEW",             323,    1,  30, V, { R }, 0 },
    { "ZTEST",                  "ZTEST",            324,    2,  3,  V, { R, V }, 0 },
    { "LARGE",                  "LARGE",            325,    2,  2,  V, { R, V }, 0 },
    { "SMALL",                  "SMALL",            326,    2,  2,  V, { R, V }, 0 },
    { "QUARTILE",               "QUARTILE",         327,    2,  2,  V, { R, V }, 0 },
    { "PERCENTILE",             "PERCENTILE",       328,    2,  2,  V, { R, V }, 0 },
    { "PERCENTRANK",            "PERCENTRANK",      329,    2,  3,  V, { R, V, EV, I }, 0 },
    { "MODE",                   "MODE",             330,    1,  30, V, { A }, 0 },
    { "TRIMMEAN",               "TRIMMEAN",         331,    2,  2,  V, { R, V }, 0 },
    { "TINV",                   "TINV",             332,    2,  2,  V, { V }, 0 },

    // *** Analysis add-in ***

    { "ACCRINT",                "ACCRINT",          NOID,   6,  7,  V, { V }, FUNCFLAG_EXTERNAL },
    { "ACCRINTM",               "ACCRINTM",         NOID,   4,  5,  V, { V }, FUNCFLAG_EXTERNAL },
    { "AMORDEGRC",              "AMORDEGRC",        NOID,   6,  7,  V, { V }, FUNCFLAG_EXTERNAL },
    { "AMORLINC",               "AMORLINC",         NOID,   6,  7,  V, { V }, FUNCFLAG_EXTERNAL },
    { "BESSELI",                "BESSELI",          NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "BESSELJ",                "BESSELJ",          NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "BESSELK",                "BESSELK",          NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "BESSELY",                "BESSELY",          NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "BIN2DEC",                "BIN2DEC",          NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "BIN2HEX",                "BIN2HEX",          NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "BIN2OCT",                "BIN2OCT",          NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "COMPLEX",                "COMPLEX",          NOID,   2,  3,  V, { V }, FUNCFLAG_EXTERNAL },
    { "CONVERT",                "CONVERT",          NOID,   3,  3,  V, { V }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
    { "COUPDAYBS",              "COUPDAYBS",        NOID,   3,  4,  V, { V }, FUNCFLAG_EXTERNAL },
    { "COUPDAYS",               "COUPDAYS",         NOID,   3,  4,  V, { V }, FUNCFLAG_EXTERNAL },
    { "COUPDAYSNC",             "COUPDAYSNC",       NOID,   3,  4,  V, { V }, FUNCFLAG_EXTERNAL },
    { "COUPNCD",                "COUPNCD",          NOID,   3,  4,  V, { V }, FUNCFLAG_EXTERNAL },
    { "COUPNUM",                "COUPNUM",          NOID,   3,  4,  V, { V }, FUNCFLAG_EXTERNAL },
    { "COUPPCD",                "COUPPCD",          NOID,   3,  4,  V, { V }, FUNCFLAG_EXTERNAL },
    { "CUMIPMT",                "CUMIPMT",          NOID,   6,  6,  V, { V }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
    { "CUMPRINC",               "CUMPRINC",         NOID,   6,  6,  V, { V }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
    { "DEC2BIN",                "DEC2BIN",          NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "DEC2HEX",                "DEC2HEX",          NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "DEC2OCT",                "DEC2OCT",          NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "DELTA",                  "DELTA",            NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "DISC",                   "DISC",             NOID,   4,  5,  V, { V }, FUNCFLAG_EXTERNAL },
    { "DOLLARDE",               "DOLLARDE",         NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "DOLLARFR",               "DOLLARFR",         NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "DURATION",               "DURATION",         NOID,   5,  6,  V, { V }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
    { "EDATE",                  "EDATE",            NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "EFFECT",                 "EFFECT",           NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
//    { "EUROCONVERT",            "EUROCONVERT",      NOID,   3,  5,  V, { V }, FUNCFLAG_EXTERNAL },       // Euro conversion add-in
    { "EOMONTH",                "EOMONTH",          NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "ERF",                    "ERF",              NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "ERFC",                   "ERFC",             NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "FACTDOUBLE",             "FACTDOUBLE",       NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "FVSCHEDULE",             "FVSCHEDULE",       NOID,   2,  2,  V, { V, A }, FUNCFLAG_EXTERNAL },
    { "GCD",                    "GCD",              NOID,   1,  30, V, { R }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
    { "GESTEP",                 "GESTEP",           NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "HEX2BIN",                "HEX2BIN",          NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "HEX2DEC",                "HEX2DEC",          NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "HEX2OCT",                "HEX2OCT",          NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMABS",                  "IMABS",            NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMAGINARY",              "IMAGINARY",        NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMARGUMENT",             "IMARGUMENT",       NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMCONJUGATE",            "IMCONJUGATE",      NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMCOS",                  "IMCOS",            NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMDIV",                  "IMDIV",            NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMEXP",                  "IMEXP",            NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMLN",                   "IMLN",             NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMLOG10",                "IMLOG10",          NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMLOG2",                 "IMLOG2",           NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMPOWER",                "IMPOWER",          NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMPRODUCT",              "IMPRODUCT",        NOID,   1,  30, V, { R }, FUNCFLAG_EXTERNAL },
    { "IMREAL",                 "IMREAL",           NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMSIN",                  "IMSIN",            NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMSQRT",                 "IMSQRT",           NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMSUB",                  "IMSUB",            NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "IMSUM",                  "IMSUM",            NOID,   1,  30, V, { R }, FUNCFLAG_EXTERNAL },
    { "INTRATE",                "INTRATE",          NOID,   4,  5,  V, { V }, FUNCFLAG_EXTERNAL },
    { "ISEVEN",                 "ISEVEN",           NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
    { "ISODD",                  "ISODD",            NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
    { "LCM",                    "LCM",              NOID,   1,  30, V, { R }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
    { "MDURATION",              "MDURATION",        NOID,   5,  6,  V, { V }, FUNCFLAG_EXTERNAL },
    { "MROUND",                 "MROUND",           NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "MULTINOMIAL",            "MULTINOMIAL",      NOID,   1,  30, V, { R }, FUNCFLAG_EXTERNAL },
    { "NETWORKDAYS",            "NETWORKDAYS",      NOID,   2,  3,  V, { V, V, A, C, I }, FUNCFLAG_EXTERNAL },
    { "NOMINAL",                "NOMINAL",          NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },       // Calc: builtin and add-in
    { "OCT2BIN",                "OCT2BIN",          NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "OCT2DEC",                "OCT2DEC",          NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "OCT2HEX",                "OCT2HEX",          NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "ODDFPRICE",              "ODDFPRICE",        NOID,   8,  9,  V, { V }, FUNCFLAG_EXTERNAL },
    { "ODDFYIELD",              "ODDFYIELD",        NOID,   8,  9,  V, { V }, FUNCFLAG_EXTERNAL },
    { "ODDLPRICE",              "ODDLPRICE",        NOID,   7,  8,  V, { V }, FUNCFLAG_EXTERNAL },
    { "ODDLYIELD",              "ODDLYIELD",        NOID,   8,  9,  V, { V }, FUNCFLAG_EXTERNAL },
    { "PRICE",                  "PRICE",            NOID,   6,  7,  V, { V }, FUNCFLAG_EXTERNAL },
    { "PRICEDISC",              "PRICEDISC",        NOID,   4,  5,  V, { V }, FUNCFLAG_EXTERNAL },
    { "PRICEMAT",               "PRICEMAT",         NOID,   5,  6,  V, { V }, FUNCFLAG_EXTERNAL },
    { "QUOTIENT",               "QUOTIENT",         NOID,   2,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "RANDBETWEEN",            "RANDBETWEEN",      NOID,   2,  2,  V, {}, FUNCFLAG_VOLATILE | FUNCFLAG_EXTERNAL },
    { "RECEIVED",               "RECEIVED",         NOID,   4,  5,  V, { V }, FUNCFLAG_EXTERNAL },
    { "SERIESSUM",              "SERIESSUM",        NOID,   4,  4,  V, { V, V, V, R }, FUNCFLAG_EXTERNAL },
    { "SQRTPI",                 "SQRTPI",           NOID,   1,  1,  V, { V }, FUNCFLAG_EXTERNAL },
    { "TBILLEQ",                "TBILLEQ",          NOID,   3,  3,  V, { V }, FUNCFLAG_EXTERNAL },
    { "TBILLPRICE",             "TBILLPRICE",       NOID,   3,  3,  V, { V }, FUNCFLAG_EXTERNAL },
    { "TBILLYIELD",             "TBILLYIELD",       NOID,   3,  3,  V, { V }, FUNCFLAG_EXTERNAL },
    { "WEEKNUM",                "WEEKNUM",          NOID,   1,  2,  V, { V }, FUNCFLAG_EXTERNAL },
    { "WORKDAY",                "WORKDAY",          NOID,   2,  3,  V, { V, V, A, C, I }, FUNCFLAG_EXTERNAL },
    { "XIRR",                   "XIRR",             NOID,   2,  3,  V, { A, R, V }, FUNCFLAG_EXTERNAL },
    { "XNPV",                   "XNPV",             NOID,   2,  3,  V, { V, A, R }, FUNCFLAG_EXTERNAL },
    { "YEARFRAC",               "YEARFRAC",         NOID,   2,  3,  V, { V }, FUNCFLAG_EXTERNAL },
    { "YIELD",                  "YIELD",            NOID,   6,  7,  V, { V }, FUNCFLAG_EXTERNAL },
    { "YIELDDISC",              "YIELDDISC",        NOID,   4,  5,  V, { V }, FUNCFLAG_EXTERNAL },
    { "YIELDMAT",               "YIELDMAT",         NOID,   5,  6,  V, { V }, FUNCFLAG_EXTERNAL }
};

/** Functions new in BIFF5/BIFF7. */
static const FunctionData saFuncTableBiff5[] =
{
    { "WEEKDAY",                "WEEKDAY",          70,     1,  2,  V, { V }, 0 },               // BIFF2-4: 1, BIFF5: 1-2
    { "HLOOKUP",                "HLOOKUP",          101,    3,  4,  V, { V, R, R, V }, 0 },      // BIFF2-4: 3, BIFF5: 3-4
    { "VLOOKUP",                "VLOOKUP",          102,    3,  4,  V, { V, R, R, V }, 0 },      // BIFF2-4: 3, BIFF5: 3-4
    { "DAYS360",                "DAYS360",          220,    2,  3,  V, { V }, 0 },               // BIFF3-4: 2, BIFF5: 2-3
    { 0,                        0,                  255,    1,  30, R, { ER, R }, FUNCFLAG_EXPORTONLY }, // MACRO or EXTERNAL
    { "CONCATENATE",            "CONCATENATE",      336,    0,  30, V, { V }, 0 },
    { "POWER",                  "POWER",            337,    2,  2,  V, { V }, 0 },
    { "RADIANS",                "RADIANS",          342,    1,  1,  V, { V }, 0 },
    { "DEGREES",                "DEGREES",          343,    1,  1,  V, { V }, 0 },
    { "SUBTOTAL",               "SUBTOTAL",         344,    2,  30, V, { V, R }, 0 },
    { "SUMIF",                  "SUMIF",            345,    2,  3,  V, { R, V, R }, 0 },
    { "COUNTIF",                "COUNTIF",          346,    2,  2,  V, { R, V }, 0 },
    { "COUNTBLANK",             "COUNTBLANK",       347,    1,  1,  V, { R }, 0 },
    { "ISPMT",                  "ISPMT",            350,    4,  4,  V, { V }, 0 },
    { 0,                        "DATEDIF",          351,    3,  3,  V, { V }, FUNCFLAG_IMPORTONLY },   // not supported in Calc
    { 0,                        "DATESTRING",       352,    1,  1,  V, { V }, FUNCFLAG_IMPORTONLY },   // not supported in Calc, missing in OOX spec
    { 0,                        "NUMBERSTRING",     353,    2,  2,  V, { V }, FUNCFLAG_IMPORTONLY },   // not supported in Calc, missing in OOX spec
    { "ROMAN",                  "ROMAN",            354,    1,  2,  V, { V }, 0 }
};

/** Functions new in BIFF8. */
static const FunctionData saFuncTableBiff8[] =
{
    { "GETPIVOTDATA",           "GETPIVOTDATA",     358,    2,  30, V, { V, R, V }, FUNCFLAG_IMPORTONLY },
    { "HYPERLINK",              "HYPERLINK",        359,    1,  2,  V, { V }, 0 },
    { 0,                        "PHONETIC",         360,    1,  1,  V, { R }, FUNCFLAG_IMPORTONLY },
    { "AVERAGEA",               "AVERAGEA",         361,    1,  30, V, { R }, 0 },
    { "MAXA",                   "MAXA",             362,    1,  30, V, { R }, 0 },
    { "MINA",                   "MINA",             363,    1,  30, V, { R }, 0 },
    { "STDEVPA",                "STDEVPA",          364,    1,  30, V, { R }, 0 },
    { "VARPA",                  "VARPA",            365,    1,  30, V, { R }, 0 },
    { "STDEVA",                 "STDEVA",           366,    1,  30, V, { R }, 0 },
    { "VARA",                   "VARA",             367,    1,  30, V, { R }, 0 },
    { "COM.MICROSOFT.BAHTTEXT", "BAHTTEXT",         368,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "THAIDAYOFWEEK",    369,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "THAIDIGIT",        370,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "THAIMONTHOFYEAR",  371,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "THAINUMSOUND",     372,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "THAINUMSTRING",    373,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "THAISTRINGLENGTH", 374,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "ISTHAIDIGIT",      375,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "ROUNDBAHTDOWN",    376,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "ROUNDBAHTUP",      377,    1,  1,  V, { V }, FUNCFLAG_MACROCALL },
    { 0,                        "THAIYEAR",         378,    1,  1,  V, { V }, FUNCFLAG_MACROCALL }
};

/** Functions new in OOX. */
static const FunctionData saFuncTableOox[] =
{
    { 0,                        "AVERAGEIF",        NOID,   2,  3,  V, { R, V, R }, 0 },
    { 0,                        "AVERAGEIFS",       NOID,   3,  30, V, { R, V }, 0 },
    { 0,                        "COUNTIFS",         NOID,   3,  30, V, { R, V }, 0 },
    { 0,                        "CUBEKPIMEMBER",    NOID,   3,  4,  V, { V }, 0 },
    { 0,                        "CUBEMEMBER",       NOID,   2,  3,  V, { V, A, V }, 0 },
    { 0,                        "CUBEMEMBERPROPERTY",NOID,  3,  3,  V, { V }, 0 },
    { 0,                        "CUBERANKEDMEMBER", NOID,   3,  4,  V, { V }, 0 },
    { 0,                        "CUBESET",          NOID,   2,  5,  V, { V, R, V }, 0 },
    { 0,                        "CUBESETCOUNT",     NOID,   1,  1,  V, { V }, 0 },
    { 0,                        "CUBEVALUE",        NOID,   2,  2,  V, { V, R }, 0 },
    { 0,                        "IFERROR",          NOID,   2,  2,  V, { V, R }, 0 },
    { 0,                        "RTD",              NOID,   3,  3,  A, { V, V, R }, 0 },
    { 0,                        "SUMIFS",           NOID,   3,  30, V, { R, V }, 0 }
};

/** Functions defined by OpenFormula, but not supported by Calc or by Excel. */
static const FunctionData saFuncTableOdf[] =
{
    { "ARABIC",                 0,                  NOID,   1,  1,  V, { V }, 0 },
    { "B",                      0,                  NOID,   3,  4,  V, { V }, 0 },
    { "BASE",                   0,                  NOID,   2,  3,  V, { V }, 0 },
    { "BITAND",                 0,                  NOID,   2,  2,  V, { V }, 0 },
    { "BITLSHIFT",              0,                  NOID,   2,  2,  V, { V }, 0 },
    { "BITOR",                  0,                  NOID,   2,  2,  V, { V }, 0 },
    { "BITRSHIFT",              0,                  NOID,   2,  2,  V, { V }, 0 },
    { "BITXOR",                 0,                  NOID,   2,  2,  V, { V }, 0 },
    { "CHISQDIST",              0,                  NOID,   2,  3,  V, { V }, 0 },
    { "CHISQINV",               0,                  NOID,   2,  2,  V, { V }, 0 },
    { "COMBINA",                0,                  NOID,   2,  2,  V, { V }, 0 },
    { "DAYS",                   0,                  NOID,   2,  2,  V, { V }, 0 },
    { "DDE",                    0,                  NOID,   3,  4,  V, { V }, 0 },
    { "DECIMAL",                0,                  NOID,   2,  2,  V, { V }, 0 },
    { "FDIST",                  0,                  NOID,   3,  4,  V, { V }, 0 },
    { "FINV",                   0,                  NOID,   3,  3,  V, { V }, 0 },
    { "FORMULA",                0,                  NOID,   1,  1,  V, { R }, 0 },
    { "GAMMA",                  0,                  NOID,   1,  1,  V, { V }, 0 },
    { "GAUSS",                  0,                  NOID,   1,  1,  V, { V }, 0 },
    { "IFNA",                   0,                  NOID,   2,  2,  V, { V, R }, 0 },
    { "ISFORMULA",              0,                  NOID,   1,  1,  V, { R }, 0 },
    { "ISOWEEKNUM",             0,                  NOID,   1,  2,  V, { V }, 0 },
    { "MULTIPLE.OPERATIONS",    0,                  NOID,   3,  5,  V, { R }, 0 },
    { "MUNIT",                  0,                  NOID,   1,  1,  A, { V }, 0 },
    { "NUMBERVALUE",            0,                  NOID,   2,  2,  V, { V }, 0 },
    { "PDURATION",              0,                  NOID,   3,  3,  V, { V }, 0 },
    { "PERMUTATIONA",           0,                  NOID,   2,  2,  V, { V }, 0 },
    { "PHI",                    0,                  NOID,   1,  1,  V, { V }, 0 },
    { "RRI",                    0,                  NOID,   3,  3,  V, { V }, 0 },
    { "SHEET",                  0,                  NOID,   1,  1,  V, { R }, 0 },
    { "SHEETS",                 0,                  NOID,   0,  1,  V, { R }, 0 },
    { "SKEWP",                  0,                  NOID,   1,  30, V, { R }, 0 },
    { "UNICHAR",                0,                  NOID,   1,  1,  V, { V }, 0 },
    { "UNICODE",                0,                  NOID,   1,  1,  V, { V }, 0 },
    { "XOR",                    0,                  NOID,   1,  30, V, { R }, 0 }
};

} // namespace

// function info parameter class iterator =====================================

FuncInfoParamClassIterator::FuncInfoParamClassIterator( const FunctionInfo& rFuncInfo ) :
    mpnParamClass( rFuncInfo.mpnParamClass ),
    mpnParamClassEnd( rFuncInfo.mpnParamClass + FUNCINFO_CLASSCOUNT )
{
}

FuncInfoParamClassIterator& FuncInfoParamClassIterator::operator++()
{
    if( (mpnParamClass + 1 < mpnParamClassEnd) && (mpnParamClass[ 1 ] != 0) )
        ++mpnParamClass;
    return *this;
}

// function provider implementation ===========================================

class FunctionProviderImpl
{
public:
    explicit            FunctionProviderImpl(
                            ApiOpCodes& rOpCodes,
                            const Reference< XSpreadsheetDocument >& rxDocument,
                            bool bImportFilter );

    explicit            FunctionProviderImpl(
                            ApiOpCodes& rOpCodes,
                            const Reference< XSpreadsheetDocument >& rxDocument,
                            BiffType eBiff,
                            bool bImportFilter );

    const FunctionInfo* getFuncInfoFromApiToken( const ApiToken& rToken ) const;
    const FunctionInfo* getFuncInfoFromOoxFuncName( const OUString& rFuncName ) const;
    const FunctionInfo* getFuncInfoFromBiffFuncId( sal_uInt16 nFuncId ) const;
    const FunctionInfo* getFuncInfoFromExternCallName( const OUString& rExtCallName ) const;

    Sequence< FormulaOpCodeMapEntry > getOoxParserMap() const;

private:
    typedef ::std::map< OUString, FormulaToken >    FormulaTokenMap;
    typedef Sequence< FormulaOpCodeMapEntry >       OpCodeEntrySequence;
    typedef ::std::vector< FormulaOpCodeMapEntry >  OpCodeEntryVector;

    static bool         fillEntrySeq( OpCodeEntrySequence& orEntrySeq, const Reference< XFormulaOpCodeMapper >& rxMapper, sal_Int32 nMapGroup );
    static bool         fillTokenMap( FormulaTokenMap& orTokenMap, const Reference< XFormulaOpCodeMapper >& rxMapper, sal_Int32 nMapGroup );

    static bool         initOpCode( sal_Int32& ornOpCode, const OpCodeEntrySequence& rEntrySeq, sal_Int32 nSpecialId );
    static bool         initOpCode( OpCodeEntryVector& orParserMap, sal_Int32& ornOpCode, const FormulaTokenMap& rTokenMap, const sal_Char* pcOdfName, const sal_Char* pcOoxName );

    void                construct( const Reference< XSpreadsheetDocument >& rxDocument, bool bImportFilter );
    void                construct( const Reference< XSpreadsheetDocument >& rxDocument, BiffType eBiff, bool bImportFilter );

    bool                initFuncNames( const OpCodeEntrySequence& rEntrySeq );
    void                initOpCodes( const Reference< XSpreadsheetDocument >& rxDocument );

    void                initFuncOpCode( FunctionInfo& orFuncInfo, const FormulaTokenMap& rFuncTokens );
    void                initFuncMaps( const FunctionData* pBeg, const FunctionData* pEnd );

private:
    typedef RefMap< sal_Int32, FunctionInfo >   OpCodeFuncMap;
    typedef RefMap< OUString, FunctionInfo >    FuncNameMap;
    typedef RefMap< sal_uInt16, FunctionInfo >  BiffIdFuncMap;

    ApiOpCodes&         mrOpCodes;          /// All needed API op-codes.
    FormulaTokenMap     maIntFuncTokens;    /// Internal functions keyed by ODFF name.
    FormulaTokenMap     maExtFuncTokens;    /// External functions keyed by ODFF name.
    OpCodeFuncMap       maOpCodeFuncs;      /// Maps API op-codes to function data.
    FuncNameMap         maOoxFuncs;         /// Maps OOX function names to function data.
    FuncNameMap         maExtProgFuncs;     /// Maps programmatical API function names to function data.
    BiffIdFuncMap       maBiffFuncs;        /// Maps BIFF function indexes to function data.
    FuncNameMap         maMacroFuncs;       /// Maps BIFF macro function names to function data.
    OpCodeEntryVector   maParserMap;        /// OOX token mapping for formula parser service.
    bool                mbImportFilter;     /// True = import filter, false = export filter.
};

// ----------------------------------------------------------------------------

FunctionProviderImpl::FunctionProviderImpl( ApiOpCodes& rOpCodes,
        const Reference< XSpreadsheetDocument >& rxDocument, bool bImportFilter ) :
    mrOpCodes( rOpCodes )
{
    construct( rxDocument, bImportFilter );
}

FunctionProviderImpl::FunctionProviderImpl( ApiOpCodes& rOpCodes,
        const Reference< XSpreadsheetDocument >& rxDocument, BiffType eBiff, bool bImportFilter ) :
    mrOpCodes( rOpCodes )
{
    construct( rxDocument, eBiff, bImportFilter );
}

const FunctionInfo* FunctionProviderImpl::getFuncInfoFromApiToken( const ApiToken& rToken ) const
{
    const FunctionInfo* pFuncInfo = 0;
    if( (rToken.OpCode == mrOpCodes.OPCODE_EXTERNAL) && rToken.Data.hasValue() )
    {
        OUString aProgFuncName;
        if( rToken.Data >>= aProgFuncName )
            pFuncInfo = maExtProgFuncs.get( aProgFuncName ).get();
    }
    else
    {
        pFuncInfo = maOpCodeFuncs.get( rToken.OpCode ).get();
    }
    return pFuncInfo;
}

const FunctionInfo* FunctionProviderImpl::getFuncInfoFromOoxFuncName( const OUString& rFuncName ) const
{
    return maOoxFuncs.get( rFuncName ).get();
}

const FunctionInfo* FunctionProviderImpl::getFuncInfoFromBiffFuncId( sal_uInt16 nFuncId ) const
{
    return maBiffFuncs.get( nFuncId ).get();
}

const FunctionInfo* FunctionProviderImpl::getFuncInfoFromExternCallName( const OUString& rExtCallName ) const
{
    return maMacroFuncs.get( rExtCallName ).get();
}

Sequence< FormulaOpCodeMapEntry > FunctionProviderImpl::getOoxParserMap() const
{
    return ContainerHelper::vectorToSequence( maParserMap );
}

// private --------------------------------------------------------------------

bool FunctionProviderImpl::fillEntrySeq( OpCodeEntrySequence& orEntrySeq,
        const Reference< XFormulaOpCodeMapper >& rxMapper, sal_Int32 nMapGroup )
{
    try
    {
        orEntrySeq = rxMapper->getAvailableMappings( ::com::sun::star::sheet::FormulaLanguage::ODFF, nMapGroup );
        return orEntrySeq.hasElements();
    }
    catch( Exception& )
    {
    }
    return false;
}

bool FunctionProviderImpl::fillTokenMap( FormulaTokenMap& orTokenMap,
        const Reference< XFormulaOpCodeMapper >& rxMapper, sal_Int32 nMapGroup )
{
    orTokenMap.clear();
    OpCodeEntrySequence aEntrySeq;
    if( fillEntrySeq( aEntrySeq, rxMapper, nMapGroup ) )
    {
        const FormulaOpCodeMapEntry* pEntry = aEntrySeq.getConstArray();
        const FormulaOpCodeMapEntry* pEntryEnd = pEntry + aEntrySeq.getLength();
        for( ; pEntry != pEntryEnd; ++pEntry )
            orTokenMap[ pEntry->Name ] = pEntry->Token;
    }
    return !orTokenMap.empty();
}

bool FunctionProviderImpl::initOpCode( sal_Int32& ornOpCode,
        const OpCodeEntrySequence& rEntrySeq, sal_Int32 nSpecialId )
{
    if( (0 <= nSpecialId) && (nSpecialId < rEntrySeq.getLength()) )
    {
        ornOpCode = rEntrySeq[ nSpecialId ].Token.OpCode;
        return true;
    }
    OSL_ENSURE( false,
        OStringBuffer( "FunctionProviderImpl::initOpCode - opcode for special offset " ).
        append( nSpecialId ).append( " not found" ).getStr() );
    return false;
}

bool FunctionProviderImpl::initOpCode( OpCodeEntryVector& orParserMap, sal_Int32& ornOpCode,
        const FormulaTokenMap& rTokenMap, const sal_Char* pcOdfName, const sal_Char* pcOoxName )
{
    OUString aOdfName = OUString::createFromAscii( pcOdfName );
    FormulaTokenMap::const_iterator aIt = rTokenMap.find( aOdfName );
    if( aIt != rTokenMap.end() )
    {
        ornOpCode = aIt->second.OpCode;
        if( pcOoxName )
        {
            FormulaOpCodeMapEntry aEntry;
            aEntry.Name = OUString::createFromAscii( pcOoxName );
            aEntry.Token.OpCode = ornOpCode;
            orParserMap.push_back( aEntry );
        }
        return true;
    }
    OSL_ENSURE( false,
        OStringBuffer( "FunctionProviderImpl::initOpCode - opcode for \"" ).
        append( OUStringToOString( aOdfName, RTL_TEXTENCODING_ASCII_US ) ).
        append( "\" not found" ).getStr() );
    return false;
}

void FunctionProviderImpl::construct(
        const Reference< XSpreadsheetDocument >& rxDocument, bool bImportFilter )
{
    construct( rxDocument, BIFF8, bImportFilter );
    // additional functions for OOX
    initFuncMaps( saFuncTableOox, STATIC_TABLE_END( saFuncTableOox ) );
}

void FunctionProviderImpl::construct(
        const Reference< XSpreadsheetDocument >& rxDocument, BiffType eBiff, bool bImportFilter )
{
    mbImportFilter = bImportFilter;
    OSL_ENSURE( mbImportFilter, "FunctionProviderImpl::construct - need special handling for macro call functions" );

    // operator op-codes, special op-codes, function op-codes
    initOpCodes( rxDocument );

    /*  Add functions supported in the current BIFF version only.
        Function tables from later BIFF versions may overwrite single
        functions from earlier tables. */
    if( eBiff >= BIFF2 )
        initFuncMaps( saFuncTableBiff2, STATIC_TABLE_END( saFuncTableBiff2 ) );
    if( eBiff >= BIFF3 )
        initFuncMaps( saFuncTableBiff3, STATIC_TABLE_END( saFuncTableBiff3 ) );
    if( eBiff >= BIFF4 )
        initFuncMaps( saFuncTableBiff4, STATIC_TABLE_END( saFuncTableBiff4 ) );
    if( eBiff >= BIFF5 )
        initFuncMaps( saFuncTableBiff5, STATIC_TABLE_END( saFuncTableBiff5 ) );
    if( eBiff >= BIFF8 )
        initFuncMaps( saFuncTableBiff8, STATIC_TABLE_END( saFuncTableBiff8 ) );
}

bool FunctionProviderImpl::initFuncNames( const OpCodeEntrySequence& rEntrySeq )
{
    const FormulaOpCodeMapEntry* pEntry = rEntrySeq.getConstArray();
    const FormulaOpCodeMapEntry* pEntryEnd = pEntry + rEntrySeq.getLength();
    for( ; pEntry != pEntryEnd; ++pEntry )
    {
        if( pEntry->Token.OpCode == mrOpCodes.OPCODE_EXTERNAL )
            maExtFuncTokens[ pEntry->Name ] = pEntry->Token;
        else
            maIntFuncTokens[ pEntry->Name ] = pEntry->Token;
    }
    return true;
}

void FunctionProviderImpl::initOpCodes( const Reference< XSpreadsheetDocument >& rxDocument )
{
    bool bIsValid = false;
    try
    {
        Reference< XMultiServiceFactory > xFactory( rxDocument, UNO_QUERY_THROW );
        Reference< XFormulaOpCodeMapper > xMapper( xFactory->createInstance(
            CREATE_OUSTRING( "com.sun.star.sheet.FormulaOpCodeMapper" ) ), UNO_QUERY_THROW );

        // op-codes provided as attributes
        mrOpCodes.OPCODE_EXTERNAL = xMapper->getOpCodeExternal();
        mrOpCodes.OPCODE_UNKNOWN = xMapper->getOpCodeUnknown();

        using namespace ::com::sun::star::sheet::FormulaMapGroup;
        using namespace ::com::sun::star::sheet::FormulaMapGroupSpecialOffset;

        OpCodeEntrySequence aEntrySeq;
        FormulaTokenMap aTokenMap;
        bIsValid =
            // special
            fillEntrySeq( aEntrySeq, xMapper, SPECIAL ) &&
            initOpCode( mrOpCodes.OPCODE_PUSH,    aEntrySeq, PUSH ) &&
            initOpCode( mrOpCodes.OPCODE_MISSING, aEntrySeq, MISSING ) &&
            initOpCode( mrOpCodes.OPCODE_SPACES,  aEntrySeq, SPACES ) &&
            initOpCode( mrOpCodes.OPCODE_NAME,    aEntrySeq, NAME ) &&
            initOpCode( mrOpCodes.OPCODE_DBAREA,  aEntrySeq, DB_AREA ) &&
            initOpCode( mrOpCodes.OPCODE_NLR,     aEntrySeq, COL_ROW_NAME ) &&
            initOpCode( mrOpCodes.OPCODE_MACRO,   aEntrySeq, MACRO ) &&
            initOpCode( mrOpCodes.OPCODE_BAD,     aEntrySeq, BAD ) &&
            initOpCode( mrOpCodes.OPCODE_NONAME,  aEntrySeq, NO_NAME ) &&
            // separators
            fillTokenMap( aTokenMap, xMapper, SEPARATORS ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_OPEN,          aTokenMap, "(",  "("  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_CLOSE,         aTokenMap, ")",  ")"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_SEP,           aTokenMap, ";",  ","  ) &&
            // array separators
            fillTokenMap( aTokenMap, xMapper, ARRAY_SEPARATORS ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_ARRAY_OPEN,    aTokenMap, "{",  "{"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_ARRAY_CLOSE,   aTokenMap, "}",  "}"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_ARRAY_ROWSEP,  aTokenMap, "|",  ";"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_ARRAY_COLSEP,  aTokenMap, ";",  ","  ) &&
            // unary operators
            fillTokenMap( aTokenMap, xMapper, UNARY_OPERATORS ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_PLUS_SIGN,     aTokenMap, "+",  0    ) && // same op-code as OPCODE_ADD
            initOpCode( maParserMap, mrOpCodes.OPCODE_MINUS_SIGN,    aTokenMap, "-",  "-"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_PERCENT,       aTokenMap, "%",  "%"  ) &&
            // binary operators
            fillTokenMap( aTokenMap, xMapper, BINARY_OPERATORS ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_ADD,           aTokenMap, "+",  "+"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_SUB,           aTokenMap, "-",  "-"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_MULT,          aTokenMap, "*",  "*"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_DIV,           aTokenMap, "/",  "/"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_POWER,         aTokenMap, "^",  "^"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_CONCAT,        aTokenMap, "&",  "&"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_EQUAL,         aTokenMap, "=",  "="  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_NOT_EQUAL,     aTokenMap, "<>", "<>" ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_LESS,          aTokenMap, "<",  "<"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_LESS_EQUAL,    aTokenMap, "<=", "<=" ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_GREATER,       aTokenMap, ">",  ">"  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_GREATER_EQUAL, aTokenMap, ">=", ">=" ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_INTERSECT,     aTokenMap, "!",  " "  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_LIST,          aTokenMap, "~",  ","  ) &&
            initOpCode( maParserMap, mrOpCodes.OPCODE_RANGE,         aTokenMap, ":",  ":"  ) &&
            // functions
            fillEntrySeq( aEntrySeq, xMapper, FUNCTIONS ) &&
            initFuncNames( aEntrySeq );

        // OPCODE_PLUS_SIGN and OPCODE_ADD should be equal, otherwise "+" has to be passed above
        OSL_ENSURE( mrOpCodes.OPCODE_PLUS_SIGN == mrOpCodes.OPCODE_ADD,
            "FunctionProviderImpl::initOpCodes - need opcode mapping for OPCODE_PLUS_SIGN" );
        // OPCODE_LIST not supported in Calc core
        mrOpCodes.OPCODE_LIST = mrOpCodes.OPCODE_SEP;
    }
    catch( Exception& )
    {
    }
    OSL_ENSURE( bIsValid, "FunctionProviderImpl::initOpCodes - opcodes not initialized" );
}

void FunctionProviderImpl::initFuncOpCode( FunctionInfo& orFuncInfo, const FormulaTokenMap& rFuncTokens )
{
    bool bRet = false;
    if( orFuncInfo.mnBiffFuncId == BIFF_FUNC_EXTERNCALL )
    {
        orFuncInfo.mnApiOpCode = mrOpCodes.OPCODE_EXTERNAL;
        bRet = true;
    }
    else if( orFuncInfo.maOdfFuncName.getLength() > 0 )
    {
        FormulaTokenMap::const_iterator aIt = rFuncTokens.find( orFuncInfo.maOdfFuncName );
        if( aIt != rFuncTokens.end() )
        {
            orFuncInfo.mnApiOpCode = aIt->second.OpCode;
            OSL_ENSURE( orFuncInfo.mnApiOpCode != mrOpCodes.OPCODE_NONAME,
                OStringBuffer( "FunctionProviderImpl::initFuncOpCode - no valid op-code for \"" ).
                append( OUStringToOString( orFuncInfo.maOdfFuncName, RTL_TEXTENCODING_ASCII_US ) ).
                append( '"' ).getStr() );
            OSL_ENSURE( orFuncInfo.maOoxFuncName.getLength() > 0,
                OStringBuffer( "FunctionProviderImpl::initFuncOpCode - no valid OOX function name for \"" ).
                append( OUStringToOString( orFuncInfo.maOdfFuncName, RTL_TEXTENCODING_ASCII_US ) ).
                append( '"' ).getStr() );
            bRet = (orFuncInfo.mnApiOpCode != mrOpCodes.OPCODE_EXTERNAL) ||
                ((aIt->second.Data >>= orFuncInfo.maExtProgName) && (orFuncInfo.maExtProgName.getLength() > 0));
            if( bRet )
            {
                // create the parser map entry
                FormulaOpCodeMapEntry aEntry;
                aEntry.Name = orFuncInfo.maOoxFuncName;
                aEntry.Token = aIt->second;
                maParserMap.push_back( aEntry );
            }
        }
        OSL_ENSURE( bRet,
            OStringBuffer( "FunctionProviderImpl::initFuncOpCode - opcode or external name for \"" ).
            append( OUStringToOString( orFuncInfo.maOdfFuncName, RTL_TEXTENCODING_ASCII_US ) ).
            append( "\" not found" ).getStr() );
    }
    if( !bRet || (orFuncInfo.mnApiOpCode == mrOpCodes.OPCODE_UNKNOWN) )
        orFuncInfo.mnApiOpCode = mrOpCodes.OPCODE_NONAME;
}

void FunctionProviderImpl::initFuncMaps( const FunctionData* pBeg, const FunctionData* pEnd )
{
    for( const FunctionData* pIt = pBeg; pIt != pEnd; ++pIt )
    {
        if( pIt->isSupported( mbImportFilter ) )
        {
            // create a function info object
            FunctionInfoRef xFuncInfo( new FunctionInfo );
            if( pIt->mpcOdfFuncName )
                xFuncInfo->maOdfFuncName = OUString::createFromAscii( pIt->mpcOdfFuncName );
            if( pIt->mpcOoxFuncName )
                xFuncInfo->maOoxFuncName = OUString::createFromAscii( pIt->mpcOoxFuncName );
            if( getFlag( pIt->mnFlags, FUNCFLAG_MACROCALL ) )
                xFuncInfo->maExternCallName = CREATE_OUSTRING( "_xlfn." ) + xFuncInfo->maOoxFuncName;
            else if( getFlag( pIt->mnFlags, FUNCFLAG_EXTERNAL ) )
                xFuncInfo->maExternCallName = xFuncInfo->maOoxFuncName;
            xFuncInfo->mnBiffFuncId = pIt->mnBiffFuncId;
            xFuncInfo->mnMinParamCount = pIt->mnMinParamCount;
            xFuncInfo->mnMaxParamCount = pIt->mnMaxParamCount;
            xFuncInfo->mnRetClass = pIt->mnRetClass;
            xFuncInfo->mpnParamClass = pIt->mpnParamClass;
            xFuncInfo->mbVolatile = getFlag( pIt->mnFlags, FUNCFLAG_VOLATILE );

            // get API opcode from BIFF function index or function name
            initFuncOpCode( *xFuncInfo, getFlag( pIt->mnFlags, FUNCFLAG_EXTERNAL ) ? maExtFuncTokens : maIntFuncTokens );

            // insert the function info into the maps
            if( xFuncInfo->mnApiOpCode != mrOpCodes.OPCODE_NONAME )
            {
                if( (xFuncInfo->mnApiOpCode == mrOpCodes.OPCODE_EXTERNAL) && (xFuncInfo->maExtProgName.getLength() > 0) )
                    maExtProgFuncs[ xFuncInfo->maExtProgName ] = xFuncInfo;
                else
                    maOpCodeFuncs[ xFuncInfo->mnApiOpCode ] = xFuncInfo;
            }
            if( xFuncInfo->maOoxFuncName.getLength() > 0 )
                maOoxFuncs[ xFuncInfo->maOoxFuncName ] = xFuncInfo;
            if( xFuncInfo->mnBiffFuncId != BIFF_FUNC_INVALID )
                maBiffFuncs[ xFuncInfo->mnBiffFuncId ] = xFuncInfo;
            if( xFuncInfo->maExternCallName.getLength() > 0 )
                maMacroFuncs[ xFuncInfo->maExternCallName ] = xFuncInfo;
        }
    }
}

// function provider ==========================================================

FunctionProvider::FunctionProvider( const GlobalDataHelper& rGlobalData )
{
    bool bImportFilter = rGlobalData.getBaseFilter().isImportFilter();
    switch( rGlobalData.getFilterType() )
    {
        case FILTER_OOX:
            mxImpl.reset( new FunctionProviderImpl( *this, rGlobalData.getDocument(), bImportFilter ) );
        break;
        case FILTER_BIFF:
            mxImpl.reset( new FunctionProviderImpl( *this, rGlobalData.getDocument(), rGlobalData.getBiff(), bImportFilter ) );
        break;
        case FILTER_UNKNOWN: break;
    }
}

FunctionProvider::FunctionProvider( const Reference< XSpreadsheetDocument >& rxDocument, bool bImportFilter )
{
    mxImpl.reset( new FunctionProviderImpl( *this, rxDocument, bImportFilter ) );
}

FunctionProvider::FunctionProvider( const Reference< XSpreadsheetDocument >& rxDocument, BiffType eBiff, bool bImportFilter )
{
    mxImpl.reset( new FunctionProviderImpl( *this, rxDocument, eBiff, bImportFilter ) );
}

FunctionProvider::~FunctionProvider()
{
}

const FunctionInfo* FunctionProvider::getFuncInfoFromApiToken( const ApiToken& rToken ) const
{
    return mxImpl->getFuncInfoFromApiToken( rToken );
}

const FunctionInfo* FunctionProvider::getFuncInfoFromOoxFuncName( const OUString& rFuncName ) const
{
    return mxImpl->getFuncInfoFromOoxFuncName( rFuncName );
}

const FunctionInfo* FunctionProvider::getFuncInfoFromBiffFuncId( sal_uInt16 nFuncId ) const
{
    return mxImpl->getFuncInfoFromBiffFuncId( nFuncId );
}

const FunctionInfo* FunctionProvider::getFuncInfoFromExternCallName( const OUString& rExtCallName ) const
{
    return mxImpl->getFuncInfoFromExternCallName( rExtCallName );
}

Sequence< FormulaOpCodeMapEntry > FunctionProvider::getOoxParserMap() const
{
    return mxImpl->getOoxParserMap();
}

// token sequence iterator ====================================================

ApiTokenIterator::ApiTokenIterator( const ApiTokenSequence& rTokens, sal_Int32 nSpacesOpCode, bool bSkipSpaces ) :
    mpToken( rTokens.getConstArray() ),
    mpTokenEnd( rTokens.getConstArray() + rTokens.getLength() ),
    mnSpacesOpCode( nSpacesOpCode ),
    mbSkipSpaces( bSkipSpaces )
{
    skipSpaces();
}

ApiTokenIterator::ApiTokenIterator( const ApiTokenIterator& rIter, bool bSkipSpaces ) :
    mpToken( rIter.mpToken ),
    mpTokenEnd( rIter.mpTokenEnd ),
    mnSpacesOpCode( rIter.mnSpacesOpCode ),
    mbSkipSpaces( bSkipSpaces )
{
    skipSpaces();
}

ApiTokenIterator& ApiTokenIterator::operator++()
{
    if( is() )
    {
        ++mpToken;
        skipSpaces();
    }
    return *this;
}

void ApiTokenIterator::skipSpaces()
{
    if( mbSkipSpaces )
        while( is() && (mpToken->OpCode == mnSpacesOpCode) )
            ++mpToken;
}

// formula parser/formula compiler base class =================================

namespace {

bool lclConvertToCellAddress( CellAddress& orAddress, const SingleReference& rSingleRef, sal_Int32 nExpectedSheet )
{
    orAddress = CellAddress( static_cast< sal_Int16 >( rSingleRef.Sheet ),
        rSingleRef.Column, rSingleRef.Row );
    return
        ((nExpectedSheet < 0) || (nExpectedSheet == rSingleRef.Sheet)) &&
        !getFlag( rSingleRef.Flags, COLUMN_DELETED | ROW_DELETED | SHEET_DELETED );
}

bool lclConvertToCellRange( CellRangeAddress& orRange, const ComplexReference& rComplexRef, sal_Int32 nExpectedSheet )
{
    orRange = CellRangeAddress( static_cast< sal_Int16 >( rComplexRef.Reference1.Sheet ),
        rComplexRef.Reference1.Column, rComplexRef.Reference1.Row,
        rComplexRef.Reference2.Column, rComplexRef.Reference2.Row );
    return
        (rComplexRef.Reference1.Sheet == rComplexRef.Reference2.Sheet) &&
        ((nExpectedSheet < 0) || (nExpectedSheet == rComplexRef.Reference1.Sheet)) &&
        !getFlag( rComplexRef.Reference1.Flags, COLUMN_DELETED | ROW_DELETED | SHEET_DELETED ) &&
        !getFlag( rComplexRef.Reference2.Flags, COLUMN_DELETED | ROW_DELETED | SHEET_DELETED );
}

enum TokenToRangeListState { STATE_REF, STATE_SEP, STATE_OPEN, STATE_CLOSE, STATE_ERROR };

TokenToRangeListState lclProcessRef( ::std::vector< CellRangeAddress >& orRanges, const Any& rData, sal_Int32 nExpectedSheet )
{
    SingleReference aSingleRef;
    if( rData >>= aSingleRef )
    {
        CellAddress aAddress;
        // ignore invalid addresses (with #REF! errors), but to not stop parsing
        if( lclConvertToCellAddress( aAddress, aSingleRef, nExpectedSheet ) )
            orRanges.push_back( CellRangeAddress( aAddress.Sheet, aAddress.Column, aAddress.Row, aAddress.Column, aAddress.Row ) );
        return STATE_REF;
    }
    ComplexReference aComplexRef;
    if( rData >>= aComplexRef )
    {
        CellRangeAddress aRange;
        // ignore invalid ranges (with #REF! errors), but to not stop parsing
        if( lclConvertToCellRange( aRange, aComplexRef, nExpectedSheet ) )
            orRanges.push_back( aRange );
        return STATE_REF;
    }
    return STATE_ERROR;
}

TokenToRangeListState lclProcessOpen( sal_Int32& ornParenLevel )
{
    ++ornParenLevel;
    return STATE_OPEN;
}

TokenToRangeListState lclProcessClose( sal_Int32& ornParenLevel )
{
    --ornParenLevel;
    return (ornParenLevel >= 0) ? STATE_CLOSE : STATE_ERROR;
}

} // namespace

// ----------------------------------------------------------------------------

FormulaProcessorBase::FormulaProcessorBase( const GlobalDataHelper& rGlobalData ) :
    GlobalDataHelper( rGlobalData ),
    maFuncProv( rGlobalData )
{
}

Sequence< CellRangeAddress > FormulaProcessorBase::convertToCellRangeList(
        const ApiTokenSequence& rTokens, sal_Int32 nExpectedSheet )
{
    ::std::vector< CellRangeAddress > aRanges;
    TokenToRangeListState eState = STATE_OPEN;
    sal_Int32 nParenLevel = 0;
    for( ApiTokenIterator aIt( rTokens, maFuncProv.OPCODE_SPACES, true ); aIt.is() && (eState != STATE_ERROR); ++aIt )
    {
        sal_Int32 nOpCode = aIt->OpCode;
        switch( eState )
        {
            case STATE_REF:
                     if( nOpCode == maFuncProv.OPCODE_LIST )    eState = STATE_SEP;
                else if( nOpCode == maFuncProv.OPCODE_CLOSE )   eState = lclProcessClose( nParenLevel );
                else                                            eState = STATE_ERROR;
            break;
            case STATE_SEP:
                     if( nOpCode == maFuncProv.OPCODE_PUSH )    eState = lclProcessRef( aRanges, aIt->Data, nExpectedSheet );
                else if( nOpCode == maFuncProv.OPCODE_LIST )    eState = STATE_SEP;
                else if( nOpCode == maFuncProv.OPCODE_OPEN )    eState = lclProcessOpen( nParenLevel );
                else if( nOpCode == maFuncProv.OPCODE_CLOSE )   eState = lclProcessClose( nParenLevel );
                else                                            eState = STATE_ERROR;
            break;
            case STATE_OPEN:
                     if( nOpCode == maFuncProv.OPCODE_PUSH )    eState = lclProcessRef( aRanges, aIt->Data, nExpectedSheet );
                else if( nOpCode == maFuncProv.OPCODE_LIST )    eState = STATE_SEP;
                else if( nOpCode == maFuncProv.OPCODE_OPEN )    eState = lclProcessOpen( nParenLevel );
                else if( nOpCode == maFuncProv.OPCODE_CLOSE )   eState = lclProcessClose( nParenLevel );
                else                                            eState = STATE_ERROR;
            break;
            case STATE_CLOSE:
                     if( nOpCode == maFuncProv.OPCODE_LIST )    eState = STATE_SEP;
                else if( nOpCode == maFuncProv.OPCODE_CLOSE )   eState = lclProcessClose( nParenLevel );
                else                                            eState = STATE_ERROR;
            break;
            default:;
        }
    }

    if( eState != STATE_ERROR )
    {
        getAddressConverter().validateCellRangeList( aRanges, false );
        return ContainerHelper::vectorToSequence( aRanges );
    }
    return Sequence< CellRangeAddress >();
}

// ============================================================================

} // namespace xls
} // namespace oox
