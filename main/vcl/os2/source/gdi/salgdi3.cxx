/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#define INCL_GRE_STRINGS
#define INCL_GPI
#define INCL_DOS

#include <string.h>
//#include <stdlib.h>
//#include <math.h>
#include <svpm.h>

#include <vcl/sysdata.hxx>
#include "tools/svwin.h"

#include "os2/saldata.hxx"
#include "os2/salgdi.h"

#include "vcl/svapp.hxx"
#include "outfont.hxx"
#include "vcl/font.hxx"
#include "fontsubset.hxx"
#include "sallayout.hxx"

#include "rtl/logfile.hxx"
#include "rtl/tencinfo.h"
#include "rtl/textcvt.h"
#include "rtl/bootstrap.hxx"


#include "osl/module.h"
#include "osl/file.hxx"
#include "osl/thread.hxx"
#include "osl/process.h"

#include "tools/poly.hxx"
#include "tools/debug.hxx"
#include "tools/stream.hxx"

#include "basegfx/polygon/b2dpolygon.hxx"
#include "basegfx/polygon/b2dpolypolygon.hxx"
#include "basegfx/matrix/b2dhommatrix.hxx"

#ifndef __H_FT2LIB
#include <os2/wingdi.h>
#include <ft2lib.h>
#endif

#include "sft.hxx"

#ifdef GCP_KERN_HACK
#include <algorithm>
#endif

using namespace vcl;

// -----------
// - Inlines -
// -----------


inline FIXED_W FixedFromDouble( double d )
{
    const long l = (long) ( d * 65536. );
    return *(FIXED_W*) &l;
}

// -----------------------------------------------------------------------

inline int IntTimes256FromFixed(FIXED_W f)
{
    int nFixedTimes256 = (f.value << 8) + ((f.fract+0x80) >> 8);
    return nFixedTimes256;
}

// -----------
// - Defines -
// -----------

// this is a special codepage code, used to identify OS/2 symbol font.
#define SYMBOL_CHARSET					65400

// =======================================================================

UniString ImplSalGetUniString( const sal_Char* pStr, xub_StrLen nLen = STRING_LEN)
{
	return UniString( pStr, nLen, gsl_getSystemTextEncoding(),
					  RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_DEFAULT |
					  RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_DEFAULT |
					  RTL_TEXTTOUNICODE_FLAGS_INVALID_DEFAULT );
}

// =======================================================================

static USHORT ImplSalToCharSet( CharSet eCharSet )
{
	// !!! Fuer DBCS-Systeme muss dieser Code auskommentiert werden und 0
	// !!! zurueckgegeben werden, solange die DBCS-Charsets nicht
	// !!! durchgereicht werden

	switch ( eCharSet )
	{
		case RTL_TEXTENCODING_IBM_437:
			return 437;

		case RTL_TEXTENCODING_IBM_850:
			return 850;

		case RTL_TEXTENCODING_IBM_860:
			return 860;

		case RTL_TEXTENCODING_IBM_861:
			return 861;

		case RTL_TEXTENCODING_IBM_863:
			return 863;

		case RTL_TEXTENCODING_IBM_865:
			return 865;
		case RTL_TEXTENCODING_MS_1252:
			return 1004;
		case RTL_TEXTENCODING_SYMBOL:
			return 65400;
	}

	return 0;
}

// -----------------------------------------------------------------------

static CharSet ImplCharSetToSal( USHORT usCodePage )
{
	switch ( usCodePage )
	{
		case 437:
			return RTL_TEXTENCODING_IBM_437;

		case 850:
			return RTL_TEXTENCODING_IBM_850;

		case 860:
			return RTL_TEXTENCODING_IBM_860;

		case 861:
			return RTL_TEXTENCODING_IBM_861;

		case 863:
			return RTL_TEXTENCODING_IBM_863;

		case 865:
			return RTL_TEXTENCODING_IBM_865;
		case 1004:
			return RTL_TEXTENCODING_MS_1252;
		case 65400:
			return RTL_TEXTENCODING_SYMBOL;
	}

	return RTL_TEXTENCODING_DONTKNOW;
}

// -----------------------------------------------------------------------

static FontFamily ImplFamilyToSal( PM_BYTE bFamilyType )
{
    switch ( bFamilyType )
    {
        case 4:
            return FAMILY_DECORATIVE;
        case 3:
            return FAMILY_SCRIPT;
    }

    return FAMILY_DONTKNOW;
}

// -----------------------------------------------------------------------

static FontWeight ImplWeightToSal( USHORT nWeight )
{
	// Falls sich jemand an die alte Doku gehalten hat
	if ( nWeight > 999 )
		nWeight /= 1000;

	switch ( nWeight )
	{
		case 1:
			return WEIGHT_THIN;

		case 2:
			return WEIGHT_ULTRALIGHT;

		case 3:
			return WEIGHT_LIGHT;

		case 4:
			return WEIGHT_SEMILIGHT;

		case 5:
			return WEIGHT_NORMAL;

		case 6:
			return WEIGHT_SEMIBOLD;

		case 7:
			return WEIGHT_BOLD;

		case 8:
			return WEIGHT_ULTRABOLD;

		case 9:
			return WEIGHT_BLACK;
	}

	return WEIGHT_DONTKNOW;
}

// -----------------------------------------------------------------------

static UniString ImpStyleNameToSal( const char* pFamilyName,
								   const char* pFaceName,
								   USHORT nLen )
{
	if ( !nLen )
		nLen = strlen(pFamilyName);

	// strip FamilyName from FaceName
	if ( strncmp( pFamilyName, pFaceName, nLen ) == 0 )
	{
		USHORT nFaceLen = (USHORT)strlen( pFaceName+nLen );
		// Ist Facename laenger, schneiden wir den FamilyName ab
		if ( nFaceLen > 1 )
			return UniString( pFaceName+(nLen+1), gsl_getSystemTextEncoding());
		else
			return UniString();
	}
	else
		return UniString( pFaceName, gsl_getSystemTextEncoding());
}

// -----------------------------------------------------------------------

inline FontPitch ImplLogPitchToSal( PM_BYTE fsType )
{
    if ( fsType & FM_TYPE_FIXED )
        return PITCH_FIXED;
    else
        return PITCH_VARIABLE;
}

// -----------------------------------------------------------------------

inline PM_BYTE ImplPitchToWin( FontPitch ePitch )
{
    if ( ePitch == PITCH_FIXED )
        return FM_TYPE_FIXED;
    //else if ( ePitch == PITCH_VARIABLE )

	return 0;
}

// -----------------------------------------------------------------------

static ImplDevFontAttributes Os2Font2DevFontAttributes( const PFONTMETRICS pFontMetric)
{
    ImplDevFontAttributes aDFA;

    // get font face attributes
    aDFA.meFamily       = ImplFamilyToSal( pFontMetric->panose.bFamilyType);
    aDFA.meWidthType    = WIDTH_DONTKNOW;
    aDFA.meWeight       = ImplWeightToSal( pFontMetric->usWeightClass);
    aDFA.meItalic       = (pFontMetric->fsSelection & FM_SEL_ITALIC) ? ITALIC_NORMAL : ITALIC_NONE;
    aDFA.mePitch        = ImplLogPitchToSal( pFontMetric->fsType );
    aDFA.mbSymbolFlag   = (pFontMetric->usCodePage == SYMBOL_CHARSET);

	// get the font face name
	// the maName field stores the font name without the style, so under OS/2
	// we must use the family name
	aDFA.maName = UniString( pFontMetric->szFamilyname, gsl_getSystemTextEncoding());

	aDFA.maStyleName = ImpStyleNameToSal( pFontMetric->szFamilyname,
 										  pFontMetric->szFacename,
 										  strlen( pFontMetric->szFamilyname) );

    // get device specific font attributes
    aDFA.mbOrientation  = (pFontMetric->fsDefn & FM_DEFN_OUTLINE) != 0;
    aDFA.mbDevice       = (pFontMetric->fsDefn & FM_DEFN_GENERIC) ? FALSE : TRUE;

    aDFA.mbEmbeddable   = false;
    aDFA.mbSubsettable  = false;
    DWORD fontType = Ft2QueryFontType( 0, pFontMetric->szFamilyname);
    if( fontType == FT2_FONTTYPE_TRUETYPE && !aDFA.mbDevice)
		aDFA.mbSubsettable = true;
    // for now we can only embed Type1 fonts
    if( fontType == FT2_FONTTYPE_TYPE1 )
        aDFA.mbEmbeddable = true;

    // heuristics for font quality
    // -   standard-type1 > opentypeTT > truetype > non-standard-type1 > raster
    // -   subsetting > embedding > none
    aDFA.mnQuality = 0;
    if( fontType == FT2_FONTTYPE_TRUETYPE )
        aDFA.mnQuality += 50;
    if( aDFA.mbSubsettable )
        aDFA.mnQuality += 200;
    else if( aDFA.mbEmbeddable )
        aDFA.mnQuality += 100;

    // #i38665# prefer Type1 versions of the standard postscript fonts
    if( aDFA.mbEmbeddable )
    {
        if( aDFA.maName.EqualsAscii( "AvantGarde" )
        ||  aDFA.maName.EqualsAscii( "Bookman" )
        ||  aDFA.maName.EqualsAscii( "Courier" )
        ||  aDFA.maName.EqualsAscii( "Helvetica" )
        ||  aDFA.maName.EqualsAscii( "NewCenturySchlbk" )
        ||  aDFA.maName.EqualsAscii( "Palatino" )
        ||  aDFA.maName.EqualsAscii( "Symbol" )
        ||  aDFA.maName.EqualsAscii( "Times" )
        ||  aDFA.maName.EqualsAscii( "ZapfChancery" )
        ||  aDFA.maName.EqualsAscii( "ZapfDingbats" ) )
            aDFA.mnQuality += 500;
    }    

    // TODO: add alias names

    return aDFA;
}

