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
#include "precompiled_vcl.hxx"

#include <rtl/process.h>
#include <rtl/ref.hxx>

#include <tools/rc.h>

// declare system types in sysdata.hxx
#include <svsys.h>

#include <vcl/window.hxx>
#include <vcl/sysdata.hxx>
#include <vcl/svapp.hxx>
#include <vcl/syschild.hxx>
#include <vcl/unohelp.hxx>

#include <window.h>
#include <salinst.hxx>
#include <salframe.hxx>
#include <salobj.hxx>
#include <svdata.hxx>

#ifdef SOLAR_JAVA
#include <jni.h>
#endif

#include <comphelper/processfactory.hxx>
#include <jvmaccess/virtualmachine.hxx>
#include <com/sun/star/java/XJavaVM.hpp>
#include <com/sun/star/java/XJavaThreadRegister_11.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>

using namespace ::com::sun::star;

// =======================================================================

long ImplSysChildProc( void* pInst, SalObject* /* pObject */,
					   sal_uInt16 nEvent, const void* /* pEvent */ )
{
	SystemChildWindow* pWindow = (SystemChildWindow*)pInst;
	long nRet = 0;

    ImplDelData aDogTag( pWindow );
	switch ( nEvent )
	{
		case SALOBJ_EVENT_GETFOCUS:
			// Focus holen und zwar so, das alle Handler gerufen
			// werden, als ob dieses Fenster den Focus bekommt,
			// ohne das der Frame den Focus wieder klaut
			pWindow->ImplGetFrameData()->mbSysObjFocus = sal_True;
			pWindow->ImplGetFrameData()->mbInSysObjToTopHdl = sal_True;
			pWindow->ToTop( TOTOP_NOGRABFOCUS );
			if( aDogTag.IsDead() )
			    break;
			pWindow->ImplGetFrameData()->mbInSysObjToTopHdl = sal_False;
			pWindow->ImplGetFrameData()->mbInSysObjFocusHdl = sal_True;
			pWindow->GrabFocus();
			if( aDogTag.IsDead() )
			    break;
			pWindow->ImplGetFrameData()->mbInSysObjFocusHdl = sal_False;
			break;

		case SALOBJ_EVENT_LOSEFOCUS:
			// Hintenrum einen LoseFocus ausloesen, das der Status
			// der Fenster dem entsprechenden Activate-Status
			// entspricht
			pWindow->ImplGetFrameData()->mbSysObjFocus = sal_False;
			if ( !pWindow->ImplGetFrameData()->mnFocusId )
			{
				pWindow->ImplGetFrameData()->mbStartFocusState = sal_True;
				Application::PostUserEvent( pWindow->ImplGetFrameData()->mnFocusId, LINK( pWindow->ImplGetFrameWindow(), Window, ImplAsyncFocusHdl ) );
			}
			break;

		case SALOBJ_EVENT_TOTOP:
			pWindow->ImplGetFrameData()->mbInSysObjToTopHdl = sal_True;
			if ( !Application::GetFocusWindow() || pWindow->HasChildPathFocus() )
				pWindow->ToTop( TOTOP_NOGRABFOCUS );
			else
				pWindow->ToTop();
			if( aDogTag.IsDead() )
			    break;
			pWindow->GrabFocus();
			if( aDogTag.IsDead() )
			    break;
			pWindow->ImplGetFrameData()->mbInSysObjToTopHdl = sal_False;
			break;
	}

	return nRet;
}

// =======================================================================

void SystemChildWindow::ImplInitSysChild( Window* pParent, WinBits nStyle, SystemWindowData *pData, sal_Bool bShow )
{
	mpWindowImpl->mpSysObj = ImplGetSVData()->mpDefInst->CreateObject( pParent->ImplGetFrame(), pData, bShow );

	Window::ImplInit( pParent, nStyle, NULL );

	// Wenn es ein richtiges SysChild ist, dann painten wir auch nicht
	if ( GetSystemData() )
	{
		mpWindowImpl->mpSysObj->SetCallback( this, ImplSysChildProc );
		SetParentClipMode( PARENTCLIPMODE_CLIP );
		SetBackground();
	}
}

// -----------------------------------------------------------------------

