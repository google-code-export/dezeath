/*

AddContact+ plugin for Miranda IM

Copyright © 2007-2010 Bartosz 'Dezeath' Bia³ek

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include "addcontactplus.h"

HINSTANCE hInst;
PLUGINLINK *pluginLink;
static HANDLE hModulesLoaded = 0, hChangedIcons = 0, hAccListChanged = 0,
			  hMainMenuItem = 0, hToolBarItem = 0, hService = 0;
HANDLE hIconLibItem;
struct MM_INTERFACE	mmi;

PLUGININFOEX pluginInfo = {
	sizeof(PLUGININFOEX),
	"AddContact+",
	PLUGIN_MAKE_VERSION(0,9,8,6),
	"Provides the ability to add contacts manually (without searching for them)",
	"Bartosz 'Dezeath' Bia³ek",
	"dezred"/*antispam*/"@"/*antispam*/"gmail"/*antispam*/"."/*antispam*/"com",
	"© 2007-2010 Bartosz 'Dezeath' Bia³ek",
	"http://code.google.com/p/dezeath",
	UNICODE_AWARE,
	0,
#ifdef _UNICODE
	// {6471D451-2FE0-4ee2-850E-9F84F3C0D187}
	{ 0x6471d451, 0x2fe0, 0x4ee2, { 0x85, 0xe, 0x9f, 0x84, 0xf3, 0xc0, 0xd1, 0x87 } }
#else
	// {64B41F85-A2D1-4cac-AA35-658DF950FE05}
	{ 0x64b41f85, 0xa2d1, 0x4cac, { 0xaa, 0x35, 0x65, 0x8d, 0xf9, 0x50, 0xfe, 0x5 } }
#endif
};

static const MUUID interfaces[] = {MIID_ADDCONTACTPLUS, MIID_LAST};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	hInst = hinstDLL;
	return TRUE;
}

__declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD mirandaVersion)
{
	if (mirandaVersion < PLUGIN_MAKE_VERSION(0,8,0,0)) {
		MessageBox(NULL, _T("The AddContact+ plugin cannot be loaded. It requires Miranda IM 0.8 or later."), _T("AddContact+ Plugin"), MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST);
		return NULL;
	}
	return &pluginInfo;
}

__declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}

int AddContactPlusDialog(WPARAM wParam,LPARAM lParam)
{
	CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_ADDCONTACT), (HWND)NULL, AddContactDlgProc, 0);
	return 0;
}

static int IconsChanged(WPARAM wParam,LPARAM lParam)
{
	CLISTMENUITEM mi = {0};

	if (!hMainMenuItem)
		return 0;

	mi.cbSize = sizeof(mi);
	mi.flags = CMIM_ICON | CMIF_ICONFROMICOLIB;
	mi.icolibItem = hIconLibItem;
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hMainMenuItem, (LPARAM)&mi);

	return 0;
}

static int AccListChanged(WPARAM wParam, LPARAM lParam)
{
	PROTOACCOUNT **accounts;
	int proto_count, ProtoCount = 0, i;
	DWORD caps;

	ProtoEnumAccounts(&proto_count, &accounts);
	for (i = 0; i < proto_count; i++) {
		if (!IsAccountEnabled(accounts[i])) continue;
		caps = (DWORD)CallProtoService(accounts[i]->szModuleName, PS_GETCAPS,PFLAGNUM_1, 0);
		if (!(caps & PF1_BASICSEARCH) && !(caps & PF1_EXTSEARCH) && !(caps & PF1_SEARCHBYEMAIL)	&& !(caps & PF1_SEARCHBYNAME))
			continue;
		ProtoCount++;
	}

	if (ProtoCount) {
		CLISTMENUITEM mi = {0};

		if (hMainMenuItem)
			return 0;

		mi.cbSize = sizeof(mi);
		mi.position = 500020001;
		mi.flags = CMIF_ICONFROMICOLIB;
		mi.icolibItem = hIconLibItem;
		mi.pszName = LPGEN("&Add Contact Manually...");
		mi.pszService = MS_ADDCONTACTPLUS_SHOW;
		hMainMenuItem = (HANDLE)CallService(MS_CLIST_ADDMAINMENUITEM, 0,(LPARAM)&mi);

		if (ServiceExists(MS_TB_ADDBUTTON)) {
			TBButton tbb = {0};

			tbb.cbSize = sizeof(TBButton);
			tbb.pszButtonID = "acplus_btn";
			tbb.pszButtonName = Translate("Add Contact Manually");
			tbb.pszServiceName = MS_ADDCONTACTPLUS_SHOW;
			tbb.pszTooltipUp = Translate("Add Contact Manually");
			tbb.hPrimaryIconHandle = hIconLibItem;
			tbb.tbbFlags = TBBF_VISIBLE;
			tbb.defPos = 10100;
			hToolBarItem = (HANDLE)CallService(MS_TB_ADDBUTTON, 0, (LPARAM)&tbb);
		}
	}
	else {
		if (!hMainMenuItem)
			return 0;

		CallService(MS_CLIST_REMOVEMAINMENUITEM, (WPARAM)hMainMenuItem, 0);
		CallService(MS_TB_REMOVEBUTTON, (WPARAM)hToolBarItem, 0);

		hMainMenuItem = 0;
	}

	return 0;
}

int InitAwayModule(WPARAM wParam,LPARAM lParam)
{
	SKINICONDESC ico = {0};
	char szFile[MAX_PATH];

	GetModuleFileNameA(hInst, szFile, MAX_PATH);

	ico.cbSize = sizeof(ico);
	ico.flags = SIDF_TCHAR;
	ico.cx = ico.cy = 16;
	ico.pszDefaultFile = szFile;
	ico.ptszSection = LPGENT("AddContact+");
	ico.iDefaultIndex = -IDI_ADDCONTACT;
	ico.ptszDescription = LPGENT("Add Contact Manually");
	ico.pszName = ICON_ADD;
	hIconLibItem = (HANDLE)CallService(MS_SKIN2_ADDICON, (WPARAM)0, (LPARAM)&ico);
	hChangedIcons = HookEvent(ME_SKIN2_ICONSCHANGED, IconsChanged);

	AccListChanged(0, 0);

	return 0;
}

int __declspec(dllexport) Load(PLUGINLINK *link)
{
	pluginLink = link;

	mir_getMMI(&mmi);
	hModulesLoaded = HookEvent(ME_SYSTEM_MODULESLOADED, InitAwayModule);
	hAccListChanged = HookEvent(ME_PROTO_ACCLISTCHANGED, AccListChanged);
	hService = CreateServiceFunction(MS_ADDCONTACTPLUS_SHOW, AddContactPlusDialog);

	return 0;
}

int __declspec(dllexport) Unload(void)
{
	UnhookEvent(hModulesLoaded);
	UnhookEvent(hChangedIcons);
	UnhookEvent(hAccListChanged);

	DestroyServiceFunction(hService);

	return 0;
}
