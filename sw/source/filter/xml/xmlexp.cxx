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



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_sw.hxx"

#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/drawing/XDrawPageSupplier.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/container/XIndexContainer.hpp>
#include <com/sun/star/uno/RuntimeException.hpp>
#include <com/sun/star/xforms/XFormsSupplier.hpp>
#include <svx/svdmodel.hxx>
#include <svx/svdpage.hxx>
#include <svx/xmleohlp.hxx>
#include <svx/xmlgrhlp.hxx>
#include <editeng/eeitem.hxx>
#include <svx/svddef.hxx>
#include <xmloff/nmspmap.hxx>
#include <xmloff/xmlnmspe.hxx>
#include <editeng/xmlcnitm.hxx>
#include <xmloff/ProgressBarHelper.hxx>
#include <xmloff/xmluconv.hxx>
#include <xmloff/xformsexport.hxx>
#include <pam.hxx>
#include <doc.hxx>
#include <swmodule.hxx>
#include <docsh.hxx>
#include <viewsh.hxx>
#include <docstat.hxx>
#include <swerror.h>
#include <unotext.hxx>
#include <xmltexte.hxx>
#include <xmlexp.hxx>
#include <sfx2/viewsh.hxx>
#include <comphelper/processfactory.hxx>
#include <docary.hxx>
#include <editeng/unolingu.hxx>
#include <editeng/forbiddencharacterstable.hxx>
#include <ForbiddenCharactersEnum.hxx>

// for locking SolarMutex: svapp + mutex
#include <vcl/svapp.hxx>
#include <vos/mutex.hxx>

#include <pausethreadstarting.hxx>

using ::rtl::OUString;
using namespace ::com::sun::star;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::xml::sax;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::text;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::document;
using namespace ::com::sun::star::drawing;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::i18n;
using namespace ::com::sun::star::xforms;
using namespace ::xmloff::token;

#ifdef XML_CORE_API
void SwXMLExport::SetCurPaM( SwPaM& rPaM, sal_Bool bWhole, sal_Bool bTabOnly )
{
	if( !pCurPaM )
	{
		pCurPaM = new SwPaM( *rPaM.End(), *rPaM.Start() );
	}
	else
	{
		*pCurPaM->GetPoint() = *rPaM.Start();
		*pCurPaM->GetMark() = *rPaM.End();
	}

	// Set PaM to table/section start node if whole doc should be exported
	if( bWhole )
	{
		SwTableNode *pTblNd = pCurPaM->GetNode()->FindTableNode();
		if( pTblNd )
		{
			pCurPaM->GetPoint()->nNode = *pTblNd;

			if( bTabOnly )
				pCurPaM->GetMark()->nNode = *pTblNd->EndOfSectionNode();
		}

		SwSectionNode * pSectNd = pCurPaM->GetNode()->FindSectionNode();
		while( pSectNd )
		{
			pCurPaM->GetPoint()->nNode = *pSectNd;

			// SwSectionNode::FindSectionNode() returns the section node itself
            pSectNd = pSectNd->StartOfSectionNode()->FindSectionNode();
		}
	}
}
#endif

// #110680#
SwXMLExport::SwXMLExport(
    const uno::Reference< lang::XMultiServiceFactory > xServiceFactory,
	sal_uInt16 nExportFlags)
:	SvXMLExport( xServiceFactory, MAP_INCH, XML_TEXT, nExportFlags ),
#ifdef XML_CORE_API
	pCurPaM( 0 ),
	pOrigPaM( &rPaM ),
#endif
	pTableItemMapper( 0 ),
	pTableLines( 0 ),
#ifdef XML_CORE_API
	bExportWholeDoc( bExpWholeDoc ),
	bExportFirstTableOnly( bExpFirstTableOnly ),
#endif
	bBlock( sal_False ),
	bShowProgress( sal_True ),
	sNumberFormat(RTL_CONSTASCII_USTRINGPARAM("NumberFormat")),
	sIsProtected(RTL_CONSTASCII_USTRINGPARAM("IsProtected")),
	sCell(RTL_CONSTASCII_USTRINGPARAM("Cell"))
{
	_InitItemExport();
}

#ifdef XML_CORE_API
// #110680#
SwXMLExport::SwXMLExport(
    const uno::Reference< lang::XMultiServiceFactory > xServiceFactory,
	const Reference< XModel >& rModel,
	SwPaM& rPaM,
	const OUString& rFileName,
	const Reference< XDocumentHandler > & rHandler,
	const Reference< XGraphicObjectResolver > & rEmbeddedGrfObjs,
	sal_Bool bExpWholeDoc, sal_Bool bExpFirstTableOnly,
	sal_Bool bShowProg )