SystemChildWindow::SystemChildWindow( Window* pParent, WinBits nStyle ) :
	Window( WINDOW_SYSTEMCHILDWINDOW )
{
	ImplInitSysChild( pParent, nStyle, NULL );
}

// -----------------------------------------------------------------------

SystemChildWindow::SystemChildWindow( Window* pParent, WinBits nStyle, SystemWindowData *pData, sal_Bool bShow ) :
	Window( WINDOW_SYSTEMCHILDWINDOW )
{
	ImplInitSysChild( pParent, nStyle, pData, bShow );
}

// -----------------------------------------------------------------------

SystemChildWindow::SystemChildWindow( Window* pParent, const ResId& rResId ) :
	Window( WINDOW_SYSTEMCHILDWINDOW )
{
	rResId.SetRT( RSC_WINDOW );
	WinBits nStyle = ImplInitRes( rResId );
	ImplInitSysChild( pParent, nStyle, NULL );
	ImplLoadRes( rResId );

	if ( !(nStyle & WB_HIDE) )
		Show();
}

// -----------------------------------------------------------------------

SystemChildWindow::~SystemChildWindow()
{
	Hide();
	if ( mpWindowImpl->mpSysObj )
	{
		ImplGetSVData()->mpDefInst->DestroyObject( mpWindowImpl->mpSysObj );
		mpWindowImpl->mpSysObj = NULL;
	}
}

// -----------------------------------------------------------------------

const SystemEnvData* SystemChildWindow::GetSystemData() const
{
	if ( mpWindowImpl->mpSysObj )
		return mpWindowImpl->mpSysObj->GetSystemData();
	else
		return NULL;
}

// -----------------------------------------------------------------------

void SystemChildWindow::EnableEraseBackground( sal_Bool bEnable )
{
	if ( mpWindowImpl->mpSysObj )
		mpWindowImpl->mpSysObj->EnableEraseBackground( bEnable );
}

// -----------------------------------------------------------------------

sal_Bool SystemChildWindow::IsEraseBackgroundEnabled()
{
	if ( mpWindowImpl->mpSysObj )
		return mpWindowImpl->mpSysObj->IsEraseBackgroundEnabled();
    else
        return sal_False;
}

// -----------------------------------------------------------------------

void SystemChildWindow::ImplTestJavaException( void* pEnv )
{
#ifdef SOLAR_JAVA
    JNIEnv*     pJavaEnv = reinterpret_cast< JNIEnv* >( pEnv );
    jthrowable  jtThrowable = pJavaEnv->ExceptionOccurred();

    if( jtThrowable )
    { // is it a java exception ?
#if OSL_DEBUG_LEVEL > 1
        pJavaEnv->ExceptionDescribe();
#endif // OSL_DEBUG_LEVEL > 1
        pJavaEnv->ExceptionClear();

        jclass          jcThrowable = pJavaEnv->FindClass("java/lang/Throwable");
        jmethodID       jmThrowable_getMessage = pJavaEnv->GetMethodID(jcThrowable, "getMessage", "()Ljava/lang/String;");
        jstring         jsMessage = (jstring) pJavaEnv->CallObjectMethod(jtThrowable, jmThrowable_getMessage);
            ::rtl::OUString ouMessage;

            if(jsMessage)
            {
                const jchar * jcMessage = pJavaEnv->GetStringChars(jsMessage, NULL);
                ouMessage = ::rtl::OUString(jcMessage);
                pJavaEnv->ReleaseStringChars(jsMessage, jcMessage);
            }

            throw uno::RuntimeException(ouMessage, uno::Reference<uno::XInterface>());
    }
#endif // SOLAR_JAVA
}

// -----------------------------------------------------------------------

