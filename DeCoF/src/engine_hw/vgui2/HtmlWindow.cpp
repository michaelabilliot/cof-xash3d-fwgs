//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: hosts the Internet Explorer web browser control for vgui2::IHTML
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <string.h>
#include <ole2.h>
#include <ocidl.h>
#include <docobj.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <mshtmdid.h>
#include <mshtmhst.h>
#include <comutil.h>

#include "HtmlWindow.h"
#include "MemoryBitmap.h"
#include "UtlVector.h"
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

using namespace vgui2;

namespace vgui2
{
extern ISurface *g_pSurface;
}

class FS_IOleInPlaceFrame;
class FS_IOleInPlaceSiteWindowless;
class FS_IOleClientSite;
class FS_IOleControlSite;
class FS_IOleCommandTarget;
class FS_IOleItemContainer;
class FS_IDispatch;
class FS_DWebBrowserEvents2;
class FS_DHTMLDocumentEvents2;
class FS_IAdviseSink2;
class FS_IAdviseSinkEx;
class FS_IDocHostUIHandler;

//-----------------------------------------------------------------------------
// Purpose: the client site of the browser control; hands out one helper
//			object per interface the control asks for
//-----------------------------------------------------------------------------
class FrameSite : public IUnknown
{
	friend class HtmlWindow;
	friend class FS_IOleInPlaceFrame;
	friend class FS_IOleInPlaceSiteWindowless;
	friend class FS_IOleClientSite;
	friend class FS_IOleControlSite;
	friend class FS_IOleCommandTarget;
	friend class FS_IOleItemContainer;
	friend class FS_IDispatch;
	friend class FS_DWebBrowserEvents2;
	friend class FS_DHTMLDocumentEvents2;
	friend class FS_IAdviseSink2;
	friend class FS_IAdviseSinkEx;
	friend class FS_IDocHostUIHandler;

public:
	FrameSite(HtmlWindow *win, bool AllowJavaScript);
	~FrameSite();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject);
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

protected:
	FS_IAdviseSinkEx *m_IAdviseSinkEx_guess;
	int m_cRef_guess;

	FS_IOleInPlaceFrame *m_IOleInPlaceFrame_guess;
	FS_IOleInPlaceSiteWindowless *m_IOleInPlaceSiteWindowless_guess;
	FS_IOleClientSite *m_IOleClientSite_guess;
	FS_IOleControlSite *m_IOleControlSite_guess;
	FS_IOleCommandTarget *m_IOleCommandTarget_guess;
	FS_IOleItemContainer *m_IOleItemContainer_guess;
	FS_IDispatch *m_IDispatch_guess;
	FS_DWebBrowserEvents2 *m_DWebBrowserEvents2_guess;
	FS_DHTMLDocumentEvents2 *m_DHTMLDocumentEvents2_guess;
	FS_IAdviseSink2 *m_IAdviseSink2_guess;
	FS_IDocHostUIHandler *m_IDocHostUIHandler_guess;

	HtmlWindow *m_window_guess;

	HDC m_hDCBuffer_guess;
	HWND m_hWndParent_guess;

	bool m_bSupportsWindowlessActivation_guess;
	bool m_bInPlaceLocked_guess;
	bool m_bInPlaceActive_guess;
	bool m_bUIActive_guess;
	bool m_bWindowless_guess;

	LCID m_nAmbientLocale_guess;
	COLORREF m_clrAmbientForeColor_guess;
	COLORREF m_clrAmbientBackColor_guess;
	bool m_bAmbientShowHatching_guess;
	bool m_bAmbientShowGrabHandles_guess;
	bool m_bAmbientUserMode_guess;
	bool m_bAmbientAppearance_guess;
};

class FS_IOleInPlaceFrame : public IOleInPlaceFrame
{
public:
	FS_IOleInPlaceFrame(FrameSite *fs) { m_fs = fs; }
	~FS_IOleInPlaceFrame() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IOleWindow
	STDMETHODIMP GetWindow(HWND *);
	STDMETHODIMP ContextSensitiveHelp(BOOL);
	// IOleInPlaceUIWindow
	STDMETHODIMP GetBorder(LPRECT);
	STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS);
	STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS);
	STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject *, LPCOLESTR);
	// IOleInPlaceFrame
	STDMETHODIMP InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS);
	STDMETHODIMP SetMenu(HMENU, HOLEMENU, HWND);
	STDMETHODIMP RemoveMenus(HMENU);
	STDMETHODIMP SetStatusText(LPCOLESTR);
	STDMETHODIMP EnableModeless(BOOL);
	STDMETHODIMP TranslateAccelerator(LPMSG, WORD);

protected:
	FrameSite *m_fs;
};

class FS_IOleInPlaceSiteWindowless : public IOleInPlaceSiteWindowless
{
public:
	FS_IOleInPlaceSiteWindowless(FrameSite *fs) { m_fs = fs; }
	~FS_IOleInPlaceSiteWindowless() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IOleWindow
	STDMETHODIMP GetWindow(HWND *phwnd) { return m_fs->m_IOleInPlaceFrame_guess->GetWindow(phwnd); }
	STDMETHODIMP ContextSensitiveHelp(BOOL fEnterMode) { return m_fs->m_IOleInPlaceFrame_guess->ContextSensitiveHelp(fEnterMode); }
	// IOleInPlaceSite
	STDMETHODIMP CanInPlaceActivate();
	STDMETHODIMP OnInPlaceActivate();
	STDMETHODIMP OnUIActivate();
	STDMETHODIMP GetWindowContext(IOleInPlaceFrame **, IOleInPlaceUIWindow **, LPRECT, LPRECT, LPOLEINPLACEFRAMEINFO);
	STDMETHODIMP Scroll(SIZE);
	STDMETHODIMP OnUIDeactivate(BOOL);
	STDMETHODIMP OnInPlaceDeactivate();
	STDMETHODIMP DiscardUndoState();
	STDMETHODIMP DeactivateAndUndo();
	STDMETHODIMP OnPosRectChange(LPCRECT);
	// IOleInPlaceSiteEx
	STDMETHODIMP OnInPlaceActivateEx(BOOL *, DWORD);
	STDMETHODIMP OnInPlaceDeactivateEx(BOOL);
	STDMETHODIMP RequestUIActivate();
	// IOleInPlaceSiteWindowless
	STDMETHODIMP CanWindowlessActivate();
	STDMETHODIMP GetCapture();
	STDMETHODIMP SetCapture(BOOL);
	STDMETHODIMP GetFocus();
	STDMETHODIMP SetFocus(BOOL);
	STDMETHODIMP GetDC(LPCRECT, DWORD, HDC *);
	STDMETHODIMP ReleaseDC(HDC);
	STDMETHODIMP InvalidateRect(LPCRECT, BOOL);
	STDMETHODIMP InvalidateRgn(HRGN, BOOL);
	STDMETHODIMP ScrollRect(INT, INT, LPCRECT, LPCRECT);
	STDMETHODIMP AdjustRect(LPRECT);
	STDMETHODIMP OnDefWindowMessage(UINT, WPARAM, LPARAM, LRESULT *);

protected:
	FrameSite *m_fs;
};

class FS_IOleClientSite : public IOleClientSite
{
public:
	FS_IOleClientSite(FrameSite *fs) { m_fs = fs; }
	~FS_IOleClientSite() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IOleClientSite
	STDMETHODIMP SaveObject();
	STDMETHODIMP GetMoniker(DWORD, DWORD, IMoniker **);
	STDMETHODIMP GetContainer(LPOLECONTAINER FAR *);
	STDMETHODIMP ShowObject();
	STDMETHODIMP OnShowWindow(BOOL);
	STDMETHODIMP RequestNewObjectLayout();

protected:
	FrameSite *m_fs;
};

class FS_IOleControlSite : public IOleControlSite
{
public:
	FS_IOleControlSite(FrameSite *fs) { m_fs = fs; }
	~FS_IOleControlSite() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IOleControlSite
	STDMETHODIMP OnControlInfoChanged();
	STDMETHODIMP LockInPlaceActive(BOOL);
	STDMETHODIMP GetExtendedControl(IDispatch **);
	STDMETHODIMP TransformCoords(POINTL *, POINTF *, DWORD);
	STDMETHODIMP TranslateAccelerator(LPMSG, DWORD);
	STDMETHODIMP Unused_guess(DWORD, DWORD, DWORD, DWORD);
	STDMETHODIMP OnFocus(BOOL);
	STDMETHODIMP ShowPropertyFrame();

protected:
	FrameSite *m_fs;
};

class FS_IOleCommandTarget : public IOleCommandTarget
{
public:
	FS_IOleCommandTarget(FrameSite *fs) { m_fs = fs; }
	~FS_IOleCommandTarget() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IOleCommandTarget
	STDMETHODIMP QueryStatus(const GUID *, ULONG, OLECMD[], OLECMDTEXT *);
	STDMETHODIMP Exec(const GUID *, DWORD, DWORD, VARIANTARG *, VARIANTARG *);

protected:
	FrameSite *m_fs;
};

class FS_IOleItemContainer : public IOleItemContainer
{
public:
	FS_IOleItemContainer(FrameSite *fs) { m_fs = fs; }
	~FS_IOleItemContainer() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IParseDisplayName
	STDMETHODIMP ParseDisplayName(IBindCtx *, LPOLESTR, ULONG *, IMoniker **);
	// IOleContainer
	STDMETHODIMP EnumObjects(DWORD, IEnumUnknown **);
	STDMETHODIMP LockContainer(BOOL);
	// IOleItemContainer
	HRESULT STDMETHODCALLTYPE GetObject(LPOLESTR, DWORD, IBindCtx *, REFIID, void **);
	STDMETHODIMP GetObjectStorage(LPOLESTR, IBindCtx *, REFIID, void **);
	STDMETHODIMP IsRunning(LPOLESTR);

protected:
	FrameSite *m_fs;
};

class FS_IDispatch : public IDispatch
{
public:
	FS_IDispatch(FrameSite *fs) { m_fs = fs; m_iDownloadCount_guess = 0; m_bOnLink_guess = false; m_bAllowJavaScript_guess = true; }
	~FS_IDispatch() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IDispatch
	STDMETHODIMP GetIDsOfNames(REFIID, OLECHAR **, unsigned int, LCID, DISPID *);
	STDMETHODIMP GetTypeInfo(unsigned int, LCID, ITypeInfo **);
	STDMETHODIMP GetTypeInfoCount(unsigned int *);
	STDMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, unsigned int *);

	void SetAllowJavaScript_guess(bool state) { m_bAllowJavaScript_guess = state; }

protected:
	FrameSite *m_fs;
	unsigned int m_iDownloadCount_guess;
	bool m_bAllowJavaScript_guess;
	bool m_bOnLink_guess;
};