:	SvXMLExport( xServiceFactory, rFileName, rHandler, rModel, rEmbeddedGrfObjs,
				 SW_MOD()->GetMetric( rPaM.GetDoc()->get(IDocumentSettingAccess::HTML_MODE) ) ),
	pCurPaM( 0 ),
	pOrigPaM( &rPaM ),
	pTableItemMapper( 0 ),
	pTableLines( 0 ),
	bExportWholeDoc( bExpWholeDoc ),
	bExportFirstTableOnly( bExpFirstTableOnly ),
	bShowProgress( bShowProg ),
	sNumberFormat(RTL_CONSTASCII_USTRINGPARAM("NumberFormat")),
	sIsProtected(RTL_CONSTASCII_USTRINGPARAM("IsProtected")),
	sCell(RTL_CONSTASCII_USTRINGPARAM("Cell"))
{
	_InitItemExport();
}
#endif

void SwXMLExport::setBlockMode()
{
	bBlock = sal_True;

}

sal_uInt32 SwXMLExport::exportDoc( enum XMLTokenEnum eClass )
{
	if( !GetModel().is() )
		return ERR_SWG_WRITE_ERROR;

    // --> OD 2007-03-30 #i73788#
    SwPauseThreadStarting aPauseThreadStarting;
    // <--

	Reference < XTextDocument > xTextDoc( GetModel(), UNO_QUERY );
	Reference < XText > xText = xTextDoc->getText();
	Reference<XUnoTunnel> xTextTunnel( xText, UNO_QUERY);
	ASSERT( xTextTunnel.is(), "missing XUnoTunnel for Cursor" );
	if( !xTextTunnel.is() )
		return ERR_SWG_WRITE_ERROR;

    // from here, we use core interfaces -> lock Solar-Mutex (#91949#)
	vos::OGuard aGuard(Application::GetSolarMutex());

	{
        Reference<XPropertySet> rInfoSet = getExportInfo();
        if( rInfoSet.is() )
        {
			OUString sAutoTextMode(
				RTL_CONSTASCII_USTRINGPARAM("AutoTextMode"));
            if( rInfoSet->getPropertySetInfo()->hasPropertyByName(
						sAutoTextMode ) )
			{
				Any aAny = rInfoSet->getPropertyValue(sAutoTextMode);
				if( aAny.getValueType() == ::getBooleanCppuType() &&
					*static_cast<const sal_Bool*>(aAny.getValue()) )
					setBlockMode();
			}
        }
	}

	SwXText *pText = reinterpret_cast< SwXText * >(
			sal::static_int_cast< sal_IntPtr >( xTextTunnel->getSomething( SwXText::getUnoTunnelId() )));
	ASSERT( pText, "SwXText missing" );
	if( !pText )
		return ERR_SWG_WRITE_ERROR;

	SwDoc *pDoc = pText->GetDoc();

	sal_Bool bExtended = sal_False;
	if( (getExportFlags() & (EXPORT_FONTDECLS|EXPORT_STYLES|
							 EXPORT_MASTERSTYLES|EXPORT_CONTENT)) != 0 )
	{
		GetTextParagraphExport()->SetBlockMode( bBlock );

		const SfxPoolItem* pItem;
		const SfxItemPool& rPool = pDoc->GetAttrPool();
		sal_uInt16 aWhichIds[5] = { RES_UNKNOWNATR_CONTAINER,
									RES_TXTATR_UNKNOWN_CONTAINER,
									SDRATTR_XMLATTRIBUTES,
									EE_PARA_XMLATTRIBS,
									EE_CHAR_XMLATTRIBS };

		sal_uInt16 nWhichIds = rPool.GetSecondaryPool() ? 5 : 2;
		for( sal_uInt16 j=0; j < nWhichIds; j++ )
		{
			sal_uInt16 nWhichId = aWhichIds[j];
			sal_uInt32 i=0, nItems = rPool.GetItemCount2( nWhichId );
			for( i = 0; i < nItems; ++i )
			{
				if( 0 != (pItem = rPool.GetItem2( nWhichId , i ) ) )
				{
					const SvXMLAttrContainerItem *pUnknown =
								PTR_CAST( SvXMLAttrContainerItem, pItem );
					ASSERT( pUnknown, "illegal attribute container item" );
					if( pUnknown && (pUnknown->GetAttrCount() > 0) )
					{
						sal_uInt16 nIdx = pUnknown->GetFirstNamespaceIndex();
						while( USHRT_MAX != nIdx )
						{
							const OUString& rPrefix =
								pUnknown->GetPrefix( nIdx );
							_GetNamespaceMap().Add( rPrefix,
												pUnknown->GetNamespace( nIdx ),
												XML_NAMESPACE_UNKNOWN );
							nIdx = pUnknown->GetNextNamespaceIndex( nIdx );
						}
						bExtended = sal_True;
					}
				}
			}
		}
	}

	MapUnit eUnit =
		SvXMLUnitConverter::GetMapUnit( SW_MOD()->GetMetric(pDoc->get(IDocumentSettingAccess::HTML_MODE)) );
	if( GetMM100UnitConverter().getXMLMeasureUnit() != eUnit )
	{
		GetMM100UnitConverter().setXMLMeasureUnit( eUnit );
		pTwipUnitConv->setXMLMeasureUnit( eUnit );
	}

	SetExtended( bExtended );

	SwDocStat aDocStat( pDoc->GetDocStat() );
	if( (getExportFlags() & EXPORT_META) != 0 )
	{
		// Update doc stat, so that correct values are exported and
		// the progress works correctly.
		if( aDocStat.bModified )
			pDoc->UpdateDocStat( aDocStat );

		SfxObjectShell* pObjSh = pDoc->GetDocShell();
		if( pObjSh )
			pObjSh->UpdateDocInfoForSave();		// update information
	}
	if( bShowProgress )
	{
		ProgressBarHelper *pProgress = GetProgressBarHelper();
		if( -1 == pProgress->GetReference() )
		{
			// progress isn't initialized:
			// We assume that the whole doc is exported, and the following
			// durations:
			// - meta information: 2
			// - settings: 4 (TODO: not now!)
			// - styles (except page styles): 2
			// - page styles: 2 (TODO: not now!) + 2 for each paragraph
			// - paragraph: 2 (1 for automatic styles and one for content)

			// If required, update doc stat, so that
			// the progress works correctly.
			if( aDocStat.bModified )
				pDoc->UpdateDocStat( aDocStat );

			// count each item once, and then multiply by two to reach the
			// figures given above
            // The styles in pDoc also count the default style that never
            // gets exported -> subtract one.
            sal_Int32 nRef = 1; // meta.xml
			nRef += pDoc->GetCharFmts()->Count() - 1;
			nRef += pDoc->GetFrmFmts()->Count() - 1;
			nRef += pDoc->GetTxtFmtColls()->Count() - 1;
//			nRef += pDoc->GetPageDescCnt();
            nRef *= 2; // for the above styles, xmloff will increment by 2!
            // #i93174#: count all paragraphs for the progress bar
            nRef += aDocStat.nAllPara; // 1: only content, no autostyle
            pProgress->SetReference( nRef );
			pProgress->SetValue( 0 );
		}
	}

	if( (getExportFlags() & (EXPORT_MASTERSTYLES|EXPORT_CONTENT)) != 0 )
	{
		//Auf die Korrektheit der OrdNums sind wir schon angewiesen.
		SdrModel* pModel = pDoc->GetDrawModel();
		if( pModel )
			pModel->GetPage( 0 )->RecalcObjOrdNums();
	}

	// adjust document class (eClass)
	if (pDoc->get(IDocumentSettingAccess::GLOBAL_DOCUMENT))
	{
		eClass = XML_TEXT_GLOBAL;

		// additionally, we take care of the save-linked-sections-thingy
        mbSaveLinkedSections = pDoc->get(IDocumentSettingAccess::GLOBAL_DOCUMENT_SAVE_LINKS);
    }
	// MIB: 03/26/04: The Label information is saved in the settings, so
	// we don't need it here.
	// else: keep default pClass that we received

	SvXMLGraphicHelper *pGraphicResolver = 0;
	if( !GetGraphicResolver().is() )
	{
		pGraphicResolver = SvXMLGraphicHelper::Create( GRAPHICHELPER_MODE_WRITE );
		Reference< XGraphicObjectResolver > xGraphicResolver( pGraphicResolver );
		SetGraphicResolver( xGraphicResolver );
	}

	SvXMLEmbeddedObjectHelper *pEmbeddedResolver = 0;
	if( !GetEmbeddedResolver().is() )
	{
        SfxObjectShell *pPersist = pDoc->GetPersist();
		if( pPersist )
		{
			pEmbeddedResolver = SvXMLEmbeddedObjectHelper::Create(
											*pPersist,
											EMBEDDEDOBJECTHELPER_MODE_WRITE );
			Reference< XEmbeddedObjectResolver > xEmbeddedResolver( pEmbeddedResolver );
			SetEmbeddedResolver( xEmbeddedResolver );
		}
	}

    // set redline mode if we export STYLES or CONTENT, unless redline
    // mode is taken care of outside (through info XPropertySet)
    sal_Bool bSaveRedline =
        ( (getExportFlags() & (EXPORT_CONTENT|EXPORT_STYLES)) != 0 );
    if( bSaveRedline )
    {
        // if the info property set has a ShowChanges property,
        // then change tracking is taken care of on the outside,
        // so we don't have to!
        Reference<XPropertySet> rInfoSet = getExportInfo();
        if( rInfoSet.is() )
        {
            OUString sShowChanges( RTL_CONSTASCII_USTRINGPARAM("ShowChanges"));
            bSaveRedline = ! rInfoSet->getPropertySetInfo()->hasPropertyByName(
                                                                sShowChanges );
        }
    }
    sal_uInt16 nRedlineMode = 0;
    bSavedShowChanges = IDocumentRedlineAccess::IsShowChanges( pDoc->GetRedlineMode() );
    if( bSaveRedline )
    {
        // now save and switch redline mode
        nRedlineMode = pDoc->GetRedlineMode();
        pDoc->SetRedlineMode(
			     (RedlineMode_t)(( nRedlineMode & nsRedlineMode_t::REDLINE_SHOW_MASK ) | nsRedlineType_t::REDLINE_INSERT ));
    }

 	sal_uInt32 nRet = SvXMLExport::exportDoc( eClass );

    // now we can restore the redline mode (if we changed it previously)
    if( bSaveRedline )
    {
      pDoc->SetRedlineMode( (RedlineMode_t)(nRedlineMode ));
    }


	if( pGraphicResolver )
		SvXMLGraphicHelper::Destroy( pGraphicResolver );
	if( pEmbeddedResolver )
	    SvXMLEmbeddedObjectHelper::Destroy( pEmbeddedResolver );

	ASSERT( !pTableLines, "there are table columns infos left" );

	return nRet;
}