// =======================================================================

// raw font data with a scoped lifetime
class RawFontData
{
public:
    explicit	RawFontData( HDC, DWORD nTableTag=0 );
    			~RawFontData() { delete[] mpRawBytes; }
    const unsigned char*	get() const { return mpRawBytes; }
    const unsigned char*	steal() { unsigned char* p = mpRawBytes; mpRawBytes = NULL; return p; }
    const int				size() const { return mnByteCount; }

private:
    unsigned char*	mpRawBytes;
    int				mnByteCount;
};

RawFontData::RawFontData( HPS hPS, DWORD nTableTag )
:	mpRawBytes( NULL )
,	mnByteCount( 0 )
{
	// get required size in bytes
    mnByteCount = ::Ft2GetFontData( hPS, nTableTag, 0, NULL, 0 );
    if( mnByteCount == FT2_ERROR )
        return;
    else if( !mnByteCount )
        return;

	// allocate the array
    mpRawBytes = new unsigned char[ mnByteCount ];

	// get raw data in chunks small enough for GetFontData()
	int nRawDataOfs = 0;
	DWORD nMaxChunkSize = 0x100000;
	for(;;)
	{
		// calculate remaining raw data to get
		DWORD nFDGet = mnByteCount - nRawDataOfs;
		if( nFDGet <= 0 )
			break;
		// #i56745# limit GetFontData requests
		if( nFDGet > nMaxChunkSize )
			nFDGet = nMaxChunkSize;
		const DWORD nFDGot = ::Ft2GetFontData( hPS, nTableTag, nRawDataOfs,
			(void*)(mpRawBytes + nRawDataOfs), nFDGet );
		if( !nFDGot )
			break;
		else if( nFDGot != FT2_ERROR )
			nRawDataOfs += nFDGot;
		else
		{
			// was the chunk too big? reduce it
			nMaxChunkSize /= 2;
			if( nMaxChunkSize < 0x10000 )
				break;
		}
	}

	// cleanup if the raw data is incomplete
	if( nRawDataOfs != mnByteCount )
	{
		delete[] mpRawBytes;
		mpRawBytes = NULL;
	}
}

// -----------------------------------------------------------------------

// =======================================================================

ImplOs2FontData::ImplOs2FontData( PFONTMETRICS _pFontMetric,
    int nHeight, PM_BYTE nPitchAndFamily )
:	ImplFontData( Os2Font2DevFontAttributes(_pFontMetric), 0 ),
    pFontMetric( _pFontMetric ),
	meOs2CharSet( _pFontMetric->usCodePage),
    mnPitchAndFamily( nPitchAndFamily ),
    mpFontCharSets( NULL ),
    mpUnicodeMap( NULL ),
    mbDisableGlyphApi( false ),
    mbHasKoreanRange( false ),
    mbHasCJKSupport( false ),
    mbAliasSymbolsLow( false ),
    mbAliasSymbolsHigh( false ),
    mnId( 0 )
{
    SetBitmapSize( 0, nHeight );
}

// -----------------------------------------------------------------------

ImplOs2FontData::~ImplOs2FontData()
{
    delete[] mpFontCharSets;

    if( mpUnicodeMap )
        mpUnicodeMap->DeReference();
}

// -----------------------------------------------------------------------

sal_IntPtr ImplOs2FontData::GetFontId() const
{
    return mnId;
}

// -----------------------------------------------------------------------

void ImplOs2FontData::UpdateFromHPS( HPS hPS ) const
{
    // short circuit if already initialized
    if( mpUnicodeMap != NULL )
        return;

    ReadCmapTable( hPS );
    ReadOs2Table( hPS );

    // even if the font works some fonts have problems with the glyph API
    // => the heuristic below tries to figure out which fonts have the problem
	DWORD	fontType = Ft2QueryFontType( 0, pFontMetric->szFacename);
    if( fontType != FT2_FONTTYPE_TRUETYPE 
		&& (pFontMetric->fsDefn & FM_DEFN_GENERIC) == 0)
		mbDisableGlyphApi = true;
}

// -----------------------------------------------------------------------

bool ImplOs2FontData::HasGSUBstitutions( HPS hPS ) const
{
    if( !mbGsubRead )
        ReadGsubTable( hPS );
    return !maGsubTable.empty();
}

// -----------------------------------------------------------------------

bool ImplOs2FontData::IsGSUBstituted( sal_Ucs cChar ) const
{
    return( maGsubTable.find( cChar ) != maGsubTable.end() );
}

// -----------------------------------------------------------------------

const ImplFontCharMap* ImplOs2FontData::GetImplFontCharMap() const
{
    mpUnicodeMap->AddReference();
    return mpUnicodeMap;
}

// -----------------------------------------------------------------------

static unsigned GetUInt( const unsigned char* p ) { return((p[0]<<24)+(p[1]<<16)+(p[2]<<8)+p[3]);}
static unsigned GetUShort( const unsigned char* p ){ return((p[0]<<8)+p[1]);}
static signed GetSShort( const unsigned char* p ){ return((short)((p[0]<<8)+p[1]));}
static inline DWORD CalcTag( const char p[4]) { return (p[0]+(p[1]<<8)+(p[2]<<16)+(p[3]<<24)); }

void ImplOs2FontData::ReadOs2Table( HPS hPS ) const
{
    const DWORD Os2Tag = CalcTag( "OS/2" );
    DWORD nLength = Ft2GetFontData( hPS, Os2Tag, 0, NULL, 0 );
    if( (nLength == FT2_ERROR) || !nLength )
        return;
    std::vector<unsigned char> aOS2map( nLength );
    unsigned char* pOS2map = &aOS2map[0];
    DWORD nRC = Ft2GetFontData( hPS, Os2Tag, 0, pOS2map, nLength );
    sal_uInt32 nVersion = GetUShort( pOS2map );
    if ( nVersion >= 0x0001 && nLength >= 58 )
    {
        // We need at least version 0x0001 (TrueType rev 1.66)
        // to have access to the needed struct members.
        sal_uInt32 ulUnicodeRange1 = GetUInt( pOS2map + 42 );
        sal_uInt32 ulUnicodeRange2 = GetUInt( pOS2map + 46 );
#if 0
        sal_uInt32 ulUnicodeRange3 = GetUInt( pOS2map + 50 );
        sal_uInt32 ulUnicodeRange4 = GetUInt( pOS2map + 54 );
#endif

        // Check for CJK capabilities of the current font
        mbHasCJKSupport = (ulUnicodeRange2 & 0x2DF00000);
        mbHasKoreanRange= (ulUnicodeRange1 & 0x10000000)
                        | (ulUnicodeRange2 & 0x01100000);
        mbHasArabicSupport = (ulUnicodeRange1 & 0x00002000);
    }
}


// -----------------------------------------------------------------------

void ImplOs2FontData::ReadGsubTable( HPS hPS ) const
{
    mbGsubRead = true;

    // check the existence of a GSUB table
    const DWORD GsubTag = CalcTag( "GSUB" );
    DWORD nRC = Ft2GetFontData( hPS, GsubTag, 0, NULL, 0 );
    if( (nRC == FT2_ERROR) || !nRC )
        return;

    // parse the GSUB table through sft
    // TODO: parse it directly

    // sft needs the full font file data => get it
    const RawFontData aRawFontData( hPS );
    if( !aRawFontData.get() )
    	return;

    // open font file
    sal_uInt32 nFaceNum = 0;
    if( !*aRawFontData.get() )  // TTC candidate
        nFaceNum = ~0U;  // indicate "TTC font extracts only"

    TrueTypeFont* pTTFont = NULL;
    ::OpenTTFontBuffer( (void*)aRawFontData.get(), aRawFontData.size(), nFaceNum, &pTTFont );
    if( !pTTFont )
        return;

    // add vertically substituted characters to list
    static const sal_Unicode aGSUBCandidates[] = {
        0x0020, 0x0080, // ASCII
        0x2000, 0x2600, // misc
        0x3000, 0x3100, // CJK punctutation
        0x3300, 0x3400, // squared words
        0xFF00, 0xFFF0, // halfwidth|fullwidth forms
    0 };

    for( const sal_Unicode* pPair = aGSUBCandidates; *pPair; pPair += 2 )
        for( sal_Unicode cChar = pPair[0]; cChar < pPair[1]; ++cChar )
            if( ::MapChar( pTTFont, cChar, 0 ) != ::MapChar( pTTFont, cChar, 1 ) )
                maGsubTable.insert( cChar ); // insert GSUBbed unicodes

    CloseTTFont( pTTFont );
}