class FS_DWebBrowserEvents2 : public DWebBrowserEvents2
{
public:
	FS_DWebBrowserEvents2(FrameSite *fs) { m_fs = fs; }
	~FS_DWebBrowserEvents2() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IDispatch
	STDMETHODIMP GetIDsOfNames(REFIID riid, OLECHAR **rgszNames, unsigned int cNames, LCID lcid, DISPID *rgDispId) { return m_fs->m_IDispatch_guess->GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId); }
	STDMETHODIMP GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo **ppTInfo) { return m_fs->m_IDispatch_guess->GetTypeInfo(iTInfo, lcid, ppTInfo); }
	STDMETHODIMP GetTypeInfoCount(unsigned int *pctinfo) { return m_fs->m_IDispatch_guess->GetTypeInfoCount(pctinfo); }
	STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr) { return m_fs->m_IDispatch_guess->Invoke(dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr); }

protected:
	FrameSite *m_fs;
};

class FS_DHTMLDocumentEvents2 : public HTMLDocumentEvents
{
public:
	FS_DHTMLDocumentEvents2(FrameSite *fs) { m_fs = fs; }
	~FS_DHTMLDocumentEvents2() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IDispatch
	STDMETHODIMP GetIDsOfNames(REFIID riid, OLECHAR **rgszNames, unsigned int cNames, LCID lcid, DISPID *rgDispId) { return m_fs->m_IDispatch_guess->GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId); }
	STDMETHODIMP GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo **ppTInfo) { return m_fs->m_IDispatch_guess->GetTypeInfo(iTInfo, lcid, ppTInfo); }
	STDMETHODIMP GetTypeInfoCount(unsigned int *pctinfo) { return m_fs->m_IDispatch_guess->GetTypeInfoCount(pctinfo); }
	STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr) { return m_fs->m_IDispatch_guess->Invoke(dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr); }

protected:
	FrameSite *m_fs;
};

class FS_IAdviseSink2 : public IAdviseSink2
{
public:
	FS_IAdviseSink2(FrameSite *fs) { m_fs = fs; }
	~FS_IAdviseSink2() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IAdviseSink
	void STDMETHODCALLTYPE OnDataChange(FORMATETC *, STGMEDIUM *);
	void STDMETHODCALLTYPE OnViewChange(DWORD, LONG);
	void STDMETHODCALLTYPE OnRename(IMoniker *);
	void STDMETHODCALLTYPE OnSave();
	void STDMETHODCALLTYPE OnClose();
	// IAdviseSink2
	void STDMETHODCALLTYPE OnLinkSrcChange(IMoniker *);

protected:
	FrameSite *m_fs;
};

class FS_IAdviseSinkEx : public IAdviseSinkEx
{
public:
	FS_IAdviseSinkEx(FrameSite *fs) { m_fs = fs; }
	~FS_IAdviseSinkEx() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IAdviseSink
	void STDMETHODCALLTYPE OnDataChange(FORMATETC *pFormatetc, STGMEDIUM *pStgmed) { m_fs->m_IAdviseSink2_guess->OnDataChange(pFormatetc, pStgmed); }
	void STDMETHODCALLTYPE OnViewChange(DWORD dwAspect, LONG lindex) { m_fs->m_IAdviseSink2_guess->OnViewChange(dwAspect, lindex); }
	void STDMETHODCALLTYPE OnRename(IMoniker *pmk) { m_fs->m_IAdviseSink2_guess->OnRename(pmk); }
	void STDMETHODCALLTYPE OnSave() { m_fs->m_IAdviseSink2_guess->OnSave(); }
	void STDMETHODCALLTYPE OnClose() { m_fs->m_IAdviseSink2_guess->OnClose(); }
	// IAdviseSinkEx
	void STDMETHODCALLTYPE OnViewStatusChange(DWORD);

protected:
	FrameSite *m_fs;
};

class FS_IDocHostUIHandler : public IDocHostUIHandler
{
public:
	FS_IDocHostUIHandler(FrameSite *fs) { m_fs = fs; }
	~FS_IDocHostUIHandler() {}

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) { return m_fs->QueryInterface(iid, ppvObject); }
	ULONG STDMETHODCALLTYPE AddRef() { return m_fs->AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return m_fs->Release(); }
	// IDocHostUIHandler
	STDMETHODIMP ShowContextMenu(DWORD dwID, POINT *ppt, IUnknown *pcmdtReserved, IDispatch *pdispReserved) { return S_FALSE; }
	STDMETHODIMP GetHostInfo(DOCHOSTUIINFO *pInfo);
	STDMETHODIMP ShowUI(DWORD dwID, IOleInPlaceActiveObject *pActiveObject, IOleCommandTarget *pCommandTarget, IOleInPlaceFrame *pFrame, IOleInPlaceUIWindow *pDoc) { return S_FALSE; }
	STDMETHODIMP HideUI() { return S_OK; }
	STDMETHODIMP UpdateUI() { return S_OK; }
	STDMETHODIMP EnableModeless(BOOL fEnable) { return S_OK; }
	STDMETHODIMP OnDocWindowActivate(BOOL fActivate) { return S_OK; }
	STDMETHODIMP OnFrameWindowActivate(BOOL fActivate) { return S_OK; }
	STDMETHODIMP ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow *pUIWindow, BOOL fRameWindow) { return S_OK; }
	STDMETHODIMP TranslateAccelerator(LPMSG lpMsg, const GUID *pguidCmdGroup, DWORD nCmdID) { return S_FALSE; }
	STDMETHODIMP GetOptionKeyPath(LPOLESTR *pchKey, DWORD dw) { return S_FALSE; }
	STDMETHODIMP GetDropTarget(IDropTarget *pDropTarget, IDropTarget **ppDropTarget) { return E_FAIL; }
	STDMETHODIMP GetExternal(IDispatch **ppDispatch) { ppDispatch = NULL; return S_FALSE; }
	STDMETHODIMP TranslateUrl(DWORD dwTranslate, OLECHAR *pchURLIn, OLECHAR **ppchURLOut) { ppchURLOut = NULL; return S_FALSE; }
	STDMETHODIMP FilterDataObject(IDataObject *pDO, IDataObject **ppDORet) { ppDORet = NULL; return S_FALSE; }

protected:
	FrameSite *m_fs;
};

//-----------------------------------------------------------------------------
// Purpose: page layout of the control: no 3D border and no scroll bars
//-----------------------------------------------------------------------------
HRESULT FS_IDocHostUIHandler::GetHostInfo(DOCHOSTUIINFO *pInfo)
{
	pInfo->cbSize = sizeof(DOCHOSTUIINFO);
	pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_SCROLL_NO;
	return S_OK;
}

static int s_KeyCodeToVirtual_guess[256];

