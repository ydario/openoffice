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
#ifndef DBACCESS_JACCESS_HXX
#include "JAccess.hxx"
#endif
#ifndef DBAUI_JOINTABLEVIEW_HXX
#include "JoinTableView.hxx"
#endif
#ifndef DBAUI_JOINTABLEVIEW_HXX
#include "JoinTableView.hxx"
#endif
#ifndef DBAUI_TABLEWINDOW_HXX
#include "TableWindow.hxx"
#endif
#ifndef _COM_SUN_STAR_ACCESSIBILITY_ACCESSIBLEROLE_HPP_
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#endif
#ifndef DBAUI_JOINDESIGNVIEW_HXX
#include "JoinDesignView.hxx"
#endif
#ifndef DBAUI_JOINCONTROLLER_HXX
#include "JoinController.hxx"
#endif
#ifndef DBAUI_TABLECONNECTION_HXX
#include "TableConnection.hxx"
#endif

namespace dbaui
{
	using namespace ::com::sun::star::accessibility;
	using namespace ::com::sun::star::uno;
	using namespace ::com::sun::star::beans;
	using namespace ::com::sun::star::lang;

	OJoinDesignViewAccess::OJoinDesignViewAccess(OJoinTableView* _pTableView)
		:VCLXAccessibleComponent(_pTableView->GetComponentInterface().is() ? _pTableView->GetWindowPeer() : NULL)
		,m_pTableView(_pTableView)
	{
	}
	// -----------------------------------------------------------------------------
	::rtl::OUString SAL_CALL OJoinDesignViewAccess::getImplementationName() throw(RuntimeException)
	{
		return getImplementationName_Static();
	}
	// -----------------------------------------------------------------------------
	::rtl::OUString OJoinDesignViewAccess::getImplementationName_Static(void) throw( RuntimeException )
	{
		return ::rtl::OUString::createFromAscii("org.openoffice.comp.dbu.JoinViewAccessibility");
	}
    // -----------------------------------------------------------------------------
    void OJoinDesignViewAccess::clearTableView()
    {
        ::osl::MutexGuard aGuard( m_aMutex );
        m_pTableView = NULL;
    }
	// -----------------------------------------------------------------------------
	// XAccessibleContext
	sal_Int32 SAL_CALL OJoinDesignViewAccess::getAccessibleChildCount(  ) throw (RuntimeException)
	{
		// TODO may be this will change to only visible windows
		// this is the same assumption mt implements
		::osl::MutexGuard aGuard( m_aMutex  );
		sal_Int32 nChildCount = 0;
		if ( m_pTableView )
			nChildCount = m_pTableView->GetTabWinCount() + m_pTableView->getTableConnections()->size();
		return nChildCount;
	}
	// -----------------------------------------------------------------------------
	Reference< XAccessible > SAL_CALL OJoinDesignViewAccess::getAccessibleChild( sal_Int32 i ) throw (IndexOutOfBoundsException,RuntimeException)
	{
		Reference< XAccessible > aRet;
		::osl::MutexGuard aGuard( m_aMutex  );
		if(i >= 0 && i < getAccessibleChildCount() && m_pTableView )
		{
			// check if we should return a table window or a connection
			sal_Int32 nTableWindowCount = m_pTableView->GetTabWinCount();
			if( i < nTableWindowCount )
			{
				OJoinTableView::OTableWindowMap::iterator aIter = m_pTableView->GetTabWinMap()->begin();
				for (sal_Int32 j=i; j; ++aIter,--j)
					;
				aRet = aIter->second->GetAccessible();
			}
			else if( size_t(i - nTableWindowCount) < m_pTableView->getTableConnections()->size() )
				aRet = (*m_pTableView->getTableConnections())[i - nTableWindowCount]->GetAccessible();
		}
		else
			throw IndexOutOfBoundsException();
		return aRet;
	}
	// -----------------------------------------------------------------------------
	sal_Bool OJoinDesignViewAccess::isEditable() const
	{
		return m_pTableView && !m_pTableView->getDesignView()->getController().isReadOnly();
	}
	// -----------------------------------------------------------------------------
	sal_Int16 SAL_CALL OJoinDesignViewAccess::getAccessibleRole(  ) throw (RuntimeException)
	{
		return AccessibleRole::VIEW_PORT;
	}
	// -----------------------------------------------------------------------------
	Reference< XAccessibleContext > SAL_CALL OJoinDesignViewAccess::getAccessibleContext(  ) throw (::com::sun::star::uno::RuntimeException)
	{
		return this;
	}
	// -----------------------------------------------------------------------------
	// -----------------------------------------------------------------------------
	// XInterface
	// -----------------------------------------------------------------------------
	IMPLEMENT_FORWARD_XINTERFACE2( OJoinDesignViewAccess, VCLXAccessibleComponent, OJoinDesignViewAccess_BASE )
	// -----------------------------------------------------------------------------
	// XTypeProvider
	// -----------------------------------------------------------------------------
	IMPLEMENT_FORWARD_XTYPEPROVIDER2( OJoinDesignViewAccess, VCLXAccessibleComponent, OJoinDesignViewAccess_BASE )
}

// -----------------------------------------------------------------------------