// -----------------------------------------------------------------------

void ImplOs2FontData::ReadCmapTable( HPS hPS ) const
{
    if( mpUnicodeMap != NULL )
        return;

    bool bIsSymbolFont = (meOs2CharSet == SYMBOL_CHARSET);
    // get the CMAP table from the font which is selected into the DC
    const DWORD nCmapTag = CalcTag( "cmap" );
    const RawFontData aRawFontData( hPS, nCmapTag );
    // parse the CMAP table if available
    if( aRawFontData.get() ) {
        CmapResult aResult;
        ParseCMAP( aRawFontData.get(), aRawFontData.size(), aResult );
        mbDisableGlyphApi |= aResult.mbRecoded;
        aResult.mbSymbolic = bIsSymbolFont;
        if( aResult.mnRangeCount > 0 )
            mpUnicodeMap = new ImplFontCharMap( aResult );
    }

    if( !mpUnicodeMap )
        mpUnicodeMap = ImplFontCharMap::GetDefaultMap( bIsSymbolFont );
    mpUnicodeMap->AddReference();
}

// =======================================================================

void Os2SalGraphics::SetTextColor( SalColor nSalColor )
{
	CHARBUNDLE cb;

	cb.lColor = MAKE_SALCOLOR( SALCOLOR_RED( nSalColor ),
						  SALCOLOR_GREEN( nSalColor ),
						  SALCOLOR_BLUE( nSalColor ) );

	// set default color attributes
	Ft2SetAttrs( mhPS,
				 PRIM_CHAR,
				 CBB_COLOR,
				 0,
				 &cb );
}

// -----------------------------------------------------------------------

USHORT Os2SalGraphics::ImplDoSetFont( ImplFontSelectData* i_pFont, float& o_rFontScale, int nFallbackLevel)
{

#if OSL_DEBUG_LEVEL>10
    debug_printf( "Os2SalGraphics::ImplDoSetFont mbPrinter=%d\n", mbPrinter);
#endif

	ImplOs2FontData* pFontData = (ImplOs2FontData*)i_pFont->mpFontData;
	PFONTMETRICS 	pFontMetric = NULL;
	FATTRS		  	aFAttrs;
	PM_BOOL		  	bOutline = FALSE;
	APIRET			rc;			

	memset( &aFAttrs, 0, sizeof( FATTRS ) );
	aFAttrs.usRecordLength = sizeof( FATTRS );

	aFAttrs.lMaxBaselineExt = i_pFont->mnHeight;
	aFAttrs.lAveCharWidth	= i_pFont->mnWidth;

	// do we have a pointer to the FONTMETRICS of the selected font? -> use it!
	if ( pFontData )
	{
		pFontMetric = pFontData->GetFontMetrics();

		bOutline = (pFontMetric->fsDefn & FM_DEFN_OUTLINE) != 0;

		// use match&registry fields to get correct match
		aFAttrs.lMatch	     	= pFontMetric->lMatch;
		aFAttrs.idRegistry 		= pFontMetric->idRegistry;			
		aFAttrs.usCodePage 		= pFontMetric->usCodePage;

		if ( bOutline )
		{
			aFAttrs.fsFontUse |= FATTR_FONTUSE_OUTLINE;
			if ( i_pFont->mnOrientation )
				aFAttrs.fsFontUse |= FATTR_FONTUSE_TRANSFORMABLE;
		}
		else
		{
			aFAttrs.lMaxBaselineExt = pFontMetric->lMaxBaselineExt;
			aFAttrs.lAveCharWidth	= pFontMetric->lAveCharWidth;
		}

	}
#if OSL_DEBUG_LEVEL>1
	if (pFontMetric->szFacename[0] == 'D') {
		rc = 0; // debugger breakpoint
	}
#endif

	// use family name for outline fonts
    if ( mbPrinter ) {
		// use font face name for printers because otherwise ft2lib will fail 
		// to select the correct font for GPI (ticket#117)
		strncpy( (char*)(aFAttrs.szFacename), pFontMetric->szFacename, sizeof( aFAttrs.szFacename ) );
	} else if ( !pFontMetric) {
		// use OOo name if fontmetrics not available!
		ByteString aName( i_pFont->maName.GetToken( 0 ), gsl_getSystemTextEncoding());
		strncpy( (char*)(aFAttrs.szFacename), aName.GetBuffer(), sizeof( aFAttrs.szFacename ) );
	} else if ( bOutline) {
		// use fontmetric family name for outline fonts
		strncpy( (char*)(aFAttrs.szFacename), pFontMetric->szFamilyname, sizeof( aFAttrs.szFacename ) );
	} else {
		// use real font face name for bitmaps (WarpSans only)
		strncpy( (char*)(aFAttrs.szFacename), pFontMetric->szFacename, sizeof( aFAttrs.szFacename ) );
	}

	if ( i_pFont->meItalic != ITALIC_NONE )
		aFAttrs.fsSelection |= FATTR_SEL_ITALIC;
	if ( i_pFont->meWeight > WEIGHT_MEDIUM )
		aFAttrs.fsSelection |= FATTR_SEL_BOLD;

#if OSL_DEBUG_LEVEL>1
	if (pFontMetric->szFacename[0] == 'A') {
		debug_printf( "Os2SalGraphics::SetFont hps %x lMatch '%d'\n", mhPS, pFontMetric->lMatch);
		debug_printf( "Os2SalGraphics::SetFont hps %x fontmetrics facename '%s'\n", mhPS, pFontMetric->szFacename);
		debug_printf( "Os2SalGraphics::SetFont hps %x fattrs facename '%s'\n", mhPS, aFAttrs.szFacename);
		debug_printf( "Os2SalGraphics::SetFont hps %x fattrs height '%d'\n", mhPS, aFAttrs.lMaxBaselineExt);
		debug_printf( "Os2SalGraphics::SetFont hps %x fattrs width '%d'\n", mhPS, aFAttrs.lAveCharWidth);
	}
#endif

	// set default font
	rc = Ft2SetCharSet( mhPS, LCID_DEFAULT);
	// delete selected font
	rc = Ft2DeleteSetId( mhPS, nFallbackLevel + LCID_BASE);

	// create new logical font
	if ( (rc=Ft2CreateLogFont( mhPS, NULL, nFallbackLevel + LCID_BASE, &aFAttrs)) == GPI_ERROR ) {
#if OSL_DEBUG_LEVEL>1
		ERRORID nLastError = WinGetLastError( GetSalData()->mhAB );
    	debug_printf( "Os2SalGraphics::SetFont hps %x Ft2CreateLogFont failed err %x\n", mhPS, nLastError );
#endif
		return SAL_SETFONT_REMOVEANDMATCHNEW;
	}

#if OSL_DEBUG_LEVEL>1
	// select new font
	rc = Ft2SetCharSet( mhPS, nFallbackLevel + LCID_BASE);

	// query fontmetric of new font
	FONTMETRICS aOS2Metric = {0};
	rc = Ft2QueryFontMetrics( mhPS, sizeof( aOS2Metric ), &aOS2Metric );

	if (pFontMetric->szFacename[0] == 'D') {
		debug_printf( "Os2SalGraphics::SetFont Ft2QueryFontMetrics fattrs facename '%s'\n", aOS2Metric.szFacename);
		debug_printf( "Os2SalGraphics::SetFont Ft2QueryFontMetrics fattrs height '%d'\n", aOS2Metric.lMaxBaselineExt);
		debug_printf( "Os2SalGraphics::SetFont Ft2QueryFontMetrics fattrs width '%d'\n", aOS2Metric.lAveCharWidth);
	}
#endif

	// apply font sizing, rotation
	CHARBUNDLE aBundle;

	ULONG nAttrsDefault = 0;
	ULONG nAttrs = CBB_SET;
	aBundle.usSet = nFallbackLevel + LCID_BASE;

	if ( bOutline )
	{
		nAttrs |= CBB_BOX;
		aBundle.sizfxCell.cy = MAKEFIXED( i_pFont->mnHeight, 0 );

		if ( !i_pFont->mnWidth )
		{
			LONG nXFontRes;
			LONG nYFontRes;
			LONG nHeight;

			// Auf die Aufloesung achten, damit das Ergebnis auch auf
			// Drucken mit 180*360 DPI stimmt. Ausserdem muss gerundet
			// werden, da auf meinem OS2 beispielsweise als
			// Bildschirmaufloesung 3618*3622 PixelPerMeter zurueck-
			// gegeben wird
			GetResolution( nXFontRes, nYFontRes );
			nHeight = i_pFont->mnHeight;
			nHeight *= nXFontRes;
			nHeight += nYFontRes/2;
			nHeight /= nYFontRes;
			aBundle.sizfxCell.cx = MAKEFIXED( nHeight, 0 );
		}
		else
			aBundle.sizfxCell.cx = MAKEFIXED( i_pFont->mnWidth, 0 );
	}

	// set orientation for outlinefonts
	if ( i_pFont->mnOrientation )
	{
		if ( bOutline )
		{
			nAttrs |= CBB_ANGLE;
			double alpha = (double)(i_pFont->mnOrientation);
			alpha *= 0.0017453292;	 // *PI / 1800
			mnOrientationY = (long) (1000.0 * sin( alpha ));
			mnOrientationX = (long) (1000.0 * cos( alpha ));
			aBundle.ptlAngle.x = mnOrientationX;
			aBundle.ptlAngle.y = mnOrientationY;
		}
		else
		{
			mnOrientationX = 1;
			mnOrientationY = 0;
			nAttrs |= CBB_ANGLE;
			aBundle.ptlAngle.x = 1;
			aBundle.ptlAngle.y = 0;
		}
	}
	else
	{
		mnOrientationX = 1;
		mnOrientationY = 0;
		nAttrs |= CBB_ANGLE;
		aBundle.ptlAngle.x = 1;
		aBundle.ptlAngle.y = 0;
	}

	rc = Ft2SetAttrs( mhPS, PRIM_CHAR, nAttrs, nAttrsDefault, &aBundle );

#if OSL_DEBUG_LEVEL>1
	rc = Ft2QueryFontMetrics( mhPS, sizeof( aOS2Metric ), &aOS2Metric );
	if (pFontMetric->szFacename[0] == 'D') {
		debug_printf( "Os2SalGraphics::SetFont Ft2QueryFontMetrics fattrs facename '%s'\n", aOS2Metric.szFacename);
		debug_printf( "Os2SalGraphics::SetFont Ft2QueryFontMetrics fattrs height '%d'\n", aOS2Metric.lMaxBaselineExt);
		debug_printf( "Os2SalGraphics::SetFont Ft2QueryFontMetrics fattrs width '%d'\n", aOS2Metric.lAveCharWidth);
	}
#endif

	return 0;
}