//-----------------------------------------------------------------------------
// Purpose: vgui key code to windows virtual key table
//-----------------------------------------------------------------------------
void InitKeyTable_guess()
{
	s_KeyCodeToVirtual_guess[KEY_0] = '0';
	s_KeyCodeToVirtual_guess[KEY_1] = '1';
	s_KeyCodeToVirtual_guess[KEY_2] = '2';
	s_KeyCodeToVirtual_guess[KEY_3] = '3';
	s_KeyCodeToVirtual_guess[KEY_4] = '4';
	s_KeyCodeToVirtual_guess[KEY_5] = '5';
	s_KeyCodeToVirtual_guess[KEY_6] = '6';
	s_KeyCodeToVirtual_guess[KEY_7] = '7';
	s_KeyCodeToVirtual_guess[KEY_8] = '8';
	s_KeyCodeToVirtual_guess[KEY_9] = '9';
	s_KeyCodeToVirtual_guess[KEY_A] = 'A';
	s_KeyCodeToVirtual_guess[KEY_B] = 'B';
	s_KeyCodeToVirtual_guess[KEY_C] = 'C';
	s_KeyCodeToVirtual_guess[KEY_D] = 'D';
	s_KeyCodeToVirtual_guess[KEY_E] = 'E';
	s_KeyCodeToVirtual_guess[KEY_F] = 'F';
	s_KeyCodeToVirtual_guess[KEY_G] = 'G';
	s_KeyCodeToVirtual_guess[KEY_H] = 'H';
	s_KeyCodeToVirtual_guess[KEY_I] = 'I';
	s_KeyCodeToVirtual_guess[KEY_J] = 'J';
	s_KeyCodeToVirtual_guess[KEY_K] = 'K';
	s_KeyCodeToVirtual_guess[KEY_L] = 'L';
	s_KeyCodeToVirtual_guess[KEY_M] = 'M';
	s_KeyCodeToVirtual_guess[KEY_N] = 'N';
	s_KeyCodeToVirtual_guess[KEY_O] = 'O';
	s_KeyCodeToVirtual_guess[KEY_P] = 'P';
	s_KeyCodeToVirtual_guess[KEY_Q] = 'Q';
	s_KeyCodeToVirtual_guess[KEY_R] = 'R';
	s_KeyCodeToVirtual_guess[KEY_S] = 'S';
	s_KeyCodeToVirtual_guess[KEY_T] = 'T';
	s_KeyCodeToVirtual_guess[KEY_U] = 'U';
	s_KeyCodeToVirtual_guess[KEY_V] = 'V';
	s_KeyCodeToVirtual_guess[KEY_W] = 'W';
	s_KeyCodeToVirtual_guess[KEY_X] = 'X';
	s_KeyCodeToVirtual_guess[KEY_Y] = 'Y';
	s_KeyCodeToVirtual_guess[KEY_Z] = 'Z';
	s_KeyCodeToVirtual_guess[KEY_PAD_0] = VK_NUMPAD0;
	s_KeyCodeToVirtual_guess[KEY_PAD_1] = VK_NUMPAD1;
	s_KeyCodeToVirtual_guess[KEY_PAD_2] = VK_NUMPAD2;
	s_KeyCodeToVirtual_guess[KEY_PAD_3] = VK_NUMPAD3;
	s_KeyCodeToVirtual_guess[KEY_PAD_4] = VK_NUMPAD4;
	s_KeyCodeToVirtual_guess[KEY_PAD_5] = VK_NUMPAD5;
	s_KeyCodeToVirtual_guess[KEY_PAD_6] = VK_NUMPAD6;
	s_KeyCodeToVirtual_guess[KEY_PAD_7] = VK_NUMPAD7;
	s_KeyCodeToVirtual_guess[KEY_PAD_8] = VK_NUMPAD8;
	s_KeyCodeToVirtual_guess[KEY_PAD_9] = VK_NUMPAD9;
	s_KeyCodeToVirtual_guess[KEY_PAD_DIVIDE] = VK_DIVIDE;
	s_KeyCodeToVirtual_guess[KEY_PAD_MULTIPLY] = VK_MULTIPLY;
	s_KeyCodeToVirtual_guess[KEY_PAD_MINUS] = VK_SUBTRACT;
	s_KeyCodeToVirtual_guess[KEY_PAD_PLUS] = VK_ADD;
	s_KeyCodeToVirtual_guess[KEY_PAD_ENTER] = VK_RETURN;
	s_KeyCodeToVirtual_guess[KEY_PAD_DECIMAL] = VK_DECIMAL;
	s_KeyCodeToVirtual_guess[KEY_LBRACKET] = 0xdb;
	s_KeyCodeToVirtual_guess[KEY_RBRACKET] = 0xdd;
	s_KeyCodeToVirtual_guess[KEY_SEMICOLON] = 0xba;
	s_KeyCodeToVirtual_guess[KEY_APOSTROPHE] = 0xde;
	s_KeyCodeToVirtual_guess[KEY_BACKQUOTE] = 0xc0;
	s_KeyCodeToVirtual_guess[KEY_COMMA] = 0xbc;
	s_KeyCodeToVirtual_guess[KEY_PERIOD] = 0xbe;
	s_KeyCodeToVirtual_guess[KEY_SLASH] = 0xbf;
	s_KeyCodeToVirtual_guess[KEY_BACKSLASH] = 0xdc;
	s_KeyCodeToVirtual_guess[KEY_MINUS] = 0xbd;
	s_KeyCodeToVirtual_guess[KEY_EQUAL] = 0xbb;
	s_KeyCodeToVirtual_guess[KEY_ENTER] = VK_RETURN;
	s_KeyCodeToVirtual_guess[KEY_SPACE] = VK_SPACE;
	s_KeyCodeToVirtual_guess[KEY_BACKSPACE] = VK_BACK;
	s_KeyCodeToVirtual_guess[KEY_TAB] = VK_TAB;
	s_KeyCodeToVirtual_guess[KEY_CAPSLOCK] = VK_CAPITAL;
	s_KeyCodeToVirtual_guess[KEY_NUMLOCK] = VK_NUMLOCK;
	s_KeyCodeToVirtual_guess[KEY_ESCAPE] = VK_ESCAPE;
	s_KeyCodeToVirtual_guess[KEY_SCROLLLOCK] = VK_SCROLL;
	s_KeyCodeToVirtual_guess[KEY_INSERT] = VK_INSERT;
	s_KeyCodeToVirtual_guess[KEY_DELETE] = VK_DELETE;
	s_KeyCodeToVirtual_guess[KEY_HOME] = VK_HOME;
	s_KeyCodeToVirtual_guess[KEY_END] = VK_END;
	s_KeyCodeToVirtual_guess[KEY_PAGEUP] = VK_PRIOR;
	s_KeyCodeToVirtual_guess[KEY_PAGEDOWN] = VK_NEXT;
	s_KeyCodeToVirtual_guess[KEY_BREAK] = VK_PAUSE;
	s_KeyCodeToVirtual_guess[KEY_LSHIFT] = VK_SHIFT;
	s_KeyCodeToVirtual_guess[KEY_RSHIFT] = VK_SHIFT;
	s_KeyCodeToVirtual_guess[KEY_LALT] = VK_MENU;
	s_KeyCodeToVirtual_guess[KEY_RALT] = VK_MENU;
	s_KeyCodeToVirtual_guess[KEY_LCONTROL] = VK_CONTROL;
	s_KeyCodeToVirtual_guess[KEY_RCONTROL] = VK_CONTROL;
	s_KeyCodeToVirtual_guess[KEY_LWIN] = VK_LWIN;
	s_KeyCodeToVirtual_guess[KEY_RWIN] = VK_RWIN;
	s_KeyCodeToVirtual_guess[KEY_APP] = VK_APPS;
	s_KeyCodeToVirtual_guess[KEY_UP] = VK_UP;
	s_KeyCodeToVirtual_guess[KEY_LEFT] = VK_LEFT;
	s_KeyCodeToVirtual_guess[KEY_DOWN] = VK_DOWN;
	s_KeyCodeToVirtual_guess[KEY_RIGHT] = VK_RIGHT;
	s_KeyCodeToVirtual_guess[KEY_F1] = VK_F1;
	s_KeyCodeToVirtual_guess[KEY_F2] = VK_F2;
	s_KeyCodeToVirtual_guess[KEY_F3] = VK_F3;
	s_KeyCodeToVirtual_guess[KEY_F4] = VK_F4;
	s_KeyCodeToVirtual_guess[KEY_F5] = VK_F5;
	s_KeyCodeToVirtual_guess[KEY_F6] = VK_F6;
	s_KeyCodeToVirtual_guess[KEY_F7] = VK_F7;
	s_KeyCodeToVirtual_guess[KEY_F8] = VK_F8;
	s_KeyCodeToVirtual_guess[KEY_F9] = VK_F9;
	s_KeyCodeToVirtual_guess[KEY_F10] = VK_F10;
	s_KeyCodeToVirtual_guess[KEY_F11] = VK_F11;
	s_KeyCodeToVirtual_guess[KEY_F12] = VK_F12;
}

static CUtlVector<HtmlWindow *> html_windows_guess;
static bool s_bKeyTableInit_guess = false;
static bool s_bHooked_guess = false;

