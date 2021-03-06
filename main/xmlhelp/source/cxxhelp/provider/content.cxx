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
#include "precompiled_xmlhelp.hxx"

/**************************************************************************
								TODO
 **************************************************************************

 *************************************************************************/
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/beans/XPropertyAccess.hpp>
#include <com/sun/star/sdbc/XRow.hpp>
#include <com/sun/star/ucb/OpenCommandArgument2.hpp>
#include <com/sun/star/ucb/OpenMode.hpp>
#include <com/sun/star/ucb/XCommandInfo.hpp>
#include <com/sun/star/io/XActiveDataSink.hpp>
#include <com/sun/star/io/XOutputStream.hpp>
#include <com/sun/star/lang/IllegalAccessException.hpp>
#include <com/sun/star/ucb/UnsupportedDataSinkException.hpp>
#include <com/sun/star/io/XActiveDataStreamer.hpp>
#include <com/sun/star/ucb/XPersistentPropertySet.hpp>
#ifndef _VOS_DIAGNOSE_HXX_
#include <vos/diagnose.hxx>
#endif
#include <ucbhelper/contentidentifier.hxx>
#include <ucbhelper/propertyvalueset.hxx>
#ifndef _UCBHELPER_CANCELCOMMANDEXECUTION_HXX
#include <ucbhelper/cancelcommandexecution.hxx>
#endif
#include "content.hxx"
#include "provider.hxx"
#include "resultset.hxx"
#include "databases.hxx"
#include "resultsetfactory.hxx"
#include "resultsetbase.hxx"
#include "resultsetforroot.hxx"
#include "resultsetforquery.hxx"

using namespace com::sun::star;
using namespace chelp;

//=========================================================================
//=========================================================================
//
// Content Implementation.
//
//=========================================================================
//=========================================================================

Content::Content( const uno::Reference< lang::XMultiServiceFactory >& rxSMgr,
				  ::ucbhelper::ContentProviderImplHelper* pProvider,
				  const uno::Reference< ucb::XContentIdentifier >& 
                      Identifier,
				  Databases* pDatabases )
	: ContentImplHelper( rxSMgr, pProvider, Identifier ),
	  m_aURLParameter( Identifier->getContentIdentifier(),pDatabases ),
	  m_pDatabases( pDatabases ) // not owner
{
}

//=========================================================================
// virtual
Content::~Content()
{
}

//=========================================================================
//
// XInterface methods.
//
//=========================================================================

// virtual
void SAL_CALL Content::acquire()
	throw( )
{
	ContentImplHelper::acquire();
}

//=========================================================================
// virtual
void SAL_CALL Content::release()
	throw( )
{
	ContentImplHelper::release();
}

//=========================================================================
// virtual
uno::Any SAL_CALL Content::queryInterface( const uno::Type & rType )
	throw ( uno::RuntimeException )
{
	uno::Any aRet;
 	return aRet.hasValue() ? aRet : ContentImplHelper::queryInterface( rType );
}

//=========================================================================
//
// XTypeProvider methods.
//
//=========================================================================

XTYPEPROVIDER_COMMON_IMPL( Content );

//=========================================================================
// virtual
uno::Sequence< uno::Type > SAL_CALL Content::getTypes()
	throw( uno::RuntimeException )
{
	static cppu::OTypeCollection* pCollection = NULL;

	if ( !pCollection )
	{
		osl::MutexGuard aGuard( osl::Mutex::getGlobalMutex() );
	  	if ( !pCollection )
	  	{
	  		static cppu::OTypeCollection aCollection(
				CPPU_TYPE_REF( lang::XTypeProvider ),
			   	CPPU_TYPE_REF( lang::XServiceInfo ),
			   	CPPU_TYPE_REF( lang::XComponent ),
			   	CPPU_TYPE_REF( ucb::XContent ),
			   	CPPU_TYPE_REF( ucb::XCommandProcessor ),
			   	CPPU_TYPE_REF( beans::XPropertiesChangeNotifier ),
			   	CPPU_TYPE_REF( ucb::XCommandInfoChangeNotifier ),
			   	CPPU_TYPE_REF( beans::XPropertyContainer ),
			   	CPPU_TYPE_REF( beans::XPropertySetInfoChangeNotifier ),
			   	CPPU_TYPE_REF( container::XChild ) );
	  		pCollection = &aCollection;
		}
	}

	return (*pCollection).getTypes();
}

//=========================================================================
//
// XServiceInfo methods.
//
//=========================================================================

// virtual
rtl::OUString SAL_CALL Content::getImplementationName()
	throw( uno::RuntimeException )
{
	return rtl::OUString::createFromAscii( "CHelpContent" );
}