USHORT Os2SalGraphics::SetFont( ImplFontSelectData* pFont, int nFallbackLevel )
{

    // return early if there is no new font
    if( !pFont )
    {
#if 0
        // deselect still active font
        if( mhDefFont )
            Ft2SetCharSet( mhPS, mhDefFont );
        // release no longer referenced font handles
        for( int i = nFallbackLevel; i < MAX_FALLBACK; ++i )
        {
            if( mhFonts[i] )
                Ft2DeleteSetId( mhPS, mhFonts[i] );
            mhFonts[ i ] = 0;
        }
#endif
        mhDefFont = 0;
        return 0;
    }

#if OSL_DEBUG_LEVEL>10
    debug_printf( "Os2SalGraphics::SetFont\n");
#endif

    DBG_ASSERT( pFont->mpFontData, "WinSalGraphics mpFontData==NULL");
    mpOs2FontEntry[ nFallbackLevel ] = reinterpret_cast<ImplOs2FontEntry*>( pFont->mpFontEntry );
    mpOs2FontData[ nFallbackLevel ] = static_cast<const ImplOs2FontData*>( pFont->mpFontData );

	ImplDoSetFont( pFont, mfFontScale, nFallbackLevel);

    if( !mhDefFont )
    {
        // keep default font
        mhDefFont = nFallbackLevel + LCID_BASE;
    }
    else
    {
        // release no longer referenced font handles
        for( int i = nFallbackLevel; i < MAX_FALLBACK; ++i )
        {
            if( mhFonts[i] )
            {
#if 0
                Ft2DeleteSetId( mhPS, mhFonts[i] );
#endif
                mhFonts[i] = 0;
            }
        }
    }

    // store new font in correct layer
    mhFonts[ nFallbackLevel ] = nFallbackLevel + LCID_BASE;

    // now the font is live => update font face
    if( mpOs2FontData[ nFallbackLevel ] )
        mpOs2FontData[ nFallbackLevel ]->UpdateFromHPS( mhPS );

    if( !nFallbackLevel )
    {
        mbFontKernInit = TRUE;
        if ( mpFontKernPairs )
        {
            delete[] mpFontKernPairs;
            mpFontKernPairs = NULL;
        }
        mnFontKernPairCount = 0;
    }

    // some printers have higher internal resolution, so their
    // text output would be different from what we calculated
    // => suggest DrawTextArray to workaround this problem
    if ( mbPrinter )
        return SAL_SETFONT_USEDRAWTEXTARRAY;
    else
        return 0;
}

// -----------------------------------------------------------------------

void Os2SalGraphics::GetFontMetric( ImplFontMetricData* pMetric, int nFallbackLevel )
{
	FONTMETRICS aOS2Metric;
	Ft2QueryFontMetrics( mhPS, sizeof( aOS2Metric ), &aOS2Metric );

	// @TODO sync , int nFallbackLevel in 340

#if OSL_DEBUG_LEVEL>1
	debug_printf( "Os2SalGraphics::GetFontMetric hps %x\n", mhPS);
	if (aOS2Metric.szFacename[0] == 'A') {
		debug_printf( "Os2SalGraphics::GetFontMetric hps %x fontmetrics facename '%s'\n", mhPS, aOS2Metric.szFacename);
		debug_printf( "Os2SalGraphics::GetFontMetric hps %x fontmetrics lMatch '%d'\n", mhPS, aOS2Metric.lMatch);
	}
#endif

	pMetric->maName 			= UniString( aOS2Metric.szFamilyname, gsl_getSystemTextEncoding());
	pMetric->maStyleName		= ImpStyleNameToSal( aOS2Metric.szFamilyname,
													 aOS2Metric.szFacename,
													 strlen( aOS2Metric.szFamilyname ) );

    // device independent font attributes
    pMetric->meFamily       = ImplFamilyToSal( aOS2Metric.panose.bFamilyType);
    pMetric->mbSymbolFlag   = (aOS2Metric.usCodePage == SYMBOL_CHARSET);
    pMetric->meWeight       = ImplWeightToSal( aOS2Metric.usWeightClass );
    pMetric->mePitch        = ImplLogPitchToSal( aOS2Metric.fsType );
    pMetric->meItalic       = (aOS2Metric.fsSelection & FM_SEL_ITALIC) ? ITALIC_NORMAL : ITALIC_NONE;
    pMetric->mnSlant        = 0;

    // device dependend font attributes
    pMetric->mbDevice       = (aOS2Metric.fsDefn & FM_DEFN_GENERIC) ? FALSE : TRUE;
    pMetric->mbScalableFont = (aOS2Metric.fsDefn & FM_DEFN_OUTLINE) ? true : false;
    if( pMetric->mbScalableFont )
    {
        // check if there are kern pairs
        // TODO: does this work with GPOS kerning?
        pMetric->mbKernableFont = (aOS2Metric.sKerningPairs > 0);
    }
    else
    {
        // bitmap fonts cannot be rotated directly
        pMetric->mnOrientation  = 0;
        // bitmap fonts have no kerning
        pMetric->mbKernableFont = false;
    }

    // transformation dependend font metrics
	if ( aOS2Metric.fsDefn & FM_DEFN_OUTLINE )
	{
		pMetric->mnWidth	   = aOS2Metric.lEmInc;
	}
	else
	{
		pMetric->mnWidth	   = aOS2Metric.lAveCharWidth;
		pMetric->mnOrientation = 0;
	}
	pMetric->mnIntLeading		= aOS2Metric.lInternalLeading;
	pMetric->mnExtLeading		= aOS2Metric.lExternalLeading;
	pMetric->mnAscent			= aOS2Metric.lMaxAscender;
	pMetric->mnDescent			= aOS2Metric.lMaxDescender;

    // #107888# improved metric compatibility for Asian fonts...
    // TODO: assess workaround below for CWS >= extleading
    // TODO: evaluate use of aWinMetric.sTypo* members for CJK
    if( mpOs2FontData[0] && mpOs2FontData[0]->SupportsCJK() )
    {
        pMetric->mnIntLeading += pMetric->mnExtLeading;

        // #109280# The line height for Asian fonts is too small.
        // Therefore we add half of the external leading to the
        // ascent, the other half is added to the descent.
        const long nHalfTmpExtLeading = pMetric->mnExtLeading / 2;
        const long nOtherHalfTmpExtLeading = pMetric->mnExtLeading - nHalfTmpExtLeading;

        // #110641# external leading for Asian fonts.
        // The factor 0.3 has been confirmed with experiments.
        long nCJKExtLeading = static_cast<long>(0.30 * (pMetric->mnAscent + pMetric->mnDescent));
        nCJKExtLeading -= pMetric->mnExtLeading;
        pMetric->mnExtLeading = (nCJKExtLeading > 0) ? nCJKExtLeading : 0;

        pMetric->mnAscent   += nHalfTmpExtLeading;
        pMetric->mnDescent  += nOtherHalfTmpExtLeading;

        // #109280# HACK korean only: increase descent for wavelines and impr
		// YD win9x only
    }

}