//-----------------------------------------------------------------------------
// Purpose: window procedure of the IE server window: keeps focus messages
//			away from the control
//-----------------------------------------------------------------------------
LRESULT CALLBACK HtmlWndProc_guess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HtmlWindow *win_guess = NULL;
	for (int i_guess = 0; i_guess < html_windows_guess.Count(); i_guess++)
	{
		if (html_windows_guess[i_guess] && html_windows_guess[i_guess]->GetIEHWND() == hwnd)
		{
			win_guess = html_windows_guess[i_guess];
			break;
		}
	}

	if (win_guess)
	{
		switch (msg)
		{
		case WM_KILLFOCUS:
			return 0;
		case WM_SETFOCUS:
			PostMessage(win_guess->GetHWND(), WM_APP, 0, 0);
			return 0;
		default:
			return CallWindowProc(win_guess->GetIEWndProc_guess(), hwnd, msg, wParam, lParam);
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: message hook of the browser thread: updates the window on timers
//-----------------------------------------------------------------------------
LRESULT CALLBACK GetMsgProc_guess(int code, WPARAM wParam, LPARAM lParam)
{
	MSG *pMsg_guess = (MSG *)lParam;
	HtmlWindow *win_guess = NULL;
	for (int i_guess = 0; i_guess < html_windows_guess.Count(); i_guess++)
	{
		if (html_windows_guess[i_guess] && html_windows_guess[i_guess]->GetIEHWND() == pMsg_guess->hwnd)
		{
			win_guess = html_windows_guess[i_guess];
			break;
		}
	}

	if (pMsg_guess && win_guess)
	{
		switch (pMsg_guess->message)
		{
		case WM_TIMER:
			win_guess->OnUpdate();
			break;
		}
		return CallNextHookEx(win_guess->GetHook_guess(), code, wParam, lParam);
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: forwards a mouse click to the child window under the cursor
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumChildProc_guess(HWND hwnd, LPARAM lParam)
{
	POINT point_guess;
	point_guess.y = lParam >> 16;
	point_guess.x = lParam & 0xffff;
	HtmlWindow *win_guess = NULL;
	HWND hParent_guess = GetParent(hwnd);
	for (int idx_guess = 0; idx_guess < html_windows_guess.Count(); idx_guess++)
	{
		if (html_windows_guess[idx_guess] && html_windows_guess[idx_guess]->GetIEHWND() == hParent_guess)
		{
			win_guess = html_windows_guess[idx_guess];
			break;
		}
	}

	if (win_guess)
	{
		RECT rcChild_guess, rcParent_guess;
		GetWindowRect(hwnd, &rcChild_guess);
		GetWindowRect(win_guess->GetIEHWND(), &rcParent_guess);

		point_guess.x -= rcChild_guess.left - rcParent_guess.left;
		point_guess.y -= rcChild_guess.top - rcParent_guess.top;
		lParam = point_guess.y << 16;
		lParam |= point_guess.x & 0xffff;

		rcChild_guess.bottom -= rcChild_guess.top;
		rcChild_guess.top = 0;
		rcChild_guess.right -= rcChild_guess.left;
		rcChild_guess.left = 0;

		if (PtInRect(&rcChild_guess, point_guess))
		{
			if (!PostMessage(hwnd, win_guess->GetMouseMessage_guess(), MK_LBUTTON, lParam))
			{
				DWORD err_guess = GetLastError();
				return TRUE;
			}
			if (!PostMessage(hwnd, WM_SETFOCUS, (WPARAM)win_guess->GetIEHWND(), lParam))
			{
				DWORD err_guess = GetLastError();
				return TRUE;
			}
		}
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
HtmlWindow::HtmlWindow(IHTMLEvents *events, VPANEL c, HWND parent, bool AllowJavaScript, bool DirectToHWND)
{
	m_oleObject_guess = NULL;
	m_oleInPlaceObject_guess = NULL;
	m_webBrowser_guess = NULL;
	m_events_guess = events;
	m_ieHWND_guess = 0;
	m_iWide_guess = 0;
	m_iTall_guess = 0;
	m_iX_guess = 0;
	m_iY_guess = 0;
	m_iUnused_guess = 0;
	m_HtmlEventsAdviseCookie_guess = 0;
	m_Bitmap_guess = NULL;
	m_bVisible_guess = false;
	m_hdcMem_guess = NULL;
	m_lastHDC_guess = NULL;
	m_hBitmap_guess = NULL;
	m_hHook_guess = NULL;
	m_bUnused_guess = false;
	m_bLoaded_guess = false;
	m_bUnused2_guess = false;
	m_bDirectToHWND_guess = DirectToHWND;
	m_hIEWndProc_guess = NULL;

	strcpy(m_currentUrl_guess, "");
	m_specificallyOpened_guess = false;

	m_parent_guess = parent;
	m_context_guess = c;

	if (!s_bKeyTableInit_guess)
	{
		s_bKeyTableInit_guess = true;
		InitKeyTable_guess();
	}

	if (!DirectToHWND)
	{
		char className_guess[50];
		if (!GetClassName(parent, className_guess, 50))
		{
			WNDCLASS wc_guess;
			memset(&wc_guess, 0, sizeof(wc_guess));
			wc_guess.style = CS_OWNDC;
			wc_guess.lpfnWndProc = HtmlWndProc_guess;
			wc_guess.hInstance = GetModuleHandle(NULL);
			wc_guess.lpszClassName = "VGUI_HTML";
			UnregisterClass("VGUI_HTML", GetModuleHandle(NULL));
			RegisterClass(&wc_guess);
			strcpy(className_guess, "VGUI_HTML");
		}

		m_parent_guess = CreateWindowEx(0, className_guess, "", WS_CHILD | WS_DISABLED, 0, 0, 1, 1, parent, NULL, GetModuleHandle(NULL), NULL);
	}

	CreateBrowser_guess(AllowJavaScript);
}

//-----------------------------------------------------------------------------
// Purpose: creates the web browser control and puts it in place
//-----------------------------------------------------------------------------
void HtmlWindow::CreateBrowser_guess(bool AllowJavaScript)
{
	bool bSetClientSiteFirst_guess;
	IUnknown *unk_guess;
	IConnectionPointContainer *cpContainer_guess;
	bool bVisible_guess;
	DWORD misc_guess;
	FrameSite *pSite_guess;
	MSG verbMsg_guess;
	HRESULT hret_guess;
	RECT posrect_guess;
	IPersistStreamInit *pInit_guess;

	// Get IUnknown Interface
	hret_guess = CoCreateInstance(CLSID_WebBrowser, NULL, CLSCTX_ALL, IID_IUnknown, (void **)&unk_guess);

	// Get IOleObject interface
	hret_guess = unk_guess->QueryInterface(IID_IViewObject, (void **)&m_viewObject_guess);
	hret_guess = unk_guess->QueryInterface(IID_IOleObject, (void **)&m_oleObject_guess);

	pSite_guess = new FrameSite(this, AllowJavaScript);
	pSite_guess->AddRef();
	m_frameSite_guess = pSite_guess;

	m_viewObject_guess->SetAdvise(DVASPECT_CONTENT, ADVF_PRIMEFIRST, pSite_guess->m_IAdviseSinkEx_guess);

	m_oleObject_guess->GetMiscStatus(DVASPECT_CONTENT, &misc_guess);
	bSetClientSiteFirst_guess = false;
	if (misc_guess & OLEMISC_SETCLIENTSITEFIRST)
	{
		bSetClientSiteFirst_guess = true;
	}
	bVisible_guess = true;
	if (misc_guess & OLEMISC_INVISIBLEATRUNTIME)
	{
		bVisible_guess = false;
	}

	if (bSetClientSiteFirst_guess)
		m_oleObject_guess->SetClientSite(pSite_guess->m_IOleClientSite_guess);

	pInit_guess = NULL;
	hret_guess = unk_guess->QueryInterface(IID_IPersistStreamInit, (void **)&pInit_guess);
	if (SUCCEEDED(hret_guess) && pInit_guess != NULL)
	{
		hret_guess = pInit_guess->InitNew();
	}
	pInit_guess->Release();

	// Get IOleInPlaceObject interface
	hret_guess = unk_guess->QueryInterface(IID_IOleInPlaceObject, (void **)&m_oleInPlaceObject_guess);

	posrect_guess.left = 0;
	posrect_guess.top = 0;
	posrect_guess.right = m_iWide_guess;
	posrect_guess.bottom = m_iTall_guess;

	m_oleInPlaceObject_guess->SetObjectRects(&posrect_guess, &posrect_guess);

	if (bVisible_guess)
	{
		hret_guess = m_oleObject_guess->DoVerb(OLEIVERB_INPLACEACTIVATE, &verbMsg_guess, pSite_guess->m_IOleClientSite_guess, 0, m_parent_guess, &posrect_guess);
	}

	hret_guess = m_oleInPlaceObject_guess->GetWindow(&m_oleObjectHWND_guess);

	if (!bSetClientSiteFirst_guess)
		m_oleObject_guess->SetClientSite(pSite_guess->m_IOleClientSite_guess);

	// Get IWebBrowser2 Interface
	hret_guess = unk_guess->QueryInterface(IID_IWebBrowser2, (void **)&m_webBrowser_guess);

	hret_guess = unk_guess->QueryInterface(IID_IConnectionPointContainer, (void **)&cpContainer_guess);
	hret_guess = cpContainer_guess->FindConnectionPoint(DIID_DWebBrowserEvents2, &m_connectionPoint_guess);
	m_connectionPoint_guess->Advise(pSite_guess->m_DWebBrowserEvents2_guess, &m_adviseCookie_guess);
	cpContainer_guess->Release();

	unk_guess->Release();

	m_webBrowser_guess->put_MenuBar(VARIANT_FALSE);
	m_webBrowser_guess->put_AddressBar(VARIANT_FALSE);
	m_webBrowser_guess->put_StatusBar(VARIANT_FALSE);
	m_webBrowser_guess->put_ToolBar(VARIANT_FALSE);

	m_webBrowser_guess->put_RegisterAsBrowser(VARIANT_TRUE);
	m_webBrowser_guess->put_RegisterAsDropTarget(VARIANT_TRUE);
	m_webBrowser_guess->put_Silent(VARIANT_TRUE);

	if (m_bDirectToHWND_guess)
	{
		ShowWindow(m_oleObjectHWND_guess, SW_HIDE);
	}

	html_windows_guess.AddToTail(this);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
HtmlWindow::~HtmlWindow()
{
	html_windows_guess.FindAndRemove(this);

	if (m_oleInPlaceObject_guess)
	{
		m_oleInPlaceObject_guess->InPlaceDeactivate();
		m_oleInPlaceObject_guess->UIDeactivate();
		m_oleInPlaceObject_guess->Release();
	}
	if (m_connectionPoint_guess)
	{
		m_connectionPoint_guess->Unadvise(m_adviseCookie_guess);
		m_connectionPoint_guess->Unadvise(m_HtmlEventsAdviseCookie_guess);
		m_connectionPoint_guess->Release();
	}
	if (m_oleObject_guess)
	{
		m_oleObject_guess->Close(OLECLOSE_NOSAVE);
		m_oleObject_guess->SetClientSite(NULL);
		m_oleObject_guess->Release();
	}
	if (m_viewObject_guess)
	{
		m_viewObject_guess->Release();
	}
	if (m_webBrowser_guess)
	{
		m_webBrowser_guess->Release();
	}
	if (m_frameSite_guess)
	{
		delete m_frameSite_guess;
	}
	if (m_lastHDC_guess)
	{
		DeleteObject(m_hBitmap_guess);
		DeleteDC(m_hdcMem_guess);
	}
	if (m_hHook_guess)
	{
		UnhookWindowsHookEx(m_hHook_guess);
		m_hHook_guess = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the browser needs to be resized
//-----------------------------------------------------------------------------
void HtmlWindow::OnSize(int x, int y, int w, int h)
{
	m_iWide_guess = w;
	m_iTall_guess = h;
	m_iScrollX_guess = x;
	m_iScrollY_guess = y;

	int panelx_guess, panely_guess;
	ipanel()->GetAbsPos(m_context_guess, panelx_guess, panely_guess);

	SetWindowPos(m_parent_guess, NULL, panelx_guess, panely_guess, m_iWide_guess, m_iTall_guess, SWP_HIDEWINDOW | SWP_NOACTIVATE);
	m_iX_guess = panelx_guess + 1;
	m_iY_guess = panely_guess + 1;

	if (m_lastHDC_guess)
	{
		DeleteObject(m_hBitmap_guess);
		DeleteDC(m_hdcMem_guess);
	}
	SetBounds_guess();
	m_lastHDC_guess = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: display the control
//-----------------------------------------------------------------------------
bool HtmlWindow::Show(bool shown)
{
	if (m_webBrowser_guess)
	{
		m_webBrowser_guess->put_Visible(shown);
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: shows or hides the browser window
//-----------------------------------------------------------------------------
void HtmlWindow::SetVisible(bool state)
{
	if (m_webBrowser_guess)
	{
		m_webBrowser_guess->put_Visible(state);
	}
	m_bVisible_guess = state;

	if (m_bLoaded_guess || !state)
	{
		if (m_bDirectToHWND_guess)
		{
			ShowWindow(m_oleObjectHWND_guess, state ? SW_SHOWNA : SW_HIDE);
			HWND focus_guess = ::GetFocus();
			if (focus_guess == m_oleObjectHWND_guess || focus_guess == m_ieHWND_guess)
			{
				::SetFocus(m_parent_guess);
			}
		}
		else
		{
			ShowWindow(m_oleObjectHWND_guess, SW_HIDE);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: stops the existing page from loading
//-----------------------------------------------------------------------------
bool HtmlWindow::StopLoading()
{
	HRESULT hret_guess = m_webBrowser_guess->Stop();
	if (SUCCEEDED(hret_guess))
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: refreshes the current page
//-----------------------------------------------------------------------------
bool HtmlWindow::Refresh()
{
	OpenURL(m_currentUrl_guess);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: places the control in the window, returns its rectangle
//-----------------------------------------------------------------------------
RECT HtmlWindow::SetBounds_guess()
{
	RECT rcPos_guess;
	if (m_bDirectToHWND_guess)
	{
		rcPos_guess.right = m_iWide_guess + m_iX_guess;
		rcPos_guess.bottom = m_iTall_guess + m_iY_guess;
		rcPos_guess.left = m_iX_guess;
		rcPos_guess.top = m_iY_guess;
	}
	else
	{
		rcPos_guess.right = m_iWide_guess;
		rcPos_guess.bottom = m_iTall_guess;
		rcPos_guess.left = 0;
		rcPos_guess.top = 0;
	}

	if (m_oleInPlaceObject_guess)
	{
		HRESULT hret_guess = m_oleInPlaceObject_guess->SetObjectRects(&rcPos_guess, &rcPos_guess);
	}

	if (m_webBrowser_guess)
	{
		m_webBrowser_guess->put_Height(m_iTall_guess);
		m_webBrowser_guess->put_Width(m_iWide_guess);
		ScrollHTML_guess(m_iScrollX_guess, m_iScrollY_guess);
	}

	if (!m_bDirectToHWND_guess)
	{
		RedrawWindow(m_ieHWND_guess, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
	}

	return rcPos_guess;
}

//-----------------------------------------------------------------------------
// Purpose: draws the page into an offscreen bitmap and uploads it as a texture
//-----------------------------------------------------------------------------
void HtmlWindow::OnPaint(HDC hDC)
{
	if (!m_iWide_guess || !m_iTall_guess)
		return;
	if (m_bDirectToHWND_guess)
		return;

	RECT rcClient_guess = SetBounds_guess();
	HGDIOBJ oldBitmap_guess = NULL;

	if (m_viewObject_guess)
	{
		if (!m_lastHDC_guess || m_lastHDC_guess != hDC)
		{
			if (m_lastHDC_guess)
			{
				DeleteObject(m_hBitmap_guess);
				DeleteDC(m_hdcMem_guess);
			}
			m_lastHDC_guess = hDC;
			m_hdcMem_guess = CreateCompatibleDC(hDC);
			m_hBitmap_guess = CreateCompatibleBitmap(hDC, m_iWide_guess, m_iTall_guess);
		}
		oldBitmap_guess = SelectObject(m_hdcMem_guess, m_hBitmap_guess);

		m_webBrowser_guess->put_Height(m_iTall_guess);
		m_webBrowser_guess->put_Width(m_iWide_guess);
		ScrollHTML_guess(m_iScrollX_guess, m_iScrollY_guess);

		RECTL *pRect_guess = (RECTL *)&rcClient_guess;
		m_viewObject_guess->Draw(DVASPECT_CONTENT, -1, NULL, NULL, hDC, m_hdcMem_guess, pRect_guess, NULL, NULL, 0);

		BITMAPINFO *pbmi_guess = (BITMAPINFO *)new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
		if (pbmi_guess)
		{
			memset(pbmi_guess, 0, sizeof(BITMAPINFOHEADER));
			pbmi_guess->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

			int lines_guess = GetDIBits(m_hdcMem_guess, m_hBitmap_guess, 0, m_iTall_guess, NULL, pbmi_guess, DIB_RGB_COLORS);
			unsigned char *bits_guess = NULL;
			if (lines_guess)
			{
				pbmi_guess->bmiHeader.biBitCount = 32;
				pbmi_guess->bmiHeader.biCompression = BI_BITFIELDS;
				pbmi_guess->bmiHeader.biSizeImage = pbmi_guess->bmiHeader.biWidth * pbmi_guess->bmiHeader.biHeight * (pbmi_guess->bmiHeader.biBitCount / 8);
				pbmi_guess->bmiHeader.biHeight *= -1;

				bits_guess = new unsigned char[pbmi_guess->bmiHeader.biSizeImage];
				if (bits_guess)
				{
					lines_guess = GetDIBits(m_hdcMem_guess, m_hBitmap_guess, 0, m_iTall_guess, bits_guess, pbmi_guess, DIB_RGB_COLORS);
					if (lines_guess)
					{
						unsigned char *data_guess = bits_guess;
						for (unsigned int i_guess = 0; i_guess < pbmi_guess->bmiHeader.biSizeImage; i_guess += 4)
						{
							unsigned char r_guess = data_guess[i_guess];
							data_guess[i_guess] = data_guess[i_guess + 2];
							data_guess[i_guess + 2] = r_guess;
							data_guess[i_guess + 3] = 255;
						}

						if (!m_Bitmap_guess)
						{
							m_Bitmap_guess = new MemoryBitmap(data_guess, m_iWide_guess, m_iTall_guess);
						}
						else
						{
							m_Bitmap_guess->ForceUpload_guess(data_guess, m_iWide_guess, m_iTall_guess);
						}
					}
					delete [] bits_guess;
				}
			}
			else
			{
				DWORD err_guess = GetLastError();
				err_guess++;
			}
			delete [] pbmi_guess;
		}

		if (oldBitmap_guess)
		{
			SelectObject(m_hdcMem_guess, oldBitmap_guess);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: passes a mouse event to the IE server window
//-----------------------------------------------------------------------------
void HtmlWindow::OnMouse(MouseCode code, MOUSE_STATE s, int x, int y)
{
	UINT msgId_guess = 0;
	WPARAM wparam_guess = MK_LBUTTON;
	LPARAM pos_guess = 0;
	LPARAM spare_guess = 0;

	pos_guess = y << 16;
	pos_guess |= x;
	m_lastMousePos_guess = pos_guess;

	if (code != MOUSE_LEFT)
		return;

	if (s == IHTML::UP)
	{
		if (code == MOUSE_LEFT)
			msgId_guess = WM_LBUTTONUP;
		else if (code == MOUSE_RIGHT)
			msgId_guess = WM_RBUTTONUP;
	}
	else if (s == IHTML::DOWN)
	{
		if (code == MOUSE_LEFT)
			msgId_guess = WM_LBUTTONDOWN;
		else if (code == MOUSE_RIGHT)
			msgId_guess = WM_RBUTTONDOWN;
	}
	else if (s == IHTML::MOVE)
	{
		msgId_guess = WM_MOUSEMOVE;
	}
	else
		return;

	if (!m_oleInPlaceObject_guess)
		return;

	if (m_ieHWND_guess && !PostMessage(m_ieHWND_guess, msgId_guess, wparam_guess, pos_guess))
	{
		DWORD err_guess = GetLastError();
		return;
	}
	if (m_ieHWND_guess && !PostMessage(m_ieHWND_guess, WM_SETFOCUS, (WPARAM)m_oleObjectHWND_guess, pos_guess))
	{
		DWORD err_guess = GetLastError();
		return;
	}
	EnumChildWindows(m_ieHWND_guess, EnumChildProc_guess, pos_guess);
}

//-----------------------------------------------------------------------------
// Purpose: clear the text in the control
//-----------------------------------------------------------------------------
void HtmlWindow::Clear()
{
	IDispatch *pDisp_guess;
	m_webBrowser_guess->get_Document(&pDisp_guess);
	if (pDisp_guess)
	{
		IHTMLDocument2 *pHTMLDocument2_guess;
		HRESULT hret_guess = pDisp_guess->QueryInterface(IID_IHTMLDocument2, (void **)&pHTMLDocument2_guess);
		if (hret_guess == S_OK)
		{
			pHTMLDocument2_guess->close();
			pHTMLDocument2_guess->Release();
		}
		pDisp_guess->Release();
	}
}

//-----------------------------------------------------------------------------
// Purpose: add text to the control (as a HTML formated string)
//-----------------------------------------------------------------------------
void HtmlWindow::AddText(const char *text)
{
	IDispatch *pDisp_guess;
	m_webBrowser_guess->get_Document(&pDisp_guess);

	IHTMLDocument2 *doc_guess;
	HRESULT result_guess = pDisp_guess->QueryInterface(IID_IHTMLDocument2, (void **)&doc_guess);
	if (result_guess == S_OK)
	{
		wchar_t *wtext_guess = new wchar_t[strlen(text) + 1];
		if (wtext_guess)
		{
			MultiByteToWideChar(CP_ACP, 0, text, strlen(text) + 1, wtext_guess, strlen(text) + 1);
			BSTR bstrText_guess = SysAllocString(wtext_guess);

			IHTMLElement *body_guess;
			HRESULT hret2_guess = doc_guess->get_body(&body_guess);
			if (hret2_guess == S_OK && body_guess)
			{
				BSTR where_guess = L"beforeEnd";
				body_guess->insertAdjacentHTML(where_guess, wtext_guess);
				body_guess->Release();
			}
		}
		doc_guess->Release();
	}
	pDisp_guess->Release();
}

//-----------------------------------------------------------------------------
// Purpose: passes a character to the IE server window
//-----------------------------------------------------------------------------
void HtmlWindow::OnChar(wchar_t unichar)
{
	UINT msgId_guess = 0;
	WPARAM wparam_guess = MK_LBUTTON;
	LPARAM pos_guess = 0;
	LPARAM spare_guess = 0;

	wparam_guess = unichar;
	msgId_guess = WM_CHAR;

	if (!m_oleInPlaceObject_guess)
		return;

	if (m_ieHWND_guess)
	{
		PostMessage(m_ieHWND_guess, msgId_guess, wparam_guess, pos_guess);
	}
}

//-----------------------------------------------------------------------------
// Purpose: passes a key press to the IE server window
//-----------------------------------------------------------------------------
void HtmlWindow::OnKeyDown(KeyCode code)
{
	UINT msgId_guess = 0;
	WPARAM wparam_guess = MK_LBUTTON;
	LPARAM pos_guess = 0;
	LPARAM spare_guess = 0;

	wparam_guess = s_KeyCodeToVirtual_guess[code];
	msgId_guess = WM_KEYDOWN;

	if (!m_oleInPlaceObject_guess)
		return;

	if (m_ieHWND_guess)
	{
		PostMessage(m_ieHWND_guess, msgId_guess, wparam_guess, pos_guess);
	}
}

//-----------------------------------------------------------------------------
// Purpose: a new page starts loading
//-----------------------------------------------------------------------------
bool HtmlWindow::OnStartURL(const char *url, const char *target, bool first)
{
	if (m_HtmlEventsAdviseCookie_guess != 0)
	{
		m_connectionPoint_guess->Unadvise(m_HtmlEventsAdviseCookie_guess);
	}

	strncpy(m_currentUrl_guess, url, 512);
	if (m_specificallyOpened_guess)
	{
		m_specificallyOpened_guess = false;
	}
	m_bLoaded_guess = false;

	if (m_events_guess)
	{
		if (m_events_guess->OnStartURL(url, target, first))
			return true;
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void HtmlWindow::OnProgressURL(long current, long maximum)
{
	if (m_events_guess)
	{
		m_events_guess->OnProgressURL(current, maximum);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void HtmlWindow::OnSetStatusText(const char *text)
{
	if (m_events_guess)
	{
		m_events_guess->OnSetStatusText(text);
	}
}

//-----------------------------------------------------------------------------
// Purpose: the page changed: listens to its document events and measures it
//-----------------------------------------------------------------------------
void HtmlWindow::OnUpdate()
{
	IDispatch *pDisp_guess;
	m_webBrowser_guess->get_Document(&pDisp_guess);
	if (pDisp_guess)
	{
		IHTMLDocument2 *pHTMLDocument2_guess;
		HRESULT hret_guess = pDisp_guess->QueryInterface(IID_IHTMLDocument2, (void **)&pHTMLDocument2_guess);
		if (hret_guess == S_OK)
		{
			IHTMLElement *body_guess;
			hret_guess = pHTMLDocument2_guess->get_body(&body_guess);

			FrameSite *c_guess = m_frameSite_guess;
			IConnectionPointContainer *cpContainer_guess;
			hret_guess = pDisp_guess->QueryInterface(IID_IConnectionPointContainer, (void **)&cpContainer_guess);
			hret_guess = cpContainer_guess->FindConnectionPoint(DIID_HTMLDocumentEvents, &m_connectionPoint_guess);
			m_connectionPoint_guess->Advise(c_guess->m_DHTMLDocumentEvents2_guess, &m_HtmlEventsAdviseCookie_guess);
			cpContainer_guess->Release();

			if (body_guess)
			{
				CalculateHTMLSize_guess(body_guess);
				body_guess->Release();
			}
			pHTMLDocument2_guess->Release();
		}
		pDisp_guess->Release();
	}

	if (m_events_guess)
	{
		m_events_guess->OnUpdate();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void HtmlWindow::OnLink()
{
	if (m_events_guess)
	{
		m_events_guess->OnLink();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void HtmlWindow::OffLink()
{
	if (m_events_guess)
	{
		m_events_guess->OffLink();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the element has the given tag name
//-----------------------------------------------------------------------------
bool HtmlWindow::CheckIsLink_guess(IHTMLElement *el, char *type)
{
	bool found_guess = false;
	BSTR wTag_guess;
	HRESULT hret_guess = el->get_tagName(&wTag_guess);
	_bstr_t tag_guess(wTag_guess);
	if (wTag_guess)
	{
		const char *text_guess = tag_guess;
		if (!_stricmp(text_guess, type))
		{
			found_guess = true;
		}
		SysFreeString(wTag_guess);
	}
	return found_guess;
}

//-----------------------------------------------------------------------------
// Purpose: turns the scroll bars off and reads the size of the page
//-----------------------------------------------------------------------------
void HtmlWindow::CalculateHTMLSize_guess(IHTMLElement *pElement)
{
	IHTMLBodyElement *body_guess;
	IHTMLTextContainer *pText_guess;
	pElement->QueryInterface(IID_IHTMLBodyElement, (void **)&body_guess);
	if (!body_guess)
		return;

	body_guess->put_scroll(_bstr_t("no"));

	body_guess->QueryInterface(IID_IHTMLTextContainer, (void **)&pText_guess);
	if (pText_guess)
	{
		pText_guess->get_scrollWidth(&m_iHtmlWide_guess);
		pText_guess->get_scrollHeight(&m_iHtmlTall_guess);
		pText_guess->Release();
	}
	body_guess->Release();
}

//-----------------------------------------------------------------------------
// Purpose: scrolls the page
//-----------------------------------------------------------------------------
void HtmlWindow::ScrollHTML_guess(int x, int y)
{
	IDispatch *pDisp_guess;
	m_webBrowser_guess->get_Document(&pDisp_guess);
	if (pDisp_guess)
	{
		IHTMLDocument2 *pHTMLDocument2_guess;
		HRESULT hret_guess = pDisp_guess->QueryInterface(IID_IHTMLDocument2, (void **)&pHTMLDocument2_guess);
		if (hret_guess == S_OK && pHTMLDocument2_guess)
		{
			IHTMLElement *pElement_guess;
			hret_guess = pHTMLDocument2_guess->get_body(&pElement_guess);
			if (hret_guess == S_OK && pElement_guess)
			{
				IHTMLTextContainer *pText_guess;
				pElement_guess->QueryInterface(IID_IHTMLTextContainer, (void **)&pText_guess);
				if (pText_guess)
				{
					pText_guess->put_scrollLeft(x);
					pText_guess->put_scrollTop(y);
					pText_guess->Release();
				}
				pElement_guess->Release();
			}
			pHTMLDocument2_guess->Release();
		}
		pDisp_guess->Release();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the mouse is over a link
//-----------------------------------------------------------------------------
bool HtmlWindow::OnMouseOver_guess()
{
	bool isLink_guess = false;
	IDispatch *pDispatch_guess;
	m_webBrowser_guess->get_Document(&pDispatch_guess);

	IHTMLDocument2 *doc_guess;
	HRESULT result_guess = pDispatch_guess->QueryInterface(IID_IHTMLDocument2, (void **)&doc_guess);
	if (result_guess == S_OK)
	{
		IHTMLWindow2 *parentWindow_guess;
		HRESULT hret2_guess = doc_guess->get_parentWindow(&parentWindow_guess);
		if (SUCCEEDED(hret2_guess))
		{
			IHTMLEventObj *pEvtObj_guess;
			hret2_guess = parentWindow_guess->get_event(&pEvtObj_guess);
			parentWindow_guess->Release();
			if (SUCCEEDED(hret2_guess) && pEvtObj_guess)
			{
				IHTMLElement *pElement_guess;
				pEvtObj_guess->get_srcElement(&pElement_guess);
				if (CheckIsLink_guess(pElement_guess, "A"))
				{
					isLink_guess = true;
				}
				else
				{
					IHTMLElement *pParent_guess = pElement_guess;
					IHTMLElement *last_guess = NULL;
					while (!isLink_guess && SUCCEEDED(pParent_guess->get_parentElement(&pParent_guess)))
					{
						if (last_guess)
						{
							last_guess->Release();
						}
						if (!pParent_guess || pParent_guess == last_guess)
							break;

						if (CheckIsLink_guess(pParent_guess, "A"))
						{
							isLink_guess = true;
						}
						last_guess = pParent_guess;
					}
					if (pParent_guess)
					{
						pParent_guess->Release();
					}
				}
				pElement_guess->Release();
			}
		}
		doc_guess->Release();
	}
	pDispatch_guess->Release();
	return isLink_guess;
}

//-----------------------------------------------------------------------------
// Purpose: the browser's application object, for a new window
//-----------------------------------------------------------------------------
void HtmlWindow::GetApplication_guess(IDispatch **ppDisp)
{
	m_webBrowser_guess->get_Application(ppDisp);
}

//-----------------------------------------------------------------------------
// Purpose: a page finished loading
//-----------------------------------------------------------------------------
void HtmlWindow::OnFinishURL(const char *url)
{
	if (!url)
		return;

	strncpy(m_currentUrl_guess, url, 512);
	m_bLoaded_guess = true;
	m_bUnused_guess = false;

	IDispatch *pDispatch_guess;
	m_webBrowser_guess->get_Document(&pDispatch_guess);
	if (pDispatch_guess)
	{
		IHTMLDocument2 *pHTMLDocument2_guess;
		HRESULT hret_guess = pDispatch_guess->QueryInterface(IID_IHTMLDocument2, (void **)&pHTMLDocument2_guess);
		if (hret_guess == S_OK)
		{
			IHTMLElement *body_guess;
			hret_guess = pHTMLDocument2_guess->get_body(&body_guess);

			FrameSite *c_guess = m_frameSite_guess;
			IConnectionPointContainer *cpContainer_guess;
			hret_guess = pDispatch_guess->QueryInterface(IID_IConnectionPointContainer, (void **)&cpContainer_guess);
			hret_guess = cpContainer_guess->FindConnectionPoint(DIID_HTMLDocumentEvents, &m_connectionPoint_guess);
			m_connectionPoint_guess->Advise(c_guess->m_DHTMLDocumentEvents2_guess, &m_HtmlEventsAdviseCookie_guess);
			cpContainer_guess->Release();

			if (body_guess)
			{
				CalculateHTMLSize_guess(body_guess);
				body_guess->Release();
			}
			pHTMLDocument2_guess->Release();
		}
		pDispatch_guess->Release();
	}

	if (m_events_guess)
	{
		m_events_guess->OnFinishURL(url);
	}

	// find the IE server window
	HWND child_guess = m_oleObjectHWND_guess;
	char name_guess[100];
	memset(name_guess, 0, 100);
	while (strcmp(name_guess, "Internet Explorer_Server") && child_guess)
	{
		child_guess = GetWindow(child_guess, GW_CHILD);
		if (child_guess)
		{
			GetClassName(child_guess, name_guess, 100);
		}
	}
	if (child_guess)
	{
		m_ieHWND_guess = child_guess;
	}

	if (!s_bHooked_guess)
	{
		DWORD threadId_guess = GetWindowThreadProcessId(m_oleObjectHWND_guess, NULL);
		m_hHook_guess = SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc_guess, NULL, threadId_guess);
		if (!m_hHook_guess)
		{
			DWORD err_guess = GetLastError();
			err_guess++;
		}
		s_bHooked_guess = true;
	}

	if (m_bVisible_guess)
	{
		if (m_bDirectToHWND_guess)
		{
			ShowWindow(m_oleObjectHWND_guess, m_bVisible_guess ? SW_SHOWNA : SW_HIDE);
		}
		else
		{
			ShowWindow(m_oleObjectHWND_guess, SW_HIDE);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: open a new page
//-----------------------------------------------------------------------------
void HtmlWindow::OpenURL(const char *url)
{
	if (!url || !*url)
		return;

	VARIANT flags_guess, target_guess, vURL_guess, headers_guess, post_guess;
	flags_guess.vt = VT_EMPTY;
	target_guess.vt = VT_EMPTY;
	post_guess.vt = VT_EMPTY;
	headers_guess.vt = VT_EMPTY;
	vURL_guess.vt = VT_BSTR;
	_bstr_t bstrURL_guess(url);
	vURL_guess.bstrVal = bstrURL_guess;

	m_specificallyOpened_guess = true;
	HRESULT hret_guess = m_webBrowser_guess->Navigate2(&vURL_guess, &flags_guess, &target_guess, &post_guess, &headers_guess);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
FrameSite::FrameSite(HtmlWindow *win, bool AllowJavaScript)
{
	m_cRef_guess = 0;

	m_window_guess = win;
	m_bSupportsWindowlessActivation_guess = true;
	m_bInPlaceLocked_guess = false;
	m_bUIActive_guess = false;
	m_bInPlaceActive_guess = false;
	m_bWindowless_guess = false;

	m_nAmbientLocale_guess = 0;
	m_clrAmbientForeColor_guess = ::GetSysColor(COLOR_WINDOWTEXT);
	m_clrAmbientBackColor_guess = ::GetSysColor(COLOR_WINDOW);
	m_bAmbientUserMode_guess = true;
	m_bAmbientShowHatching_guess = true;
	m_bAmbientShowGrabHandles_guess = true;
	m_bAmbientAppearance_guess = true;

	m_hDCBuffer_guess = NULL;
	m_hWndParent_guess = m_window_guess->GetHWND();

	m_IOleInPlaceFrame_guess = new FS_IOleInPlaceFrame(this);
	m_IOleInPlaceSiteWindowless_guess = new FS_IOleInPlaceSiteWindowless(this);
	m_IOleClientSite_guess = new FS_IOleClientSite(this);
	m_IOleControlSite_guess = new FS_IOleControlSite(this);
	m_IOleCommandTarget_guess = new FS_IOleCommandTarget(this);
	m_IOleItemContainer_guess = new FS_IOleItemContainer(this);
	m_IDispatch_guess = new FS_IDispatch(this);
	m_IDispatch_guess->SetAllowJavaScript_guess(AllowJavaScript);
	m_DWebBrowserEvents2_guess = new FS_DWebBrowserEvents2(this);
	m_DHTMLDocumentEvents2_guess = new FS_DHTMLDocumentEvents2(this);
	m_IAdviseSink2_guess = new FS_IAdviseSink2(this);
	m_IAdviseSinkEx_guess = new FS_IAdviseSinkEx(this);
	m_IDocHostUIHandler_guess = new FS_IDocHostUIHandler(this);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
FrameSite::~FrameSite()
{
	delete m_IAdviseSinkEx_guess;
	delete m_IAdviseSink2_guess;
	delete m_DWebBrowserEvents2_guess;
	delete m_DHTMLDocumentEvents2_guess;
	delete m_IDispatch_guess;
	delete m_IOleItemContainer_guess;
	delete m_IOleCommandTarget_guess;
	delete m_IOleControlSite_guess;
	delete m_IOleClientSite_guess;
	delete m_IOleInPlaceSiteWindowless_guess;
	delete m_IOleInPlaceFrame_guess;
	delete m_IDocHostUIHandler_guess;
}

//IUnknown

STDMETHODIMP FrameSite::QueryInterface(REFIID riid, void **ppv)
{
	if (ppv == NULL)
		return E_INVALIDARG;

	*ppv = NULL;

	if (IsEqualIID(riid, IID_IUnknown))
		*ppv = this;
	else if (IsEqualIID(riid, IID_IOleWindow) ||
		IsEqualIID(riid, IID_IOleInPlaceUIWindow) ||
		IsEqualIID(riid, IID_IOleInPlaceFrame))
		*ppv = m_IOleInPlaceFrame_guess;
	else if (IsEqualIID(riid, IID_IOleInPlaceSite) ||
		IsEqualIID(riid, IID_IOleInPlaceSiteEx) ||
		IsEqualIID(riid, IID_IOleInPlaceSiteWindowless))
		*ppv = m_IOleInPlaceSiteWindowless_guess;
	else if (IsEqualIID(riid, IID_IOleClientSite))
		*ppv = m_IOleClientSite_guess;
	else if (IsEqualIID(riid, IID_IOleControlSite))
		*ppv = m_IOleControlSite_guess;
	else if (IsEqualIID(riid, IID_IOleCommandTarget))
		*ppv = m_IOleCommandTarget_guess;
	else if (IsEqualIID(riid, IID_IOleItemContainer) ||
		IsEqualIID(riid, IID_IOleContainer) ||
		IsEqualIID(riid, IID_IParseDisplayName))
		*ppv = m_IOleItemContainer_guess;
	else if (IsEqualIID(riid, IID_IDispatch))
		*ppv = m_IDispatch_guess;
	else if (IsEqualIID(riid, DIID_DWebBrowserEvents2))
		*ppv = m_DWebBrowserEvents2_guess;
	else if (IsEqualIID(riid, DIID_HTMLDocumentEvents))
		*ppv = m_DHTMLDocumentEvents2_guess;
	else if (IsEqualIID(riid, IID_IAdviseSink) ||
		IsEqualIID(riid, IID_IAdviseSink2))
		*ppv = m_IAdviseSink2_guess;
	else if (IsEqualIID(riid, IID_IAdviseSinkEx))
		*ppv = m_IAdviseSinkEx_guess;
	else if (IsEqualIID(riid, IID_IDocHostUIHandler))
		*ppv = m_IDocHostUIHandler_guess;

	if (*ppv == NULL)
		return (HRESULT)E_NOINTERFACE;
	AddRef();
	return S_OK;
}

ULONG FrameSite::AddRef()
{
	return ++m_cRef_guess;
}

ULONG FrameSite::Release()
{
	if (--m_cRef_guess == 0)
	{
		delete this;
		return 0;
	}
	else return m_cRef_guess;
}

//IDispatch

HRESULT FS_IDispatch::GetIDsOfNames(REFIID riid, OLECHAR **rgszNames, unsigned int cNames, LCID lcid, DISPID *rgDispId)
{
	return E_NOTIMPL;
}

HRESULT FS_IDispatch::GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
	return E_NOTIMPL;
}

HRESULT FS_IDispatch::GetTypeInfoCount(unsigned int *pcTInfo)
{
	return E_NOTIMPL;
}

HRESULT FS_IDispatch::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, unsigned int *puArgErr)
{
	if (wFlags & DISPATCH_PROPERTYGET)
	{
		if (pVarResult == NULL)
			return E_INVALIDARG;
		switch (dispIdMember)
		{
		case DISPID_AMBIENT_APPEARANCE:
			pVarResult->vt = VT_BOOL;
			pVarResult->boolVal = m_fs->m_bAmbientAppearance_guess;
			break;

		case DISPID_AMBIENT_FORECOLOR:
			pVarResult->vt = VT_I4;
			pVarResult->lVal = (long)m_fs->m_clrAmbientForeColor_guess;
			break;

		case DISPID_AMBIENT_BACKCOLOR:
			pVarResult->vt = VT_I4;
			pVarResult->lVal = (long)m_fs->m_clrAmbientBackColor_guess;
			break;

		case DISPID_AMBIENT_LOCALEID:
			pVarResult->vt = VT_I4;
			pVarResult->lVal = (long)m_fs->m_nAmbientLocale_guess;
			break;

		case DISPID_AMBIENT_USERMODE:
			pVarResult->vt = VT_BOOL;
			pVarResult->boolVal = m_fs->m_bAmbientUserMode_guess;
			break;

		case DISPID_AMBIENT_SHOWGRABHANDLES:
			pVarResult->vt = VT_BOOL;
			pVarResult->boolVal = m_fs->m_bAmbientShowGrabHandles_guess;
			break;

		case DISPID_AMBIENT_SHOWHATCHING:
			pVarResult->vt = VT_BOOL;
			pVarResult->boolVal = m_fs->m_bAmbientShowHatching_guess;
			break;

		case DISPID_AMBIENT_DLCONTROL:
			pVarResult->vt = VT_I4;
			pVarResult->lVal = DLCTL_DLIMAGES | DLCTL_VIDEOS | DLCTL_BGSOUNDS;
			if (!m_bAllowJavaScript_guess)
			{
				pVarResult->lVal |= DLCTL_NO_SCRIPTS;
			}
			break;

		default:
			return DISP_E_MEMBERNOTFOUND;
		}
		return S_OK;
	}

	switch (dispIdMember)
	{
	case DISPID_BEFORENAVIGATE2:
		{
			VARIANT *vurl_guess = pDispParams->rgvarg[5].pvarVal;
			VARIANT *target_guess = pDispParams->rgvarg[3].pvarVal;
			if (m_fs->m_window_guess->OnStartURL(_bstr_t(vurl_guess->bstrVal), _bstr_t(target_guess->bstrVal), m_iDownloadCount_guess == 0))
			{
				*pDispParams->rgvarg[0].pboolVal = VARIANT_FALSE;
			}
			else
			{
				*pDispParams->rgvarg[0].pboolVal = VARIANT_TRUE;
			}
		}
		break;

	case DISPID_PROGRESSCHANGE:
		{
			long current_guess = pDispParams->rgvarg[1].lVal;
			long maximum_guess = pDispParams->rgvarg[0].lVal;
			m_fs->m_window_guess->OnProgressURL(current_guess, maximum_guess);
		}
		break;

	case DISPID_DOWNLOADCOMPLETE:
		if (m_iDownloadCount_guess > 0)
		{
			m_iDownloadCount_guess--;
		}
		break;

	case DISPID_DOCUMENTCOMPLETE:
		if (m_iDownloadCount_guess == 0)
		{
			VARIANT *vurl_guess = pDispParams->rgvarg[0].pvarVal;
			m_fs->m_window_guess->OnFinishURL(_bstr_t(vurl_guess->bstrVal));
		}
		else
		{
			m_fs->m_window_guess->OnUpdate();
		}

	case DISPID_NAVIGATECOMPLETE2:
		m_fs->m_window_guess->OnUpdate();
		break;

	case DISPID_DOWNLOADBEGIN:
		m_iDownloadCount_guess++;
		break;

	case DISPID_NEWWINDOW2:
		if (!g_pSurface->SupportsFeature(ISurface::OPENING_NEW_HTML_WINDOWS))
		{
			m_iDownloadCount_guess++;
			IDispatch *pDisp_guess;
			m_fs->m_window_guess->GetApplication_guess(&pDisp_guess);
			*pDispParams->rgvarg[1].ppdispVal = pDisp_guess;
			break;
		}

	case DISPID_HTMLDOCUMENTEVENTS_ONMOUSEOVER:
		if (m_fs->m_window_guess->OnMouseOver_guess())
		{
			m_bOnLink_guess = true;
			m_fs->m_window_guess->OnLink();
		}
		else if (m_bOnLink_guess = true)
		{
			m_bOnLink_guess = false;
			m_fs->m_window_guess->OffLink();
		}
		break;
	}

	return S_OK;
}

//IOleWindow

HRESULT FS_IOleInPlaceFrame::GetWindow(HWND *phwnd)
{
	if (phwnd == NULL)
		return E_INVALIDARG;
	(*phwnd) = m_fs->m_hWndParent_guess;
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::ContextSensitiveHelp(BOOL fEnterMode)
{
	return S_OK;
}

//IOleInPlaceUIWindow

HRESULT FS_IOleInPlaceFrame::GetBorder(LPRECT lprectBorder)
{
	if (lprectBorder == NULL)
		return E_INVALIDARG;
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT FS_IOleInPlaceFrame::RequestBorderSpace(LPCBORDERWIDTHS pborderwidths)
{
	if (pborderwidths == NULL)
		return E_INVALIDARG;
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT FS_IOleInPlaceFrame::SetBorderSpace(LPCBORDERWIDTHS pborderwidths)
{
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::SetActiveObject(IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName)
{
	return S_OK;
}

//IOleInPlaceFrame

HRESULT FS_IOleInPlaceFrame::InsertMenus(HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths)
{
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::SetMenu(HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject)
{
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::RemoveMenus(HMENU hmenuShared)
{
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::SetStatusText(LPCOLESTR pszStatusText)
{
	if (m_fs->m_window_guess->GetEvents_guess())
	{
		char status_guess[512];
		WideCharToMultiByte(CP_ACP, 0, pszStatusText, -1, status_guess, 512, NULL, NULL);
		m_fs->m_window_guess->GetEvents_guess()->OnSetStatusText(status_guess);
	}
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::EnableModeless(BOOL fEnable)
{
	return S_OK;
}

HRESULT FS_IOleInPlaceFrame::TranslateAccelerator(LPMSG lpmsg, WORD wID)
{
	return E_NOTIMPL;
}

//IOleInPlaceSite

HRESULT FS_IOleInPlaceSiteWindowless::CanInPlaceActivate()
{
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnInPlaceActivate()
{
	m_fs->m_bInPlaceActive_guess = true;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnUIActivate()
{
	m_fs->m_bUIActive_guess = true;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::GetWindowContext(IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc, LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
	if (ppFrame == NULL || ppDoc == NULL || lprcPosRect == NULL ||
		lprcClipRect == NULL || lpFrameInfo == NULL)
	{
		if (ppFrame != NULL)
			(*ppFrame) = NULL;
		if (ppDoc != NULL)
			(*ppDoc) = NULL;
		return E_INVALIDARG;
	}

	(*ppDoc) = (*ppFrame) = m_fs->m_IOleInPlaceFrame_guess;
	(*ppDoc)->AddRef();
	(*ppFrame)->AddRef();

	int w_guess, h_guess;
	m_fs->m_window_guess->GetSize_guess(w_guess, h_guess);
	lprcPosRect->left = 0;
	lprcPosRect->top = 0;
	lprcPosRect->right = w_guess;
	lprcPosRect->bottom = h_guess;
	lprcClipRect->left = 0;
	lprcClipRect->top = 0;
	lprcClipRect->right = w_guess;
	lprcClipRect->bottom = h_guess;

	lpFrameInfo->fMDIApp = FALSE;
	lpFrameInfo->hwndFrame = m_fs->m_hWndParent_guess;
	lpFrameInfo->haccel = NULL;
	lpFrameInfo->cAccelEntries = 0;

	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::Scroll(SIZE scrollExtent)
{
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnUIDeactivate(BOOL fUndoable)
{
	m_fs->m_bUIActive_guess = false;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnInPlaceDeactivate()
{
	m_fs->m_bInPlaceActive_guess = false;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::DiscardUndoState()
{
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::DeactivateAndUndo()
{
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnPosRectChange(LPCRECT lprcPosRect)
{
	return S_OK;
}

//IOleInPlaceSiteEx

HRESULT FS_IOleInPlaceSiteWindowless::OnInPlaceActivateEx(BOOL *pfNoRedraw, DWORD dwFlags)
{
	if (pfNoRedraw)
		(*pfNoRedraw) = FALSE;
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnInPlaceDeactivateEx(BOOL fNoRedraw)
{
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::RequestUIActivate()
{
	return S_OK;
}

//IOleInPlaceSiteWindowless

HRESULT FS_IOleInPlaceSiteWindowless::CanWindowlessActivate()
{
	return (m_fs->m_bSupportsWindowlessActivation_guess) ? S_OK : S_FALSE;
}

HRESULT FS_IOleInPlaceSiteWindowless::GetCapture()
{
	return S_FALSE;
}

HRESULT FS_IOleInPlaceSiteWindowless::SetCapture(BOOL fCapture)
{
	return S_FALSE;
}

HRESULT FS_IOleInPlaceSiteWindowless::GetFocus()
{
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::SetFocus(BOOL fFocus)
{
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::GetDC(LPCRECT pRect, DWORD grfFlags, HDC *phDC)
{
	if (phDC == NULL)
		return E_INVALIDARG;
	if (grfFlags & OLEDC_NODRAW)
	{
		(*phDC) = m_fs->m_hDCBuffer_guess;
		return S_OK;
	}
	if (m_fs->m_hDCBuffer_guess != NULL)
		return E_UNEXPECTED;
	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::ReleaseDC(HDC hDC)
{
	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::InvalidateRect(LPCRECT pRect, BOOL fErase)
{
	// Clip the rectangle against the object's size and invalidate it
	RECT rcI_guess = { 0, 0, 0, 0 };
	RECT posRect_guess = m_fs->m_window_guess->SetBounds_guess();
	if (pRect == NULL)
	{
		rcI_guess = posRect_guess;
	}
	else
	{
		IntersectRect(&rcI_guess, &posRect_guess, pRect);
	}
	::InvalidateRect(m_fs->m_hWndParent_guess, &rcI_guess, fErase);
	return S_OK;
}

HRESULT FS_IOleInPlaceSiteWindowless::InvalidateRgn(HRGN, BOOL)
{
	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::ScrollRect(INT, INT, LPCRECT, LPCRECT)
{
	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::AdjustRect(LPRECT)
{
	return E_NOTIMPL;
}

HRESULT FS_IOleInPlaceSiteWindowless::OnDefWindowMessage(UINT, WPARAM, LPARAM, LRESULT *)
{
	return E_NOTIMPL;
}

//IOleClientSite

HRESULT FS_IOleClientSite::SaveObject()
{
	return S_OK;
}

HRESULT FS_IOleClientSite::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker **ppmk)
{
	return E_NOTIMPL;
}

HRESULT FS_IOleClientSite::GetContainer(LPOLECONTAINER FAR *ppContainer)
{
	if (ppContainer == NULL)
		return E_INVALIDARG;
	this->QueryInterface(IID_IOleContainer, (void **)(ppContainer));
	return S_OK;
}

HRESULT FS_IOleClientSite::ShowObject()
{
	return S_OK;
}

HRESULT FS_IOleClientSite::OnShowWindow(BOOL fShow)
{
	return S_OK;
}

HRESULT FS_IOleClientSite::RequestNewObjectLayout()
{
	return E_NOTIMPL;
}

//IParseDisplayName

HRESULT FS_IOleItemContainer::ParseDisplayName(IBindCtx *pbc, LPOLESTR pszDisplayName, ULONG *pchEaten, IMoniker **ppmkOut)
{
	return E_NOTIMPL;
}

//IOleContainer

HRESULT FS_IOleItemContainer::EnumObjects(DWORD grfFlags, IEnumUnknown **ppenum)
{
	return E_NOTIMPL;
}

HRESULT FS_IOleItemContainer::LockContainer(BOOL fLock)
{
	return S_OK;
}

//IOleItemContainer

HRESULT STDMETHODCALLTYPE FS_IOleItemContainer::GetObject(LPOLESTR pszItem, DWORD dwSpeedNeeded, IBindCtx *pbc, REFIID riid, void **ppvObject)
{
	if (pszItem == NULL)
		return E_INVALIDARG;
	if (ppvObject == NULL)
		return E_INVALIDARG;

	*ppvObject = NULL;
	return MK_E_NOOBJECT;
}

HRESULT FS_IOleItemContainer::GetObjectStorage(LPOLESTR pszItem, IBindCtx *pbc, REFIID riid, void **ppvStorage)
{
	if (pszItem == NULL)
		return E_INVALIDARG;
	if (ppvStorage == NULL)
		return E_INVALIDARG;

	*ppvStorage = NULL;
	return MK_E_NOOBJECT;
}

HRESULT FS_IOleItemContainer::IsRunning(LPOLESTR pszItem)
{
	if (pszItem == NULL)
		return E_INVALIDARG;

	return MK_E_NOOBJECT;
}

//IOleControlSite

HRESULT FS_IOleControlSite::OnControlInfoChanged()
{
	return S_OK;
}

HRESULT FS_IOleControlSite::LockInPlaceActive(BOOL fLock)
{
	m_fs->m_bInPlaceLocked_guess = (fLock) ? true : false;
	return S_OK;
}

HRESULT FS_IOleControlSite::GetExtendedControl(IDispatch **ppDisp)
{
	return E_NOTIMPL;
}

HRESULT FS_IOleControlSite::TransformCoords(POINTL *pPtlHimetric, POINTF *pPtfContainer, DWORD dwFlags)
{
	HRESULT hr_guess = S_OK;

	if (pPtlHimetric == NULL)
		return E_INVALIDARG;
	if (pPtfContainer == NULL)
		return E_INVALIDARG;

	HDC hdc_guess = ::GetDC(m_fs->m_hWndParent_guess);
	::SetMapMode(hdc_guess, MM_HIMETRIC);
	POINT pt_guess[2];
	pt_guess[0].x = 0;
	pt_guess[0].y = 0;

	if (dwFlags & XFORMCOORDS_HIMETRICTOCONTAINER)
	{
		pt_guess[1].x = pPtlHimetric->x;
		pt_guess[1].y = pPtlHimetric->y;
		::LPtoDP(hdc_guess, pt_guess, 2);
		if (dwFlags & XFORMCOORDS_SIZE)
		{
			pPtfContainer->x = (float)(pt_guess[1].x - pt_guess[0].x);
			pPtfContainer->y = (float)(pt_guess[0].y - pt_guess[1].y);
		}
		else if (dwFlags & XFORMCOORDS_POSITION)
		{
			pPtfContainer->x = (float)pt_guess[1].x;
			pPtfContainer->y = (float)pt_guess[1].y;
		}
		else
		{
			hr_guess = E_INVALIDARG;
		}
	}
	else if (dwFlags & XFORMCOORDS_CONTAINERTOHIMETRIC)
	{
		pt_guess[1].x = (int)(pPtfContainer->x);
		pt_guess[1].y = (int)(pPtfContainer->y);
		::DPtoLP(hdc_guess, pt_guess, 2);
		if (dwFlags & XFORMCOORDS_SIZE)
		{
			pPtlHimetric->x = pt_guess[1].x - pt_guess[0].x;
			pPtlHimetric->y = pt_guess[0].y - pt_guess[1].y;
		}
		else if (dwFlags & XFORMCOORDS_POSITION)
		{
			pPtlHimetric->x = pt_guess[1].x;
			pPtlHimetric->y = pt_guess[1].y;
		}
		else
		{
			hr_guess = E_INVALIDARG;
		}
	}
	else
	{
		hr_guess = E_INVALIDARG;
	}

	::ReleaseDC(m_fs->m_hWndParent_guess, hdc_guess);
	return hr_guess;
}

HRESULT FS_IOleControlSite::TranslateAccelerator(LPMSG pMsg, DWORD grfModifiers)
{
	return E_NOTIMPL;
}

HRESULT FS_IOleControlSite::Unused_guess(DWORD, DWORD, DWORD, DWORD)
{
	return S_OK;
}

HRESULT FS_IOleControlSite::OnFocus(BOOL fGotFocus)
{
	return S_OK;
}

HRESULT FS_IOleControlSite::ShowPropertyFrame()
{
	return E_NOTIMPL;
}

//IOleCommandTarget

HRESULT FS_IOleCommandTarget::QueryStatus(const GUID *pguidCmdGroup, ULONG cCmds, OLECMD *prgCmds, OLECMDTEXT *pCmdTet)
{
	if (prgCmds == NULL)
		return E_INVALIDARG;
	bool found_guess = false;
	for (ULONG nCmd_guess = 0; nCmd_guess < cCmds; nCmd_guess++)
	{
		// unsupported by default
		prgCmds[nCmd_guess].cmdf = 0;
	}
	if (!found_guess)
		return OLECMDERR_E_UNKNOWNGROUP;
	return S_OK;
}

HRESULT FS_IOleCommandTarget::Exec(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG *pVaIn, VARIANTARG *pVaOut)
{
	bool found_guess = false;
	if (!found_guess)
		return OLECMDERR_E_UNKNOWNGROUP;
	return OLECMDERR_E_NOTSUPPORTED;
}

//IAdviseSink

void STDMETHODCALLTYPE FS_IAdviseSink2::OnDataChange(FORMATETC *pFormatEtc, STGMEDIUM *pgStgMed)
{
}

void STDMETHODCALLTYPE FS_IAdviseSink2::OnViewChange(DWORD dwAspect, LONG lIndex)
{
	// redraw the control
	m_fs->m_IOleInPlaceSiteWindowless_guess->InvalidateRect(NULL, FALSE);
}

void STDMETHODCALLTYPE FS_IAdviseSink2::OnRename(IMoniker *pmk)
{
}

void STDMETHODCALLTYPE FS_IAdviseSink2::OnSave()
{
}

void STDMETHODCALLTYPE FS_IAdviseSink2::OnClose()
{
}

//IAdviseSink2

void STDMETHODCALLTYPE FS_IAdviseSink2::OnLinkSrcChange(IMoniker *pmk)
{
}

//IAdviseSinkEx

void STDMETHODCALLTYPE FS_IAdviseSinkEx::OnViewStatusChange(DWORD dwViewStatus)
{
}