XMLTextParagraphExport* SwXMLExport::CreateTextParagraphExport()
{
	return new SwXMLTextParagraphExport( *this, *GetAutoStylePool().get() );
}

XMLShapeExport* SwXMLExport::CreateShapeExport()
{
	XMLShapeExport* pShapeExport = new XMLShapeExport( *this, XMLTextParagraphExport::CreateShapeExtPropMapper( *this ) );
	Reference < XDrawPageSupplier > xDPS( GetModel(), UNO_QUERY );
	if( xDPS.is() )
	{
	 	Reference < XShapes > xShapes( xDPS->getDrawPage(), UNO_QUERY );
		pShapeExport->seekShapes( xShapes );
	}

	return pShapeExport;
}

__EXPORT SwXMLExport::~SwXMLExport()
{
	_FinitItemExport();
}


void SwXMLExport::_ExportFontDecls()
{
	GetFontAutoStylePool();	// make sure the pool is created
	SvXMLExport::_ExportFontDecls();
}

#define NUM_EXPORTED_VIEW_SETTINGS 11
void SwXMLExport::GetViewSettings(Sequence<PropertyValue>& aProps)
{
	Reference< XMultiServiceFactory > xServiceFactory =
			comphelper::getProcessServiceFactory();
	ASSERT( xServiceFactory.is(),
			"XMLReader::Read: got no service manager" );
	if( !xServiceFactory.is() )
		return;

	aProps.realloc( NUM_EXPORTED_VIEW_SETTINGS );
	 // Currently exporting 9 properties
    PropertyValue *pValue = aProps.getArray();
    sal_Int32 nIndex = 0;

    Reference < XIndexContainer > xBox (xServiceFactory->createInstance
            (OUString( RTL_CONSTASCII_USTRINGPARAM ("com.sun.star.document.IndexedPropertyValues") ) ), UNO_QUERY);
    if (xBox.is() )
	{
		pValue[nIndex].Name = OUString( RTL_CONSTASCII_USTRINGPARAM ( "Views") );
		pValue[nIndex++].Value <<= Reference < XIndexAccess > ( xBox, UNO_QUERY );
	}

	Reference < XText > xText;
	SwXText *pText = 0;

	if( GetModel().is() )
    {
		Reference < XTextDocument > xTextDoc( GetModel(), UNO_QUERY );
		xText = xTextDoc->getText();
		Reference<XUnoTunnel> xTextTunnel( xText, UNO_QUERY);
		ASSERT( xTextTunnel.is(), "missing XUnoTunnel for Cursor" );
		if( xTextTunnel.is() )
		{
			pText = reinterpret_cast< SwXText * >(
					sal::static_int_cast< sal_IntPtr >( xTextTunnel->getSomething( SwXText::getUnoTunnelId()) ));
			ASSERT( pText, "SwXText missing" );
		}
	}

	if( !pText )
    {
        aProps.realloc(nIndex);
		return;
    }

	SwDoc *pDoc = pText->GetDoc();
	const Rectangle rRect =
        pDoc->GetDocShell()->GetVisArea( ASPECT_CONTENT );
    sal_Bool bTwip = pDoc->GetDocShell()->GetMapUnit ( ) == MAP_TWIP;

	ASSERT ( bTwip, "Map unit for visible area is not in TWIPS!" );

    pValue[nIndex].Name = OUString( RTL_CONSTASCII_USTRINGPARAM ( "ViewAreaTop") );
    pValue[nIndex++].Value <<= bTwip ? TWIP_TO_MM100 ( rRect.Top() ) : rRect.Top();

    pValue[nIndex].Name = OUString( RTL_CONSTASCII_USTRINGPARAM ( "ViewAreaLeft") );
    pValue[nIndex++].Value <<= bTwip ? TWIP_TO_MM100 ( rRect.Left() ) : rRect.Left();

    pValue[nIndex].Name = OUString( RTL_CONSTASCII_USTRINGPARAM ( "ViewAreaWidth") );
    pValue[nIndex++].Value <<= bTwip ? TWIP_TO_MM100 ( rRect.GetWidth() ) : rRect.GetWidth();

    pValue[nIndex].Name = OUString( RTL_CONSTASCII_USTRINGPARAM ( "ViewAreaHeight") );
    pValue[nIndex++].Value <<= bTwip ? TWIP_TO_MM100 ( rRect.GetHeight() ) : rRect.GetHeight();

    // "show redline mode" cannot simply be read from the document
    // since it gets changed during execution. If it's in the info
    // XPropertySet, we take it from there.
    sal_Bool bShowRedlineChanges = bSavedShowChanges;
    Reference<XPropertySet> xInfoSet( getExportInfo() );
    if ( xInfoSet.is() )
    {
        OUString sShowChanges( RTL_CONSTASCII_USTRINGPARAM( "ShowChanges" ));
        if( xInfoSet->getPropertySetInfo()->hasPropertyByName( sShowChanges ) )
        {
            bShowRedlineChanges = *(sal_Bool*) xInfoSet->
                                   getPropertyValue( sShowChanges ).getValue();
        }
    }

    pValue[nIndex].Name = OUString( RTL_CONSTASCII_USTRINGPARAM ( "ShowRedlineChanges") );
    pValue[nIndex++].Value.setValue( &bShowRedlineChanges, ::getBooleanCppuType() );

    sal_Bool bInBrowse =  pDoc->get(IDocumentSettingAccess::BROWSE_MODE);
    pValue[nIndex].Name = OUString( RTL_CONSTASCII_USTRINGPARAM ( "InBrowseMode") );
    pValue[nIndex++].Value.setValue( &bInBrowse, ::getBooleanCppuType() );

	if ( nIndex < NUM_EXPORTED_VIEW_SETTINGS )
        aProps.realloc(nIndex);
}
#undef NUM_EXPORTED_VIEW_SETTINGS