// -----------------------------------------------------------------------

ULONG Os2SalGraphics::GetKernPairs( ULONG nPairs, ImplKernPairData* pKernPairs )
{
	DBG_ASSERT( sizeof( KERNINGPAIRS ) == sizeof( ImplKernPairData ),
				"Os2SalGraphics::GetKernPairs(): KERNINGPAIRS != ImplKernPairData" );

    if ( mbFontKernInit )
    {
        if( mpFontKernPairs )
        {
            delete[] mpFontKernPairs;
            mpFontKernPairs = NULL;
        }
        mnFontKernPairCount = 0;

        {
            KERNINGPAIRS* pPairs = NULL;
			FONTMETRICS aOS2Metric;
			Ft2QueryFontMetrics( mhPS, sizeof( aOS2Metric ), &aOS2Metric );
            int nCount = aOS2Metric.sKerningPairs;
            if( nCount )
            {
#ifdef GCP_KERN_HACK
                pPairs = new KERNINGPAIRS[ nCount+1 ];
                mpFontKernPairs = pPairs;
                mnFontKernPairCount = nCount;
				Ft2QueryKerningPairs( mhPS, nCount, (KERNINGPAIRS*)pPairs );
#else // GCP_KERN_HACK
				pPairs = (KERNINGPAIRS*)pKernPairs;
				nCount = (nCount < nPairs) ? nCount : nPairs;
				Ft2QueryKerningPairs( mhPS, nCount, (KERNINGPAIRS*)pPairs );
                return nCount;
#endif // GCP_KERN_HACK
            }
        }

        mbFontKernInit = FALSE;

        std::sort( mpFontKernPairs, mpFontKernPairs + mnFontKernPairCount, ImplCmpKernData );
    }

    if( !pKernPairs )
        return mnFontKernPairCount;
    else if( mpFontKernPairs )
    {
        if ( nPairs < mnFontKernPairCount )
            nPairs = mnFontKernPairCount;
        memcpy( pKernPairs, mpFontKernPairs,
                nPairs*sizeof( ImplKernPairData ) );
        return nPairs;
    }

    return 0;
}


// -----------------------------------------------------------------------

static const ImplFontCharMap* pOs2DefaultImplFontCharMap = NULL;
static const sal_uInt32 pOs2DefaultRangeCodes[] = {0x0020,0x00FF};

const ImplFontCharMap* Os2SalGraphics::GetImplFontCharMap() const
{
    if( !mpOs2FontData[0] )
        return ImplFontCharMap::GetDefaultMap();
    return mpOs2FontData[0]->GetImplFontCharMap();
}

// -----------------------------------------------------------------------

bool Os2SalGraphics::AddTempDevFont( ImplDevFontList* pFontList,
    const String& rFontFileURL, const String& rFontName )
{
#if OSL_DEBUG_LEVEL>0
    debug_printf("Os2SalGraphics::AddTempDevFont\n");
#endif
    return false;
}

// -----------------------------------------------------------------------

void Os2SalGraphics::GetDevFontList( ImplDevFontList* pList )
{
	PFONTMETRICS	pFontMetrics;
	ULONG			nFontMetricCount;
	SalData*		pSalData;

#if OSL_DEBUG_LEVEL>0
	debug_printf("Os2SalGraphics::GetDevFontList mbPrinter=%d\n", mbPrinter);
#endif

	// install OpenSymbol
	HMODULE hMod;
	ULONG	ObjNum, Offset, rc;
	CHAR	Buff[2*_MAX_PATH];
	char 	drive[_MAX_DRIVE], dir[_MAX_DIR];
	char 	fname[_MAX_FNAME], ext[_MAX_EXT];
	// get module handle (and name)
	rc = DosQueryModFromEIP( &hMod, &ObjNum, sizeof( Buff), Buff, 
							&Offset, (ULONG)ImplSalGetUniString);
	DosQueryModuleName(hMod, sizeof(Buff), Buff);
	// replace module path with font path
	char* slash = strrchr( Buff, '\\');
	*slash = '\0';
	slash = strrchr( Buff, '\\');
	*slash = '\0';
	strcat( Buff, "\\FONTS\\OPENS___.TTF");
	rc = GpiLoadPublicFonts( GetSalData()->mhAB, Buff);

	if ( !mbPrinter )
	{
		// Bei Bildschirm-Devices cachen wir die Liste global, da
		// dies im unabhaengigen Teil auch so gemacht wird und wir
		// ansonsten auf geloeschten Systemdaten arbeiten koennten
		pSalData = GetSalData();
		nFontMetricCount	= pSalData->mnFontMetricCount;
		pFontMetrics		= pSalData->mpFontMetrics;
		// Bei Bildschirm-Devices holen wir uns die Fontliste jedesmal neu
		if ( pFontMetrics )
		{
			delete pFontMetrics;
			pFontMetrics		= NULL;
			nFontMetricCount	= 0;
		}
	}
	else
	{
		nFontMetricCount	= mnFontMetricCount;
		pFontMetrics		= mpFontMetrics;
	}

	// do we have to create the cached font list first?
	if ( !pFontMetrics )
	{
		// query the number of fonts available
		LONG nTemp = 0;
		nFontMetricCount = Ft2QueryFonts( mhPS,
										  QF_PUBLIC | QF_PRIVATE | QF_NO_DEVICE,
										  NULL, &nTemp,
										  sizeof( FONTMETRICS ), NULL );

		// procede only if at least one is available!
		if ( nFontMetricCount )
		{
			// allocate memory for font list
			pFontMetrics = new FONTMETRICS[nFontMetricCount];

			// query font list
			Ft2QueryFonts( mhPS,
						   QF_PUBLIC | QF_PRIVATE | QF_NO_DEVICE,
						   NULL,
						   (PLONG)&nFontMetricCount,
						   (LONG) sizeof( FONTMETRICS ),
						   pFontMetrics );
		}

		if ( !mbPrinter )
		{
			pSalData->mnFontMetricCount 		= nFontMetricCount;
			pSalData->mpFontMetrics 			= pFontMetrics;
		}
		else
		{
			mnFontMetricCount	= nFontMetricCount;
			mpFontMetrics		= pFontMetrics;
		}
	}

	// copy data from the font list
	for( ULONG i = 0; i < nFontMetricCount; i++ )
	{
		PFONTMETRICS pFontMetric = &pFontMetrics[i];

#if OSL_DEBUG_LEVEL>2
		debug_printf("Os2SalGraphics::GetDevFontList #%d,'%s'\n", i, pFontMetric->szFacename);
#endif

		// skip font starting with '@', this is an alias internally
		// used by truetype engine.
		if (pFontMetric->szFacename[0] == '@')
			continue;

		// skip bitmap fonts (but keep WarpSans)
		if ( (pFontMetric->fsDefn & FM_DEFN_OUTLINE) == 0 
			&& strncmp( pFontMetric->szFacename, "WarpSans", 8) )
			// Font nicht aufnehmen
			continue;

		// replace '-' in facename with ' ' (for ft2lib)
		char* dash = pFontMetric->szFacename;
		while( (dash=strchr( dash, '-')))
			*dash++ = ' ';

		// create new font list element
		ImplOs2FontData* pData 		= new ImplOs2FontData( pFontMetric, 0, 0 );

		// ticket#80: font id field is used for pdf font cache code.
		pData->SetFontId( i);

		// add font list element to font list
		pList->Add( pData );

	}
}

// ----------------------------------------------------------------------------

void Os2SalGraphics::GetDevFontSubstList( OutputDevice* pOutDev )
{
}

// -----------------------------------------------------------------------