//=========================================================================
// virtual
uno::Sequence< rtl::OUString > SAL_CALL Content::getSupportedServiceNames()
	throw( uno::RuntimeException )
{
	uno::Sequence< rtl::OUString > aSNS( 1 );
	aSNS.getArray()[ 0 ]
			= rtl::OUString::createFromAscii( MYUCP_CONTENT_SERVICE_NAME );
	return aSNS;
}

//=========================================================================
//
// XContent methods.
//
//=========================================================================

// virtual
rtl::OUString SAL_CALL Content::getContentType()
	throw( uno::RuntimeException )
{
	return rtl::OUString::createFromAscii( MYUCP_CONTENT_TYPE );
}

//=========================================================================
//
// XCommandProcessor methods.
//
//=========================================================================

//virtual
void SAL_CALL Content::abort( sal_Int32 /*CommandId*/ )
	throw( uno::RuntimeException )
{
}



class ResultSetForRootFactory
	: public ResultSetFactory
{
private:
	
	uno::Reference< lang::XMultiServiceFactory > m_xSMgr;
	uno::Reference< ucb::XContentProvider >      m_xProvider;
	sal_Int32                                    m_nOpenMode;
	uno::Sequence< beans::Property >             m_seq;
	uno::Sequence< ucb::NumberedSortingInfo >    m_seqSort;
	URLParameter                                 m_aURLParameter;
	Databases*                                   m_pDatabases;
	

public:
	
	ResultSetForRootFactory( 
        const uno::Reference< lang::XMultiServiceFactory >& xSMgr,
        const uno::Reference< ucb::XContentProvider >&  xProvider,
        sal_Int32 nOpenMode,
        const uno::Sequence< beans::Property >& seq,
        const uno::Sequence< ucb::NumberedSortingInfo >& seqSort,
        URLParameter aURLParameter,
        Databases* pDatabases )
		: m_xSMgr( xSMgr ),
		  m_xProvider( xProvider ),
		  m_nOpenMode( nOpenMode ),
		  m_seq( seq ),
		  m_seqSort( seqSort ),
		  m_aURLParameter( aURLParameter ),
		  m_pDatabases( pDatabases )
	{
	}

	ResultSetBase* createResultSet()
	{
		return new ResultSetForRoot( m_xSMgr,
									 m_xProvider,
									 m_nOpenMode,
									 m_seq,
									 m_seqSort,
									 m_aURLParameter,
									 m_pDatabases );
	}
};



class ResultSetForQueryFactory
	: public ResultSetFactory
{
private:
	
	uno::Reference< lang::XMultiServiceFactory > m_xSMgr;
	uno::Reference< ucb::XContentProvider >      m_xProvider;
	sal_Int32                                    m_nOpenMode;
	uno::Sequence< beans::Property >             m_seq;
	uno::Sequence< ucb::NumberedSortingInfo >    m_seqSort;
	URLParameter                                 m_aURLParameter;
	Databases*                                   m_pDatabases;


public:
	
	ResultSetForQueryFactory( 
        const uno::Reference< lang::XMultiServiceFactory >& xSMgr,
        const uno::Reference< ucb::XContentProvider >&  xProvider,
        sal_Int32 nOpenMode,
        const uno::Sequence< beans::Property >& seq,
        const uno::Sequence< ucb::NumberedSortingInfo >& seqSort,
        URLParameter aURLParameter,
        Databases* pDatabases )
		: m_xSMgr( xSMgr ),
		  m_xProvider( xProvider ),
		  m_nOpenMode( nOpenMode ),
		  m_seq( seq ),
		  m_seqSort( seqSort ),
		  m_aURLParameter( aURLParameter ),
		  m_pDatabases( pDatabases )
	{
	}

	ResultSetBase* createResultSet()
	{
		return new ResultSetForQuery( m_xSMgr,
									  m_xProvider,
									  m_nOpenMode,
									  m_seq,
									  m_seqSort,
									  m_aURLParameter,
									  m_pDatabases );
	}
};