void SwXMLExport::GetConfigurationSettings( Sequence < PropertyValue >& rProps)
{
    Reference< XMultiServiceFactory > xFac( GetModel(), UNO_QUERY );
    if( xFac.is() )
    {
        Reference< XPropertySet > xProps( xFac->createInstance( OUString( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.document.Settings" ) ) ), UNO_QUERY );
        if( xProps.is() )
        {
            // property CurrentDatabaseDataSource needs to be exported before CurrentDatabaseCommand and CurrentDatabaseCommandType
            // in order to assure that AOO/OOo versions (before AOO 4.0.1) are able to apply the current database settings correct
            // Thus, put CurrentDatabaseDataSource as the first one into the Sequence
//            SvXMLUnitConverter::convertPropertySet( rProps, xProps );
            uno::Reference< beans::XPropertySetInfo > xPropertySetInfo = xProps->getPropertySetInfo();
            if (xPropertySetInfo.is())
            {
                uno::Sequence< beans::Property > aProps = xPropertySetInfo->getProperties();
                const sal_Int32 nCount(aProps.getLength());
                if (nCount)
                {
                    rProps.realloc(nCount);
                    beans::PropertyValue* pProps = rProps.getArray();

                    static ::rtl::OUString csCurrentDatabaseSource = ::rtl::OUString::createFromAscii("CurrentDatabaseDataSource");
                    const sal_Bool bHasCurrentDatabaseSource = xPropertySetInfo->hasPropertyByName( csCurrentDatabaseSource );
                    if ( bHasCurrentDatabaseSource )
                    {
                        beans::Property aProp = xPropertySetInfo->getPropertyByName( csCurrentDatabaseSource );
                        pProps->Name = aProp.Name;
                        pProps->Value = xProps->getPropertyValue(aProp.Name);
                        ++pProps;
                    }

                    for (sal_Int32 i = 0; i < nCount; i++ )
                    {
                        if ( bHasCurrentDatabaseSource
                             && aProps[i].Name.compareTo( csCurrentDatabaseSource ) == 0 )
                        {
                            // nothing to do - already added as first item - see above
                        }
                        else
                        {
                            pProps->Name = aProps[i].Name;
                            pProps->Value = xProps->getPropertyValue(aProps[i].Name);
                            ++pProps;
                        }
                    }
                }
            }
        }
    }
}

sal_Int32 SwXMLExport::GetDocumentSpecificSettings( ::std::list< SettingsGroup >& _out_rSettings )
{
    // the only doc-specific settings group we know so far are the XForms settings
    uno::Sequence<beans::PropertyValue> aXFormsSettings;
    Reference< XFormsSupplier > xXFormsSupp( GetModel(), UNO_QUERY );
    Reference< XNameAccess > xXForms;
    if ( xXFormsSupp.is() )
        xXForms = xXFormsSupp->getXForms().get();
    if ( xXForms.is() )
    {
        getXFormsSettings( xXForms, aXFormsSettings );
        _out_rSettings.push_back( SettingsGroup( XML_XFORM_MODEL_SETTINGS, aXFormsSettings ) );
    }

    return aXFormsSettings.getLength() + SvXMLExport::GetDocumentSpecificSettings( _out_rSettings );
}

void SwXMLExport::SetBodyAttributes()
{
	Reference < XTextDocument > xTextDoc( GetModel(), UNO_QUERY );
	Reference < XText > xText = xTextDoc->getText();
    // export use of soft page breaks
    {
        Reference<XUnoTunnel> xTextTunnel( xText, UNO_QUERY);
        ASSERT( xTextTunnel.is(), "missing XUnoTunnel for Cursor" );
        if( xTextTunnel.is() )
        {
            SwXText *pText = reinterpret_cast< SwXText * >(
                    sal::static_int_cast< sal_IntPtr >( xTextTunnel->getSomething( SwXText::getUnoTunnelId() )));
	        ASSERT( pText, "SwXText missing" );
            if( pText )
            {
                SwDoc *pDoc = pText->GetDoc();
                if( pDoc && pDoc->GetCurrentViewShell() &&
                    pDoc->GetCurrentViewShell()->GetPageCount() > 1 )
                {
                    sal_Bool bValue = sal_True;
                    rtl::OUStringBuffer sBuffer;
                    GetMM100UnitConverter().convertBool(sBuffer, bValue);
                    AddAttribute(XML_NAMESPACE_TEXT, XML_USE_SOFT_PAGE_BREAKS,
                        sBuffer.makeStringAndClear());
                }
            }
        }
    }
}


void SwXMLExport::_ExportContent()
{
	// export forms
	Reference<XDrawPageSupplier> xDrawPageSupplier(GetModel(), UNO_QUERY);
	if (xDrawPageSupplier.is())
	{
		// export only if we actually have elements
		Reference<XDrawPage> xPage = xDrawPageSupplier->getDrawPage();
		if (xPage.is())
		{
            // #103597# prevent export of form controls which are embedded in
            // mute sections
            Reference<XIndexAccess> xIAPage( xPage, UNO_QUERY );
            GetTextParagraphExport()->PreventExportOfControlsInMuteSections(
                xIAPage, GetFormExport() );

            // #i36597# / 2004-12-13 / fs@openoffice.org
            if ( GetFormExport()->pageContainsForms( xPage ) || GetFormExport()->documentContainsXForms() )
			{
				::xmloff::OOfficeFormsExport aOfficeForms(*this);

				GetFormExport()->exportXForms();

				GetFormExport()->seekPage(xPage);
				GetFormExport()->exportForms(xPage);
			}
		}
	}

	Reference<XPropertySet> xPropSet(GetModel(), UNO_QUERY);
	if (xPropSet.is())
	{
		OUString sTwoDigitYear(RTL_CONSTASCII_USTRINGPARAM("TwoDigitYear"));

		Any aAny = xPropSet->getPropertyValue( sTwoDigitYear );
		aAny <<= (sal_Int16)1930;

		sal_Int16 nYear = 0;
		aAny >>= nYear;
		if (nYear != 1930 )
		{
			rtl::OUStringBuffer sBuffer;
			GetMM100UnitConverter().convertNumber(sBuffer, nYear);
			AddAttribute(XML_NAMESPACE_TABLE, XML_NULL_YEAR, sBuffer.makeStringAndClear());
			SvXMLElementExport aCalcSettings(*this, XML_NAMESPACE_TABLE, XML_CALCULATION_SETTINGS, sal_True, sal_True);
		}
	}

	GetTextParagraphExport()->exportTrackedChanges( sal_False );
	GetTextParagraphExport()->exportTextDeclarations();
	Reference < XTextDocument > xTextDoc( GetModel(), UNO_QUERY );
	Reference < XText > xText = xTextDoc->getText();

	GetTextParagraphExport()->exportFramesBoundToPage( bShowProgress );
	GetTextParagraphExport()->exportText( xText, bShowProgress );
}



//
// uno component registration
// helper functions for export service(s)
//

// OOo
OUString SAL_CALL SwXMLExportOOO_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExportOOO_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExportOOO_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExportOOO_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	return (cppu::OWeakObject*)new SwXMLExport( rSMgr, EXPORT_ALL);
}

OUString SAL_CALL SwXMLExportStylesOOO_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLStylesExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExportStylesOOO_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExportStylesOOO_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExportStylesOOO_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	return (cppu::OWeakObject*)new SwXMLExport( rSMgr,
		EXPORT_STYLES | EXPORT_MASTERSTYLES | EXPORT_AUTOSTYLES |
		EXPORT_FONTDECLS );
}

OUString SAL_CALL SwXMLExportContentOOO_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLContentExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExportContentOOO_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExportContentOOO_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExportContentOOO_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	return (cppu::OWeakObject*)new SwXMLExport(rSMgr,
		EXPORT_AUTOSTYLES | EXPORT_CONTENT | EXPORT_SCRIPTS |
		EXPORT_FONTDECLS );
}