bool Os2SalGraphics::GetGlyphBoundRect( sal_GlyphId aGlyphId, Rectangle& rRect )
{
    // use unity matrix
    MAT2 aMat;
    aMat.eM11 = aMat.eM22 = FixedFromDouble( 1.0 );
    aMat.eM12 = aMat.eM21 = FixedFromDouble( 0.0 );

    UINT nGGOFlags = GGO_METRICS;
    if( !(aGlyphId & GF_ISCHAR) )
        nGGOFlags |= GGO_GLYPH_INDEX;
    aGlyphId &= GF_IDXMASK;

    GLYPHMETRICS aGM;
    DWORD nSize = FT2_ERROR;
    nSize = Ft2GetGlyphOutline( mhPS, aGlyphId, nGGOFlags, &aGM, 0, NULL, &aMat );
    if( nSize == FT2_ERROR )
        return false;

    rRect = Rectangle( Point( +aGM.gmptGlyphOrigin.x, -aGM.gmptGlyphOrigin.y ),
        Size( aGM.gmBlackBoxX, aGM.gmBlackBoxY ) );
    rRect.Left()    = static_cast<int>( mfFontScale * rRect.Left() );
    rRect.Right()   = static_cast<int>( mfFontScale * rRect.Right() );
    rRect.Top()     = static_cast<int>( mfFontScale * rRect.Top() );
    rRect.Bottom()  = static_cast<int>( mfFontScale * rRect.Bottom() );
    return true;
}

// -----------------------------------------------------------------------

bool Os2SalGraphics::GetGlyphOutline( sal_GlyphId aGlyphId, ::basegfx::B2DPolyPolygon& rB2DPolyPoly )
{
#if OSL_DEBUG_LEVEL>0
	debug_printf("Os2SalGraphics::GetGlyphOutline\n");
#endif
    rB2DPolyPoly.clear();

    PM_BOOL bRet = FALSE;

    // use unity matrix
    MAT2 aMat;
    aMat.eM11 = aMat.eM22 = FixedFromDouble( 1.0 );
    aMat.eM12 = aMat.eM21 = FixedFromDouble( 0.0 );

    UINT nGGOFlags = GGO_NATIVE;
    if( !(aGlyphId & GF_ISCHAR) )
        nGGOFlags |= GGO_GLYPH_INDEX;
    aGlyphId &= GF_IDXMASK;

    GLYPHMETRICS aGlyphMetrics;
    DWORD nSize1 = FT2_ERROR;
    nSize1 = Ft2GetGlyphOutline( mhPS, aGlyphId, nGGOFlags, &aGlyphMetrics, 0, NULL, &aMat );

    if( !nSize1 )       // blank glyphs are ok
        bRet = TRUE;
    else if( nSize1 != FT2_ERROR )
    {
        PM_BYTE*   pData = new PM_BYTE[ nSize1 ];
        ULONG   nTotalCount = 0;
        DWORD   nSize2;
        nSize2 = Ft2GetGlyphOutline( mhPS, aGlyphId, nGGOFlags,
                &aGlyphMetrics, nSize1, pData, &aMat );

        if( nSize1 == nSize2 )
        {
            bRet = TRUE;

            int     nPtSize = 512;
            Point*  pPoints = new Point[ nPtSize ];
            sal_uInt8*   pFlags = new sal_uInt8[ nPtSize ];

            TTPOLYGONHEADER* pHeader = (TTPOLYGONHEADER*)pData;
            while( (PM_BYTE*)pHeader < pData+nSize2 )
            {
                // only outline data is interesting
                if( pHeader->dwType != TT_POLYGON_TYPE )
                    break;

                // get start point; next start points are end points
                // of previous segment
                int nPnt = 0;

                long nX = IntTimes256FromFixed( pHeader->pfxStart.x );
                long nY = IntTimes256FromFixed( pHeader->pfxStart.y );
                pPoints[ nPnt ] = Point( nX, nY );
                pFlags[ nPnt++ ] = POLY_NORMAL;

                bool bHasOfflinePoints = false;
                TTPOLYCURVE* pCurve = (TTPOLYCURVE*)( pHeader + 1 );
                pHeader = (TTPOLYGONHEADER*)( (PM_BYTE*)pHeader + pHeader->cb );
                while( (PM_BYTE*)pCurve < (PM_BYTE*)pHeader )
                {
                    int nNeededSize = nPnt + 16 + 3 * pCurve->cpfx;
                    if( nPtSize < nNeededSize )
                    {
                        Point* pOldPoints = pPoints;
                        sal_uInt8* pOldFlags = pFlags;
                        nPtSize = 2 * nNeededSize;
                        pPoints = new Point[ nPtSize ];
                        pFlags = new sal_uInt8[ nPtSize ];
                        for( int i = 0; i < nPnt; ++i )
                        {
                            pPoints[ i ] = pOldPoints[ i ];
                            pFlags[ i ] = pOldFlags[ i ];
                        }
                        delete[] pOldPoints;
                        delete[] pOldFlags;
                    }

                    int i = 0;
                    if( TT_PRIM_LINE == pCurve->wType )
                    {
                        while( i < pCurve->cpfx )
                        {
                            nX = IntTimes256FromFixed( pCurve->apfx[ i ].x );
                            nY = IntTimes256FromFixed( pCurve->apfx[ i ].y );
                            ++i;
                            pPoints[ nPnt ] = Point( nX, nY );
                            pFlags[ nPnt ] = POLY_NORMAL;
                            ++nPnt;
                        }
                    }
                    else if( TT_PRIM_QSPLINE == pCurve->wType )
                    {
                        bHasOfflinePoints = true;
                        while( i < pCurve->cpfx )
                        {
                            // get control point of quadratic bezier spline
                            nX = IntTimes256FromFixed( pCurve->apfx[ i ].x );
                            nY = IntTimes256FromFixed( pCurve->apfx[ i ].y );
                            ++i;
                            Point aControlP( nX, nY );

                            // calculate first cubic control point
                            // P0 = 1/3 * (PBeg + 2 * PQControl)
                            nX = pPoints[ nPnt-1 ].X() + 2 * aControlP.X();
                            nY = pPoints[ nPnt-1 ].Y() + 2 * aControlP.Y();
                            pPoints[ nPnt+0 ] = Point( (2*nX+3)/6, (2*nY+3)/6 );
                            pFlags[ nPnt+0 ] = POLY_CONTROL;

                            // calculate endpoint of segment
                            nX = IntTimes256FromFixed( pCurve->apfx[ i ].x );
                            nY = IntTimes256FromFixed( pCurve->apfx[ i ].y );

                            if ( i+1 >= pCurve->cpfx )
                            {
                                // endpoint is either last point in segment => advance
                                ++i;
                            }
                            else
                            {
                                // or endpoint is the middle of two control points
                                nX += IntTimes256FromFixed( pCurve->apfx[ i-1 ].x );
                                nY += IntTimes256FromFixed( pCurve->apfx[ i-1 ].y );
                                nX = (nX + 1) / 2;
                                nY = (nY + 1) / 2;
                                // no need to advance, because the current point
                                // is the control point in next bezier spline
                            }

                            pPoints[ nPnt+2 ] = Point( nX, nY );
                            pFlags[ nPnt+2 ] = POLY_NORMAL;

                            // calculate second cubic control point
                            // P1 = 1/3 * (PEnd + 2 * PQControl)
                            nX = pPoints[ nPnt+2 ].X() + 2 * aControlP.X();
                            nY = pPoints[ nPnt+2 ].Y() + 2 * aControlP.Y();
                            pPoints[ nPnt+1 ] = Point( (2*nX+3)/6, (2*nY+3)/6 );
                            pFlags[ nPnt+1 ] = POLY_CONTROL;

                            nPnt += 3;
                        }
                    }

                    // next curve segment
                    pCurve = (TTPOLYCURVE*)&pCurve->apfx[ i ];
                }

                // end point is start point for closed contour
                // disabled, because Polygon class closes the contour itself
                // pPoints[nPnt++] = pPoints[0];
				// #i35928#
				// Added again, but add only when not yet closed
				if(pPoints[nPnt - 1] != pPoints[0])
				{
                    if( bHasOfflinePoints )
                        pFlags[nPnt] = pFlags[0];

					pPoints[nPnt++] = pPoints[0];
				}

                // convert y-coordinates W32 -> VCL
                for( int i = 0; i < nPnt; ++i )
                    pPoints[i].Y() = -pPoints[i].Y();

                // insert into polypolygon
                Polygon aPoly( nPnt, pPoints, (bHasOfflinePoints ? pFlags : NULL) );
                // convert to B2DPolyPolygon
                // TODO: get rid of the intermediate PolyPolygon
                rB2DPolyPoly.append( aPoly.getB2DPolygon() );
            }

            delete[] pPoints;
            delete[] pFlags;
        }

        delete[] pData;
    }

    // rescaling needed for the PolyPolygon conversion
    if( rB2DPolyPoly.count() )
    {
        ::basegfx::B2DHomMatrix aMatrix;
        aMatrix.scale( 1.0/256, 1.0/256 );
        aMatrix.scale( mfFontScale, mfFontScale );
        rB2DPolyPoly.transform( aMatrix );
    }
    
    return bRet;
}

// -----------------------------------------------------------------------