// virtual
uno::Any SAL_CALL Content::execute( 
        const ucb::Command& aCommand,
        sal_Int32 CommandId,
        const uno::Reference< ucb::XCommandEnvironment >& Environment )
	throw( uno::Exception, 
           ucb::CommandAbortedException, 
           uno::RuntimeException )
{
	uno::Any aRet;
  
	if ( aCommand.Name.compareToAscii( "getPropertyValues" ) == 0 )
    {
		uno::Sequence< beans::Property > Properties;
		if ( !( aCommand.Argument >>= Properties ) )
		{
			aRet <<= lang::IllegalArgumentException();
			ucbhelper::cancelCommandExecution(aRet,Environment);
		}
      
		aRet <<= getPropertyValues( Properties );
    }
	else if ( aCommand.Name.compareToAscii( "setPropertyValues" ) == 0 )
    {
		uno::Sequence<beans::PropertyValue> propertyValues;
		
		if( ! ( aCommand.Argument >>= propertyValues ) ) {
			aRet <<= lang::IllegalArgumentException();
			ucbhelper::cancelCommandExecution(aRet,Environment);
		}
		
		uno::Sequence< uno::Any > ret(propertyValues.getLength());
		uno::Sequence< beans::Property > props(getProperties(Environment));
		// No properties can be set
		for(sal_Int32 i = 0; i < ret.getLength(); ++i) {
			ret[i] <<= beans::UnknownPropertyException();
			for(sal_Int32 j = 0; j < props.getLength(); ++j)
				if(props[j].Name == propertyValues[i].Name) {
					ret[i] <<= lang::IllegalAccessException();
					break;
				}
		}
		
		aRet <<= ret;
    }
	else if ( aCommand.Name.compareToAscii( "getPropertySetInfo" ) == 0 )
    {
		// Note: Implemented by base class.
		aRet <<= getPropertySetInfo( Environment );
    }
	else if ( aCommand.Name.compareToAscii( "getCommandInfo" ) == 0 )
    {
		// Note: Implemented by base class.
		aRet <<= getCommandInfo( Environment );
    }
	else if ( aCommand.Name.compareToAscii( "open" ) == 0 )
    {
		ucb::OpenCommandArgument2 aOpenCommand;
		if ( !( aCommand.Argument >>= aOpenCommand ) )
		{
			aRet <<= lang::IllegalArgumentException();
			ucbhelper::cancelCommandExecution(aRet,Environment);
		}
      
		uno::Reference< io::XActiveDataSink > xActiveDataSink(
			aOpenCommand.Sink, uno::UNO_QUERY);
		
		if(xActiveDataSink.is())
            m_aURLParameter.open(m_xSMgr,
								 aCommand,
								 CommandId,
								 Environment,
								 xActiveDataSink);
        
		uno::Reference< io::XActiveDataStreamer > xActiveDataStreamer(
			aOpenCommand.Sink, uno::UNO_QUERY);
		
		if(xActiveDataStreamer.is()) {
			aRet <<= ucb::UnsupportedDataSinkException();
			ucbhelper::cancelCommandExecution(aRet,Environment);
		}
        
		uno::Reference< io::XOutputStream > xOutputStream(
			aOpenCommand.Sink, uno::UNO_QUERY);

		if(xOutputStream.is() )
            m_aURLParameter.open(m_xSMgr,
								 aCommand,
								 CommandId,
								 Environment,
								 xOutputStream);
		
		if( m_aURLParameter.isRoot() )
		{
			uno::Reference< ucb::XDynamicResultSet > xSet
				= new DynamicResultSet(
					m_xSMgr,
					this,
					aOpenCommand,
					Environment,
					new ResultSetForRootFactory(
						m_xSMgr,
						m_xProvider.get(),
						aOpenCommand.Mode,
						aOpenCommand.Properties,
						aOpenCommand.SortingInfo,
						m_aURLParameter,
						m_pDatabases));
			aRet <<= xSet;
		}
		else if( m_aURLParameter.isQuery() )
		{
			uno::Reference< ucb::XDynamicResultSet > xSet
				= new DynamicResultSet(	
					m_xSMgr,
					this,
					aOpenCommand,
					Environment,
					new ResultSetForQueryFactory(
						m_xSMgr,
						m_xProvider.get(),
						aOpenCommand.Mode,
						aOpenCommand.Properties,
						aOpenCommand.SortingInfo,
						m_aURLParameter,
						m_pDatabases ) );
			aRet <<= xSet;
		}
    }
	else
    {
		//////////////////////////////////////////////////////////////////
		// Unsupported command
		//////////////////////////////////////////////////////////////////
		aRet <<= ucb::UnsupportedCommandException();
		ucbhelper::cancelCommandExecution(aRet,Environment);
    }
	
	return aRet;
}