OUString SAL_CALL SwXMLExportMetaOOO_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLMetaExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExportMetaOOO_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExportMetaOOO_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExportMetaOOO_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	return (cppu::OWeakObject*)new SwXMLExport( rSMgr, EXPORT_META);
}

OUString SAL_CALL SwXMLExportSettingsOOO_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLSettingsExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExportSettingsOOO_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExportSettingsOOO_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExportSettingsOOO_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	return (cppu::OWeakObject*)new SwXMLExport( rSMgr, EXPORT_SETTINGS);
}

// OASIS
OUString SAL_CALL SwXMLExport_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLOasisExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExport_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExport_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExport_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	return (cppu::OWeakObject*)new SwXMLExport( rSMgr, EXPORT_ALL|EXPORT_OASIS);
}

OUString SAL_CALL SwXMLExportStyles_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLOasisStylesExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExportStyles_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExportStyles_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExportStyles_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	// #110680#
	//return (cppu::OWeakObject*)new SwXMLExport(
	//	EXPORT_STYLES | EXPORT_MASTERSTYLES | EXPORT_AUTOSTYLES |
	//	EXPORT_FONTDECLS );
	return (cppu::OWeakObject*)new SwXMLExport( rSMgr,
		EXPORT_STYLES | EXPORT_MASTERSTYLES | EXPORT_AUTOSTYLES |
		EXPORT_FONTDECLS|EXPORT_OASIS );
}