// TODO:  Replace this class with boost::scoped_array
class ScopedCharArray
{
public:
	inline explicit ScopedCharArray(char * pArray): m_pArray(pArray) {}

	inline ~ScopedCharArray() { delete[] m_pArray; }

	inline char * get() const { return m_pArray; }

private:
	char * m_pArray;
};

class ScopedFont
{
public:
	explicit ScopedFont(Os2SalGraphics & rData);

	~ScopedFont();

private:
	Os2SalGraphics & m_rData;
	ULONG m_hOrigFont;
};

ScopedFont::ScopedFont(Os2SalGraphics & rData): m_rData(rData)
{
#if 0
	m_hOrigFont = m_rData.mhFonts[0];
	m_rData.mhFonts[0] = 0; // avoid deletion of current font
#endif
}

ScopedFont::~ScopedFont()
{
#if 0
	if( m_hOrigFont )
	{
		// restore original font, destroy temporary font
		HFONT hTempFont = m_rData.mhFonts[0];
		m_rData.mhFonts[0] = m_hOrigFont;
		SelectObject( m_rData.mhDC, m_hOrigFont );
		DeleteObject( hTempFont );
	}
#endif
}

class ScopedTrueTypeFont
{
public:
	inline ScopedTrueTypeFont(): m_pFont(0) {}

	~ScopedTrueTypeFont();

	int open(void * pBuffer, sal_uInt32 nLen, sal_uInt32 nFaceNum);

	inline TrueTypeFont * get() const { return m_pFont; }

private:
	TrueTypeFont * m_pFont;
};

ScopedTrueTypeFont::~ScopedTrueTypeFont()
{
	if (m_pFont != 0)
		CloseTTFont(m_pFont);
}

int ScopedTrueTypeFont::open(void * pBuffer, sal_uInt32 nLen,
							 sal_uInt32 nFaceNum)
{
	OSL_ENSURE(m_pFont == 0, "already open");
    return OpenTTFontBuffer(pBuffer, nLen, nFaceNum, &m_pFont);
}

sal_Bool Os2SalGraphics::CreateFontSubset( const rtl::OUString& rToFile,
	const ImplFontData* pFont, sal_GlyphId* pGlyphIds, sal_uInt8* pEncoding,
	sal_Int32* pGlyphWidths, int nGlyphCount, FontSubsetInfo& rInfo )
{
	// TODO: use more of the central font-subsetting code, move stuff there if needed

	// create matching ImplFontSelectData
	// we need just enough to get to the font file data
	// use height=1000 for easier debugging (to match psprint's font units)
	ImplFontSelectData aIFSD( *pFont, Size(0,1000), 1000.0, 0, false );

	// TODO: much better solution: move SetFont and restoration of old font to caller
	ScopedFont aOldFont(*this);
	SetFont( &aIFSD, 0 );

    ImplOs2FontData* pWinFontData = (ImplOs2FontData*)aIFSD.mpFontData;
    pWinFontData->UpdateFromHPS( mhPS );
    const ImplFontCharMap* pImplFontCharMap = pWinFontData->GetImplFontCharMap();

#if OSL_DEBUG_LEVEL > 100
	// get font metrics
	TEXTMETRICA aWinMetric;
	if( !::GetTextMetricsA( mhDC, &aWinMetric ) )
		return FALSE;

	DBG_ASSERT( !(aWinMetric.tmPitchAndFamily & TMPF_DEVICE), "cannot subset device font" );
	DBG_ASSERT( aWinMetric.tmPitchAndFamily & TMPF_TRUETYPE, "can only subset TT font" );
#endif

    rtl::OUString aSysPath;
    if( osl_File_E_None != osl_getSystemPathFromFileURL( rToFile.pData, &aSysPath.pData ) )
        return FALSE;
    const rtl_TextEncoding aThreadEncoding = osl_getThreadTextEncoding();
    const ByteString aToFile( aSysPath.getStr(), (xub_StrLen)aSysPath.getLength(), aThreadEncoding );

	// check if the font has a CFF-table
	const DWORD nCffTag = CalcTag( "CFF " );
	const RawFontData aRawCffData( mhPS, nCffTag );
	if( aRawCffData.get() )
	{
		sal_GlyphId aRealGlyphIds[ 256 ];
		for( int i = 0; i < nGlyphCount; ++i )
		{
			// TODO: remap notdef glyph if needed
			// TODO: use GDI's GetGlyphIndices instead? Does it handle GSUB properly?
			sal_uInt32 aGlyphId = pGlyphIds[i] & GF_IDXMASK;
			if( pGlyphIds[i] & GF_ISCHAR ) // remaining pseudo-glyphs need to be translated
				aGlyphId = pImplFontCharMap->GetGlyphIndex( aGlyphId );
			if( (pGlyphIds[i] & (GF_ROTMASK|GF_GSUB)) != 0)	// TODO: vertical substitution
				{/*####*/}

			aRealGlyphIds[i] = aGlyphId;
		}

		// provide a font subset from the CFF-table
		FILE* pOutFile = fopen( aToFile.GetBuffer(), "wb" );
		rInfo.LoadFont( FontSubsetInfo::CFF_FONT, aRawCffData.get(), aRawCffData.size() );
		bool bRC = rInfo.CreateFontSubset( FontSubsetInfo::TYPE1_PFB, pOutFile, NULL,
				aRealGlyphIds, pEncoding, nGlyphCount, pGlyphWidths );
		fclose( pOutFile );
		return bRC;
	}

    // get raw font file data
    const RawFontData xRawFontData( mhPS, NULL );
    if( !xRawFontData.get() )
		return FALSE;

	// open font file
	sal_uInt32 nFaceNum = 0;
	if( !*xRawFontData.get() )	// TTC candidate
		nFaceNum = ~0U;	 // indicate "TTC font extracts only"

	ScopedTrueTypeFont aSftTTF;
	int nRC = aSftTTF.open( (void*)xRawFontData.get(), xRawFontData.size(), nFaceNum );
	if( nRC != SF_OK )
		return FALSE;

	TTGlobalFontInfo aTTInfo;
	::GetTTGlobalFontInfo( aSftTTF.get(), &aTTInfo );
	rInfo.m_nFontType   = FontSubsetInfo::SFNT_TTF;
	rInfo.m_aPSName		= ImplSalGetUniString( aTTInfo.psname );
	rInfo.m_nAscent		= +aTTInfo.winAscent;
	rInfo.m_nDescent	= -aTTInfo.winDescent;
	rInfo.m_aFontBBox	= Rectangle( Point( aTTInfo.xMin, aTTInfo.yMin ),
									Point( aTTInfo.xMax, aTTInfo.yMax ) );
	rInfo.m_nCapHeight	= aTTInfo.yMax; // Well ...

	// subset TTF-glyphs and get their properties
	// take care that subset fonts require the NotDef glyph in pos 0
	int nOrigCount = nGlyphCount;
	USHORT	  aShortIDs[ 256 ];
	sal_uInt8 aTempEncs[ 256 ];

	int nNotDef=-1, i;
	for( i = 0; i < nGlyphCount; ++i )
	{
		aTempEncs[i] = pEncoding[i];
		sal_GlyphId aGlyphId = pGlyphIds[i] & GF_IDXMASK;
		if( pGlyphIds[i] & GF_ISCHAR )
		{
			sal_Unicode cChar = static_cast<sal_Unicode>(aGlyphId); // TODO: sal_UCS4
			const bool bVertical = ((pGlyphIds[i] & (GF_ROTMASK|GF_GSUB)) != 0);
			aGlyphId = ::MapChar( aSftTTF.get(), cChar, bVertical );
			if( (aGlyphId == 0) && pFont->IsSymbolFont() )
			{
				// #i12824# emulate symbol aliasing U+FXXX <-> U+0XXX
				cChar = (cChar & 0xF000) ? (cChar & 0x00FF) : (cChar | 0xF000);
				aGlyphId = ::MapChar( aSftTTF.get(), cChar, bVertical );
			}
		}
		aShortIDs[i] = static_cast<USHORT>( aGlyphId );
		if( !aGlyphId )
			if( nNotDef < 0 )
				nNotDef = i; // first NotDef glyph found
	}

	if( nNotDef != 0 )
	{
		// add fake NotDef glyph if needed
		if( nNotDef < 0 )
			nNotDef = nGlyphCount++;

		// NotDef glyph must be in pos 0 => swap glyphids
		aShortIDs[ nNotDef ] = aShortIDs[0];
		aTempEncs[ nNotDef ] = aTempEncs[0];
		aShortIDs[0] = 0;
		aTempEncs[0] = 0;
	}
	DBG_ASSERT( nGlyphCount < 257, "too many glyphs for subsetting" );

	// fill pWidth array
	TTSimpleGlyphMetrics* pMetrics =
		::GetTTSimpleGlyphMetrics( aSftTTF.get(), aShortIDs, nGlyphCount, aIFSD.mbVertical );
	if( !pMetrics )
		return FALSE;
	sal_uInt16 nNotDefAdv	= pMetrics[0].adv;
	pMetrics[0].adv			= pMetrics[nNotDef].adv;
	pMetrics[nNotDef].adv	= nNotDefAdv;
	for( i = 0; i < nOrigCount; ++i )
		pGlyphWidths[i] = pMetrics[i].adv;
	free( pMetrics );

	// write subset into destination file
	nRC = ::CreateTTFromTTGlyphs( aSftTTF.get(), aToFile.GetBuffer(), aShortIDs,
			aTempEncs, nGlyphCount, 0, NULL, 0 );
	return (nRC == SF_OK);
}

