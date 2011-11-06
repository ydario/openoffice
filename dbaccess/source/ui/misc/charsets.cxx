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
#include "precompiled_dbaccess.hxx"

#ifndef _DBAUI_CHARSETS_HXX_
#include "charsets.hxx"
#endif
#ifndef _TOOLS_DEBUG_HXX
#include <tools/debug.hxx>
#endif
#ifndef _DBU_MISC_HRC_
#include "dbu_misc.hrc"
#endif
#ifndef _RTL_TENCINFO_H
#include <rtl/tencinfo.h>
#endif
#ifndef _TOOLS_RCID_H 
#include <tools/rcid.h>
#endif
#ifndef _DBAUI_LOCALRESACCESS_HXX_
#include "localresaccess.hxx"
#endif

//.........................................................................
namespace dbaui
{
//.........................................................................
	using namespace ::dbtools;

	//=========================================================================
	//= OCharsetDisplay
	//=========================================================================
	//-------------------------------------------------------------------------
	OCharsetDisplay::OCharsetDisplay()
		:OCharsetMap()
		,SvxTextEncodingTable()
	{
		{
			LocalResourceAccess aCharsetStrings( RSC_CHARSETS, RSC_RESOURCE );
			m_aSystemDisplayName = String( ModuleRes( 1 ) );
		}
	}

	//-------------------------------------------------------------------------
	sal_Bool OCharsetDisplay::approveEncoding( const rtl_TextEncoding _eEncoding, const rtl_TextEncodingInfo& _rInfo ) const
	{
		if ( !OCharsetMap::approveEncoding( _eEncoding, _rInfo ) )
			return sal_False;

		if ( RTL_TEXTENCODING_DONTKNOW == _eEncoding )
			return sal_True;

		return 0 != GetTextString( _eEncoding ).Len();
	}

	//-------------------------------------------------------------------------
	OCharsetDisplay::const_iterator OCharsetDisplay::begin() const
	{
		return const_iterator( this, OCharsetMap::begin() );
	}

	//-------------------------------------------------------------------------
	OCharsetDisplay::const_iterator OCharsetDisplay::end() const
	{
		return const_iterator( this, OCharsetMap::end() );
	}

	//-------------------------------------------------------------------------
	OCharsetDisplay::const_iterator OCharsetDisplay::findEncoding(const rtl_TextEncoding _eEncoding) const
	{
		OCharsetMap::const_iterator aBaseIter = OCharsetMap::find(_eEncoding);
		return const_iterator( this, aBaseIter );
	}

	//-------------------------------------------------------------------------
	OCharsetDisplay::const_iterator OCharsetDisplay::findIanaName(const ::rtl::OUString& _rIanaName) const
	{
		OCharsetMap::const_iterator aBaseIter = OCharsetMap::find(_rIanaName, OCharsetMap::IANA());
		return const_iterator( this, aBaseIter );
	}

	//-------------------------------------------------------------------------
	OCharsetDisplay::const_iterator OCharsetDisplay::findDisplayName(const ::rtl::OUString& _rDisplayName) const
	{
		rtl_TextEncoding eEncoding = RTL_TEXTENCODING_DONTKNOW;
		if ( _rDisplayName != m_aSystemDisplayName )
		{
			eEncoding = GetTextEncoding( _rDisplayName );
			OSL_ENSURE( RTL_TEXTENCODING_DONTKNOW != eEncoding,
				"OCharsetDisplay::find: non-empty display name, but DONTKNOW!" );
		}
		return const_iterator( this, OCharsetMap::find( eEncoding ) );
	}

	//=========================================================================
	//= CharsetDisplayDerefHelper
	//=========================================================================
	//-------------------------------------------------------------------------
	CharsetDisplayDerefHelper::CharsetDisplayDerefHelper(const CharsetDisplayDerefHelper& _rSource)
		:CharsetDisplayDerefHelper_Base(_rSource)
		,m_sDisplayName(m_sDisplayName)
	{
	}

	//-------------------------------------------------------------------------
	CharsetDisplayDerefHelper::CharsetDisplayDerefHelper(const CharsetDisplayDerefHelper_Base& _rBase, const ::rtl::OUString& _rDisplayName)
		:CharsetDisplayDerefHelper_Base(_rBase)
		,m_sDisplayName(_rDisplayName)
	{
		DBG_ASSERT( m_sDisplayName.getLength(), "CharsetDisplayDerefHelper::CharsetDisplayDerefHelper: invalid display name!" );
	}

	//=========================================================================
	//= OCharsetDisplay::ExtendedCharsetIterator
	//=========================================================================
	//-------------------------------------------------------------------------
	OCharsetDisplay::ExtendedCharsetIterator::ExtendedCharsetIterator( const OCharsetDisplay* _pContainer, const base_iterator& _rPosition )
		:m_pContainer(_pContainer)
		,m_aPosition(_rPosition)
	{
		DBG_ASSERT(m_pContainer, "OCharsetDisplay::ExtendedCharsetIterator::ExtendedCharsetIterator : invalid container!");
	}

	//-------------------------------------------------------------------------
	OCharsetDisplay::ExtendedCharsetIterator::ExtendedCharsetIterator(const ExtendedCharsetIterator& _rSource)
		:m_pContainer( _rSource.m_pContainer )
		,m_aPosition( _rSource.m_aPosition )
	{
	}

	//-------------------------------------------------------------------------
	CharsetDisplayDerefHelper OCharsetDisplay::ExtendedCharsetIterator::operator*() const
	{
		DBG_ASSERT( m_aPosition != m_pContainer->OCharsetDisplay_Base::end(), "OCharsetDisplay::ExtendedCharsetIterator::operator* : invalid position!");

		rtl_TextEncoding eEncoding = (*m_aPosition).getEncoding();
		return CharsetDisplayDerefHelper(
			*m_aPosition,
			RTL_TEXTENCODING_DONTKNOW == eEncoding ? m_pContainer->m_aSystemDisplayName : (::rtl::OUString)m_pContainer->GetTextString( eEncoding )
		);
	}

	//-------------------------------------------------------------------------
	const OCharsetDisplay::ExtendedCharsetIterator&	OCharsetDisplay::ExtendedCharsetIterator::operator++()
	{
		DBG_ASSERT( m_aPosition != m_pContainer->OCharsetDisplay_Base::end(), "OCharsetDisplay::ExtendedCharsetIterator::operator++ : invalid position!");
		if ( m_aPosition != m_pContainer->OCharsetDisplay_Base::end() )
			++m_aPosition;
		return *this;
	}

	//-------------------------------------------------------------------------
	const OCharsetDisplay::ExtendedCharsetIterator&	OCharsetDisplay::ExtendedCharsetIterator::operator--()
	{
		DBG_ASSERT( m_aPosition != m_pContainer->OCharsetDisplay_Base::begin(), "OCharsetDisplay::ExtendedCharsetIterator::operator-- : invalid position!");
		if ( m_aPosition != m_pContainer->OCharsetDisplay_Base::begin() )
			--m_aPosition;
		return *this;
	}

	//-------------------------------------------------------------------------
	bool operator==(const OCharsetDisplay::ExtendedCharsetIterator& lhs, const OCharsetDisplay::ExtendedCharsetIterator& rhs)
	{
		return (lhs.m_pContainer == rhs.m_pContainer) && (lhs.m_aPosition == rhs.m_aPosition);
	}

//.........................................................................
}	// namespace dbaui
//.........................................................................