OUString SAL_CALL SwXMLExportContent_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLOasisContentExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExportContent_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExportContent_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExportContent_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	// #110680#
	//return (cppu::OWeakObject*)new SwXMLExport(
	//	EXPORT_AUTOSTYLES | EXPORT_CONTENT | EXPORT_SCRIPTS |
	//	EXPORT_FONTDECLS );
	return (cppu::OWeakObject*)new SwXMLExport(
		rSMgr,
		EXPORT_AUTOSTYLES | EXPORT_CONTENT | EXPORT_SCRIPTS |
		EXPORT_FONTDECLS|EXPORT_OASIS );
}

OUString SAL_CALL SwXMLExportMeta_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLOasisMetaExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExportMeta_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExportMeta_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExportMeta_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	return (cppu::OWeakObject*)new SwXMLExport(rSMgr, EXPORT_META|EXPORT_OASIS);
}

OUString SAL_CALL SwXMLExportSettings_getImplementationName() throw()
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM(
		"com.sun.star.comp.Writer.XMLOasisSettingsExporter" ) );
}

Sequence< OUString > SAL_CALL SwXMLExportSettings_getSupportedServiceNames()
	throw()
{
	const OUString aServiceName(SwXMLExportSettings_getImplementationName());
	const Sequence< OUString > aSeq( &aServiceName, 1 );
	return aSeq;
}