//--------------------------------------------------------------------------

const void* Os2SalGraphics::GetEmbedFontData( const ImplFontData* pFont,
	const sal_Ucs* pUnicodes, sal_Int32* pCharWidths,
	FontSubsetInfo& rInfo, long* pDataLen )
{
	// create matching ImplFontSelectData
	// we need just enough to get to the font file data
	ImplFontSelectData aIFSD( *pFont, Size(0,1000), 1000.0, 0, false );

	// TODO: much better solution: move SetFont and restoration of old font to caller
	ScopedFont aOldFont(*this);
	SetFont( &aIFSD, 0 );

	// get the raw font file data
	RawFontData aRawFontData( mhPS );
	*pDataLen = aRawFontData.size();
	if( !aRawFontData.get() )
		return NULL;

	// get important font properties
	FONTMETRICS aOS2Metric;
	if (Ft2QueryFontMetrics( mhPS, sizeof( aOS2Metric ), &aOS2Metric ) == GPI_ERROR)
			*pDataLen = 0;
	rInfo.m_nFontType	= FontSubsetInfo::ANY_TYPE1;
	rInfo.m_aPSName		= ImplSalGetUniString( aOS2Metric.szFacename );
	rInfo.m_nAscent		= +aOS2Metric.lMaxAscender;
	rInfo.m_nDescent	= -aOS2Metric.lMaxDescender;
	rInfo.m_aFontBBox	= Rectangle( Point( 0, -aOS2Metric.lMaxDescender ),
			  Point( aOS2Metric.lMaxCharInc, aOS2Metric.lMaxAscender+aOS2Metric.lExternalLeading ) );
	rInfo.m_nCapHeight	= aOS2Metric.lMaxAscender; // Well ...

	// get individual character widths
	for( int i = 0; i < 256; ++i )
	{
		LONG nCharWidth = 0;
		const sal_Ucs cChar = pUnicodes[i];
		if( !Ft2QueryStringWidthW( mhPS, (LPWSTR)&cChar, 1, &nCharWidth ) )
			*pDataLen = 0;
		pCharWidths[i] = nCharWidth;
	}

	if( !*pDataLen )
		return NULL;

	const unsigned char* pData = aRawFontData.steal();
	return (void*)pData;
}

//--------------------------------------------------------------------------

void Os2SalGraphics::FreeEmbedFontData( const void* pData, long /*nLen*/ )
{
	delete[] reinterpret_cast<char*>(const_cast<void*>(pData));
}

const Ucs2SIntMap* Os2SalGraphics::GetFontEncodingVector( const ImplFontData* pFont, const Ucs2OStrMap** pNonEncoded )
{
    // TODO: even for builtin fonts we get here... why?
    if( !pFont->IsEmbeddable() )
        return NULL;

    // fill the encoding vector
    // currently no nonencoded vector
    if( pNonEncoded )
        *pNonEncoded = NULL;

    const ImplOs2FontData* pWinFontData = static_cast<const ImplOs2FontData*>(pFont);
    const Ucs2SIntMap* pEncoding = pWinFontData->GetEncodingVector();
    if( pEncoding == NULL )
    {
        Ucs2SIntMap* pNewEncoding = new Ucs2SIntMap;
        #if 0
        // TODO: get correct encoding vector
        GLYPHSET aGlyphSet;
        aGlyphSet.cbThis = sizeof(aGlyphSet);
        DWORD aW = ::GetFontUnicodeRanges( mhPS, &aGlyphSet);
        #else
        for( sal_Unicode i = 32; i < 256; ++i )
            (*pNewEncoding)[i] = i;
        #endif
        pWinFontData->SetEncodingVector( pNewEncoding );
	pEncoding = pNewEncoding;
    }

    return pEncoding;
}

//--------------------------------------------------------------------------

void Os2SalGraphics::GetGlyphWidths( const ImplFontData* pFont,
                                     bool bVertical,
                                     Int32Vector& rWidths,
                                     Ucs2UIntMap& rUnicodeEnc )
{
    // create matching ImplFontSelectData
    // we need just enough to get to the font file data
    ImplFontSelectData aIFSD( *pFont, Size(0,1000), 1000.0, 0, false );

    // TODO: much better solution: move SetFont and restoration of old font to caller
    ScopedFont aOldFont(*this);
    
    float fScale = 0.0;
    ImplDoSetFont( &aIFSD, fScale, 0);
    
    if( pFont->IsSubsettable() )
    {
        // get raw font file data
	const RawFontData xRawFontData( mhPS );
	if( !xRawFontData.get() )
		return;

        // open font file
        sal_uInt32 nFaceNum = 0;
        if( !*xRawFontData.get() )  // TTC candidate
            nFaceNum = ~0U;  // indicate "TTC font extracts only"
    
        ScopedTrueTypeFont aSftTTF;
        int nRC = aSftTTF.open( (void*)xRawFontData.get(), xRawFontData.size(), nFaceNum );
        if( nRC != SF_OK )
            return;
    
        int nGlyphs = GetTTGlyphCount( aSftTTF.get() );
        if( nGlyphs > 0 )
        {
            rWidths.resize(nGlyphs);
            std::vector<sal_uInt16> aGlyphIds(nGlyphs);
            for( int i = 0; i < nGlyphs; i++ )
                aGlyphIds[i] = sal_uInt16(i);
            TTSimpleGlyphMetrics* pMetrics = ::GetTTSimpleGlyphMetrics( aSftTTF.get(),
                                                                        &aGlyphIds[0],
                                                                        nGlyphs,
                                                                        bVertical ? 1 : 0 );
            if( pMetrics )
            {
                for( int i = 0; i< nGlyphs; i++ )
                    rWidths[i] = pMetrics[i].adv;
                free( pMetrics );
                rUnicodeEnc.clear();
            }
            const ImplOs2FontData* pWinFont = static_cast<const ImplOs2FontData*>(pFont);
            const ImplFontCharMap* pMap = pWinFont->GetImplFontCharMap();
			DBG_ASSERT( pMap && pMap->GetCharCount(), "no map" );

			int nCharCount = pMap->GetCharCount();
            sal_uInt32 nChar = pMap->GetFirstChar();
			for( int i = 0; i < nCharCount; i++ )
            {
                if( nChar < 0x00010000 )
                {
                    sal_uInt16 nGlyph = ::MapChar( aSftTTF.get(),
                                                   static_cast<sal_uInt16>(nChar),
                                                   bVertical ? 1 : 0 );
                    if( nGlyph )
                        rUnicodeEnc[ static_cast<sal_Unicode>(nChar) ] = nGlyph;
                }
				nChar = pMap->GetNextChar( nChar );
            }
        }
    }
    else if( pFont->IsEmbeddable() )
    {
        // get individual character widths
        rWidths.clear();
        rUnicodeEnc.clear();
        rWidths.reserve( 224 );
        for( sal_Unicode i = 32; i < 256; ++i )
        {
            int nCharWidth = 0;
            if( Ft2QueryStringWidthW( mhPS, (LPWSTR)&i, 1, (LONG*)&nCharWidth ) )
            {
                rUnicodeEnc[ i ] = rWidths.size();
                rWidths.push_back( nCharWidth );
            }
        }
    }    
}

//--------------------------------------------------------------------------

void Os2SalGraphics::DrawServerFontLayout( const ServerFontLayout& )
{}

//--------------------------------------------------------------------------

SystemFontData Os2SalGraphics::GetSysFontData( int nFallbacklevel ) const
{
    SystemFontData aSysFontData;

    if (nFallbacklevel >= MAX_FALLBACK) nFallbacklevel = MAX_FALLBACK - 1;
    if (nFallbacklevel < 0 ) nFallbacklevel = 0;
    
    aSysFontData.nSize = sizeof( SystemFontData );
    aSysFontData.hFont = mhFonts[nFallbacklevel]; 
    aSysFontData.bFakeBold = false;
    aSysFontData.bFakeItalic = false;
    aSysFontData.bAntialias = true;
    aSysFontData.bVerticalCharacterType = false;
        
    return aSysFontData;
}

//--------------------------------------------------------------------------