sal_IntPtr SystemChildWindow::GetParentWindowHandle( sal_Bool bUseJava )
{
    sal_IntPtr nRet = 0;
    
    (void)bUseJava;
#if defined WNT
    nRet = reinterpret_cast< sal_IntPtr >( GetSystemData()->hWnd );
#elif defined OS2
    nRet = reinterpret_cast< sal_IntPtr >( (long)GetSystemData()->hWnd );
#elif defined QUARTZ
    // FIXME: this is wrong
    nRet = reinterpret_cast< sal_IntPtr >( GetSystemData()->mpNSView );
#elif defined UNX
    if( !bUseJava )
    {
        nRet = (sal_IntPtr) GetSystemData()->aWindow;
    }
#ifdef SOLAR_JAVA
    else
    {
        uno::Reference< lang::XMultiServiceFactory > xFactory( vcl::unohelper::GetMultiServiceFactory() );

        if( xFactory.is() && ( GetSystemData()->aWindow > 0 ) )
        {
            try
            {
                    ::rtl::Reference< ::jvmaccess::VirtualMachine > xVM;
                    uno::Reference< java::XJavaVM >                 xJavaVM( xFactory->createInstance( rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("com.sun.star.java.JavaVirtualMachine") ) ), uno::UNO_QUERY );
                    uno::Sequence< sal_Int8 >                       aProcessID( 17 );

                    rtl_getGlobalProcessId( (sal_uInt8*) aProcessID.getArray() );
                    aProcessID[ 16 ] = 0;
                    OSL_ENSURE(sizeof (sal_Int64) >= sizeof (jvmaccess::VirtualMachine *), "Pointer cannot be represented as sal_Int64");
                    sal_Int64 nPointer = reinterpret_cast< sal_Int64 >( static_cast< jvmaccess::VirtualMachine * >(0));
                    xJavaVM->getJavaVM(aProcessID) >>= nPointer;
                    xVM = reinterpret_cast< jvmaccess::VirtualMachine * >(nPointer);

                    if( xVM.is() )
                    {
                        try
                        {
                                ::jvmaccess::VirtualMachine::AttachGuard    aVMAttachGuard( xVM );
                                JNIEnv*                                     pEnv = aVMAttachGuard.getEnvironment();

                                jclass jcToolkit = pEnv->FindClass("java/awt/Toolkit");
                                ImplTestJavaException(pEnv);

                                jmethodID jmToolkit_getDefaultToolkit = pEnv->GetStaticMethodID( jcToolkit, "getDefaultToolkit", "()Ljava/awt/Toolkit;" );
                                ImplTestJavaException(pEnv);

                                pEnv->CallStaticObjectMethod(jcToolkit, jmToolkit_getDefaultToolkit);
                                ImplTestJavaException(pEnv);

                                jclass jcMotifAppletViewer = pEnv->FindClass("sun/plugin/navig/motif/MotifAppletViewer");
                                if( pEnv->ExceptionOccurred() )
                                {
                                    pEnv->ExceptionClear();

                                    jcMotifAppletViewer = pEnv->FindClass( "sun/plugin/viewer/MNetscapePluginContext");
                                    ImplTestJavaException(pEnv);
                                }

                                jclass jcClassLoader = pEnv->FindClass("java/lang/ClassLoader");
                                ImplTestJavaException(pEnv);

                                jmethodID jmClassLoader_loadLibrary = pEnv->GetStaticMethodID( jcClassLoader, "loadLibrary", "(Ljava/lang/Class;Ljava/lang/String;Z)V");
                                ImplTestJavaException(pEnv);

                                jstring jsplugin = pEnv->NewStringUTF("javaplugin_jni");
                                ImplTestJavaException(pEnv);

                                pEnv->CallStaticVoidMethod(jcClassLoader, jmClassLoader_loadLibrary, jcMotifAppletViewer, jsplugin, JNI_FALSE);
                                ImplTestJavaException(pEnv);

                                jmethodID jmMotifAppletViewer_getWidget = pEnv->GetStaticMethodID( jcMotifAppletViewer, "getWidget", "(IIIII)I" );
                                ImplTestJavaException(pEnv);

                                const Size aSize( GetOutputSizePixel() );
                                jint ji_widget = pEnv->CallStaticIntMethod( jcMotifAppletViewer, jmMotifAppletViewer_getWidget,
                                        GetSystemData()->aWindow, 0, 0, aSize.Width(), aSize.Height() );
                                ImplTestJavaException(pEnv);

                                nRet = static_cast< sal_IntPtr >( ji_widget );
                        }
                        catch( uno::RuntimeException& )
                        {
                        }

                        if( !nRet )
                            nRet = static_cast< sal_IntPtr >( GetSystemData()->aWindow );
                    }
            }
            catch( ... )
            {
            }
        }
    }
#endif // SOLAR_JAVA
#else // WNT || QUARTZ || UNX
#endif

    return nRet;
}