Reference< XInterface > SAL_CALL SwXMLExportSettings_createInstance(
		const Reference< XMultiServiceFactory > & rSMgr)
	throw( Exception )
{
	return (cppu::OWeakObject*)new SwXMLExport(rSMgr, EXPORT_SETTINGS|EXPORT_OASIS);
}

const Sequence< sal_Int8 > & SwXMLExport::getUnoTunnelId() throw()
{
    static Sequence< sal_Int8 > aSeq = ::CreateUnoTunnelId();
	return aSeq;
}

sal_Int64 SAL_CALL SwXMLExport::getSomething( const Sequence< sal_Int8 >& rId )
	throw(RuntimeException)
{
    if( rId.getLength() == 16
        && 0 == rtl_compareMemory( getUnoTunnelId().getConstArray(),
										rId.getConstArray(), 16 ) )
    {
		return sal::static_int_cast< sal_Int64 >( reinterpret_cast< sal_IntPtr >(this) );
    }
	return SvXMLExport::getSomething( rId );
}


// XServiceInfo
// override empty method from parent class
OUString SAL_CALL SwXMLExport::getImplementationName()
    throw(RuntimeException)
{
    switch( getExportFlags() )
    {
        case EXPORT_ALL:
            return SwXMLExport_getImplementationName();
        case (EXPORT_STYLES|EXPORT_MASTERSTYLES|EXPORT_AUTOSTYLES|EXPORT_FONTDECLS):
            return SwXMLExportStyles_getImplementationName();
        case (EXPORT_AUTOSTYLES|EXPORT_CONTENT|EXPORT_SCRIPTS|EXPORT_FONTDECLS):
            return SwXMLExportContent_getImplementationName();
        case EXPORT_META:
            return SwXMLExportMeta_getImplementationName();
        case EXPORT_SETTINGS:
            return SwXMLExportSettings_getImplementationName();
        default:
            // generic name for 'unknown' cases
            return OUString( RTL_CONSTASCII_USTRINGPARAM(
                "com.sun.star.comp.Writer.SwXMLExport" ) );
    }
}