//=========================================================================
uno::Reference< sdbc::XRow > Content::getPropertyValues( 
    const uno::Sequence< beans::Property >& rProperties )
{
	osl::MutexGuard aGuard( m_aMutex );
	
	rtl::Reference< ::ucbhelper::PropertyValueSet > xRow = 
		new ::ucbhelper::PropertyValueSet( m_xSMgr );
	
	for ( sal_Int32 n = 0; n < rProperties.getLength(); ++n )
	{
		const beans::Property& rProp = rProperties[n];
		
		if ( rProp.Name.compareToAscii( "ContentType" ) == 0 )
			xRow->appendString( 
				rProp,
				rtl::OUString::createFromAscii(
					"application/vnd.sun.star.help" ) );
		else if( rProp.Name.compareToAscii( "Title" ) == 0 )
			xRow->appendString ( rProp,m_aURLParameter.get_title() );
		else if( rProp.Name.compareToAscii( "IsReadOnly" ) == 0 )
			xRow->appendBoolean( rProp,true );
		else if( rProp.Name.compareToAscii( "IsDocument" ) == 0 )
			xRow->appendBoolean( 
				rProp,
				m_aURLParameter.isFile() || m_aURLParameter.isRoot() );
		else if( rProp.Name.compareToAscii( "IsFolder" ) == 0 )
			xRow->appendBoolean( 
				rProp, 
				! m_aURLParameter.isFile() || m_aURLParameter.isRoot() );
		else if( rProp.Name.compareToAscii( "IsErrorDocument" ) == 0 )
			xRow->appendBoolean( rProp, m_aURLParameter.isErrorDocument() );
		else if( rProp.Name.compareToAscii( "MediaType" ) == 0  )
			if( m_aURLParameter.isPicture() )
				xRow->appendString( 
					rProp,
					rtl::OUString::createFromAscii( "image/gif" ) );
			else if( m_aURLParameter.isActive() )
				xRow->appendString( 
					rProp,
					rtl::OUString::createFromAscii( "text/plain" ) );
			else if( m_aURLParameter.isFile() )
				xRow->appendString( 
					rProp,rtl::OUString::createFromAscii( "text/html" ) );
			else if( m_aURLParameter.isRoot() )
				xRow->appendString(
					rProp,
					rtl::OUString::createFromAscii( "text/css" ) );
			else
				xRow->appendVoid( rProp );
		else if( m_aURLParameter.isModule() )
			if( rProp.Name.compareToAscii( "KeywordList" ) == 0 )
			{
				KeywordInfo *inf = 
					m_pDatabases->getKeyword( m_aURLParameter.get_module(),
											  m_aURLParameter.get_language() );
				
				uno::Any aAny;
                if( inf )
                    aAny <<= inf->getKeywordList();
				xRow->appendObject( rProp,aAny );
			}
			else if( rProp.Name.compareToAscii( "KeywordRef" ) == 0 )
			{
				KeywordInfo *inf =
					m_pDatabases->getKeyword( m_aURLParameter.get_module(),
											  m_aURLParameter.get_language() );
				
				uno::Any aAny;
                if( inf )
                    aAny <<= inf->getIdList();
				xRow->appendObject( rProp,aAny );
			}
			else if( rProp.Name.compareToAscii( "KeywordAnchorForRef" ) == 0 )
			{
				KeywordInfo *inf = 
					m_pDatabases->getKeyword( m_aURLParameter.get_module(),
											  m_aURLParameter.get_language() );
				
				uno::Any aAny;
                if( inf )
                    aAny <<= inf->getAnchorList();
				xRow->appendObject( rProp,aAny );
			}
			else if( rProp.Name.compareToAscii( "KeywordTitleForRef" ) == 0 )
			{
				KeywordInfo *inf = 
					m_pDatabases->getKeyword( m_aURLParameter.get_module(),
											  m_aURLParameter.get_language() );
				
				uno::Any aAny;
                if( inf )
                    aAny <<= inf->getTitleList();
                xRow->appendObject( rProp,aAny );
			}
			else if( rProp.Name.compareToAscii( "SearchScopes" ) == 0 )
			{				
				uno::Sequence< rtl::OUString > seq( 2 );
				seq[0] = rtl::OUString::createFromAscii( "Heading" );
				seq[1] = rtl::OUString::createFromAscii( "FullText" );
				uno::Any aAny;
				aAny <<= seq;
				xRow->appendObject( rProp,aAny );
			}
			else if( rProp.Name.compareToAscii( "Order" ) == 0 )
			{				
				StaticModuleInformation *inf = 
					m_pDatabases->getStaticInformationForModule(
						m_aURLParameter.get_module(),
						m_aURLParameter.get_language() );
				
				uno::Any aAny;
                if( inf )
                    aAny <<= sal_Int32( inf->get_order() );
                xRow->appendObject( rProp,aAny );
			}
			else
				xRow->appendVoid( rProp );
		else if( rProp.Name.compareToAscii( "AnchorName" ) == 0 &&
				 m_aURLParameter.isFile() )
			xRow->appendString( rProp,m_aURLParameter.get_tag() );
		else
			xRow->appendVoid( rProp );
	}
	
	return uno::Reference< sdbc::XRow >( xRow.get() );
}