#ifdef XML_CORE_API
void SwXMLExport::ExportCurPaM( sal_Bool bExportWholePaM )
{
	sal_Bool bFirstNode = sal_True;
	sal_Bool bExportWholeNode = bExportWholePaM;

	SwXMLNumRuleInfo aPrevNumInfo;
	SwXMLNumRuleInfo aNextNumInfo;

	while( pCurPaM->GetPoint()->nNode.GetIndex() <
								pCurPaM->GetMark()->nNode.GetIndex() ||
		   ( pCurPaM->GetPoint()->nNode.GetIndex() ==
								pCurPaM->GetMark()->nNode.GetIndex() &&
			 pCurPaM->GetPoint()->nContent.GetIndex() <=
								pCurPaM->GetMark()->nContent.GetIndex() ) )
	{
		SwNode *pNd = pCurPaM->GetNode();

		aNextNumInfo.Set( *pNd );
		ExportListChange( aPrevNumInfo, aNextNumInfo );

		ASSERT( !(pNd->IsGrfNode() || pNd->IsOLENode()),
				"SwXMLExport::exportCurPaM: grf or OLE node unexpected" );
		if( pNd->IsTxtNode() )
		{
			SwTxtNode* pTxtNd = pNd->GetTxtNode();

			if( !bFirstNode )
				pCurPaM->GetPoint()->nContent.Assign( pTxtNd, 0 );

			ExportTxtNode( *pTxtNd, 0, STRING_LEN, bExportWholeNode );
		}
		else if( pNd->IsTableNode() )
		{
			ExportTable( *pNd->GetTableNode() );
		}
		else if( pNd->IsSectionNode() )
		{
			ExportSection( *pNd->GetSectionNode() );
		}
		else if( pNd == &pDoc->GetNodes().GetEndOfContent() )
			break;

		pCurPaM->GetPoint()->nNode++; 	// next node

		sal_uInt32 nPos = pCurPaM->GetPoint()->nNode.GetIndex();

		// if not everything should be exported, the WriteAll flag must be
		// set for all but the first and last node anyway.
		bExportWholeNode = bExportWholePaM ||
						   nPos != pCurPaM->GetMark()->nNode.GetIndex();
		bFirstNode = sal_False;

		aPrevNumInfo = aNextNumInfo;
	}
	aNextNumInfo.Reset();
	ExportListChange( aPrevNumInfo, aNextNumInfo );
}
#endif
