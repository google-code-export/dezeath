/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2011 Miranda ICQ/IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

Portions of this code modified for AddContact+ plugin
Copyright (C) 2007-2011 Bartosz 'Dezeath' Bia�ek

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "addcontactplus.h"
#include <limits.h>

// Function from miranda\src\modules\utils\utils.cpp
TCHAR* __fastcall rtrim(TCHAR* str)
{
	if (str == NULL) return NULL;
	TCHAR* p = _tcschr(str, 0);
	while (--p >= str)
	{
		switch (*p)
		{
		case ' ': case '\t': case '\n': case '\r':
			*p = 0; break;
		default:
			return str;
		}
	}
	return str;
}

typedef struct				// mNetSend protocol
{
	PROTOSEARCHRESULT hdr;
	DWORD dwIP;
} NETSENDSEARCHRESULT;

typedef struct				// Myspace protocol
{
	PROTOSEARCHRESULT psr;
	int uid;
} MYPROTOSEARCHRESULT;

typedef struct				// Tlen protocol
{
	PROTOSEARCHRESULT hdr;
	char jid[256];
} TLEN_SEARCH_RESULT;

void AddContactDlgOpts(HWND hdlg, const char* szProto, BOOL bAuthOptsOnly = FALSE)
{
	DWORD flags = (szProto) ? CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_4, 0) : 0;
	if (IsDlgButtonChecked(hdlg, IDC_ADDTEMP))
	{
		EnableWindow(GetDlgItem(hdlg, IDC_ADDED), FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_AUTH), FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_AUTHREQ), FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_AUTHGB), FALSE);
	}
	else
	{
		EnableWindow(GetDlgItem(hdlg, IDC_ADDED), !(flags & PF4_FORCEADDED));
		EnableWindow(GetDlgItem(hdlg, IDC_AUTH), !(flags & PF4_FORCEAUTH));
		EnableWindow(GetDlgItem(hdlg, IDC_AUTHREQ), (flags & PF4_NOCUSTOMAUTH) ? FALSE : IsDlgButtonChecked(hdlg, IDC_AUTH));
		EnableWindow(GetDlgItem(hdlg, IDC_AUTHGB), (flags & PF4_NOCUSTOMAUTH) ? FALSE : IsDlgButtonChecked(hdlg, IDC_AUTH));
	}

	if (bAuthOptsOnly)
		return;

	SetDlgItemText(hdlg, IDC_AUTHREQ, (flags & PF4_NOCUSTOMAUTH) ? _T("") : TranslateT("Please authorize my request and add me to your contact list."));

	char* szUniqueId = (char*)CallProtoService(szProto, PS_GETCAPS, PFLAG_UNIQUEIDTEXT, 0);
	if (szUniqueId)
	{
		size_t cbLen = strlen(szUniqueId) + 2;
		TCHAR* pszUniqueId = (TCHAR*)mir_alloc(cbLen * sizeof(TCHAR));
		mir_sntprintf(pszUniqueId, cbLen, _T(TCHAR_STR_PARAM) _T(":"), szUniqueId);
		SetDlgItemText(hdlg, IDC_IDLABEL, pszUniqueId);
		mir_free(pszUniqueId);
	}
	else
		SetDlgItemText(hdlg, IDC_IDLABEL, TranslateT("Contact ID:"));

	flags = (szProto) ? CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) : 0;
	if (flags & PF1_NUMERICUSERID)
	{
		char buffer[65];
		SetWindowLongPtr(GetDlgItem(hdlg, IDC_USERID), GWL_STYLE, GetWindowLongPtr(GetDlgItem(hdlg, IDC_USERID), GWL_STYLE) | ES_NUMBER);
		if (strstr(szProto, "GG") || strstr(szProto, "MYSPACE"))
			_ultoa(INT_MAX, buffer, 10);
		else
			_ultoa(ULONG_MAX, buffer, 10);
		SendDlgItemMessage(hdlg, IDC_USERID, EM_LIMITTEXT, (WPARAM)strlen(buffer), 0);
	}
	else
	{
		SetWindowLongPtr(GetDlgItem(hdlg, IDC_USERID), GWL_STYLE, GetWindowLongPtr(GetDlgItem(hdlg, IDC_USERID), GWL_STYLE) &~ES_NUMBER);
		SendDlgItemMessage(hdlg, IDC_USERID, EM_LIMITTEXT, 255, 0);
	}
}

void AddContactDlgAccounts(HWND hdlg, ADDCONTACTSTRUCT* acs)
{
		PROTOACCOUNT** pAccounts;
		int iRealAccCount, iAccCount = 0, i;
		DWORD dwCaps;

		ProtoEnumAccounts(&iRealAccCount, &pAccounts);
		for (i = 0; i < iRealAccCount; i++)
		{
			if (!IsAccountEnabled(pAccounts[i])) continue;
			dwCaps = (DWORD)CallProtoService(pAccounts[i]->szModuleName, PS_GETCAPS,PFLAGNUM_1, 0);
			if (dwCaps & PF1_BASICSEARCH || dwCaps & PF1_EXTSEARCH || dwCaps & PF1_SEARCHBYEMAIL || dwCaps & PF1_SEARCHBYNAME)
				iAccCount++;
		}

		if (iAccCount == 0)
		{
			if (GetParent(hdlg) == NULL)
				DestroyWindow(hdlg);
			else
				EndDialog(hdlg, 0);
			return;
		}

		HICON hIcon;
		SIZE textSize;
		RECT rc;
		int iIndex = 0, cbWidth = 0;

		HIMAGELIST hIml = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), (IsWinVerXPPlus() ? ILC_COLOR32 : ILC_COLOR16) | ILC_MASK, iAccCount, 0);
		ImageList_Destroy((HIMAGELIST)SendDlgItemMessage(hdlg, IDC_PROTO, CBEM_SETIMAGELIST, 0, (LPARAM)hIml));
		SendDlgItemMessage(hdlg, IDC_PROTO, CB_RESETCONTENT, 0, 0);

		COMBOBOXEXITEM cbei = {0};
		cbei.mask = CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_TEXT | CBEIF_LPARAM;
		HDC hdc = GetDC(hdlg);
		SelectObject(hdc, (HFONT)SendDlgItemMessage(hdlg, IDC_PROTO, WM_GETFONT, 0, 0));
		for (i = 0; i < iRealAccCount; i++)
		{
			if (!IsAccountEnabled(pAccounts[i])) continue;
			dwCaps = (DWORD)CallProtoService(pAccounts[i]->szModuleName, PS_GETCAPS,PFLAGNUM_1, 0);
			if (!(dwCaps & PF1_BASICSEARCH) && !(dwCaps & PF1_EXTSEARCH) && !(dwCaps & PF1_SEARCHBYEMAIL) && !(dwCaps & PF1_SEARCHBYNAME))
				continue;

			cbei.pszText = pAccounts[i]->tszAccountName;
			GetTextExtentPoint32(hdc, cbei.pszText, lstrlen(cbei.pszText), &textSize);
			if (textSize.cx > cbWidth) cbWidth = textSize.cx;
			hIcon = (HICON)CallProtoService(pAccounts[i]->szModuleName, PS_LOADICON, PLI_PROTOCOL | PLIF_SMALL, 0);
			cbei.iImage = cbei.iSelectedImage = ImageList_AddIcon(hIml, hIcon);
			DestroyIcon(hIcon);
			cbei.lParam = (LPARAM)pAccounts[i]->szModuleName;
			SendDlgItemMessage(hdlg, IDC_PROTO, CBEM_INSERTITEM, 0, (LPARAM)&cbei); 
			if (acs->szProto && cbei.lParam && !strcmp(acs->szProto, pAccounts[i]->szModuleName))
				iIndex = cbei.iItem;
			cbei.iItem++;
		}
		cbWidth += 32;
		SendDlgItemMessage(hdlg, IDC_PROTO, CB_GETDROPPEDCONTROLRECT, 0, (LPARAM)&rc);
		if ((rc.right - rc.left) < cbWidth)
			SendDlgItemMessage(hdlg, IDC_PROTO, CB_SETDROPPEDWIDTH, cbWidth, 0);
		SendDlgItemMessage(hdlg, IDC_PROTO, CB_SETCURSEL, iIndex, 0);
		SendMessage(hdlg, WM_COMMAND, MAKEWPARAM(IDC_PROTO, CBN_SELCHANGE), (LPARAM)GetDlgItem(hdlg, IDC_PROTO));
		if (iAccCount == 1)
			SetFocus(GetDlgItem(hdlg, IDC_USERID));
}

#define DM_ADDCONTACT_CHANGEICONS WM_USER + 11
#define DM_ADDCONTACT_CHANGEACCLIST WM_USER + 12

INT_PTR CALLBACK AddContactDlgProc(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	ADDCONTACTSTRUCT* acs;
	switch (msg)
	{
		case WM_INITDIALOG:
			acs = (ADDCONTACTSTRUCT*)mir_calloc(sizeof(ADDCONTACTSTRUCT));
			acs->handleType = HANDLE_SEARCHRESULT;
			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR)acs);

			Utils_RestoreWindowPositionNoSize(hdlg, NULL, "AddContact", "");
			TranslateDialogDefault(hdlg);
			SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)CallService(MS_SKIN2_GETICON, 1, (LPARAM)ICON_ADD));
			SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD));
			HookEventMessage(ME_SKIN2_ICONSCHANGED, hdlg, DM_ADDCONTACT_CHANGEICONS);
			HookEventMessage(ME_PROTO_ACCLISTCHANGED, hdlg, DM_ADDCONTACT_CHANGEACCLIST);
			
			{
				for (int groupId = 0; groupId < 999; groupId++)
				{
					DBVARIANT dbv;
					char idstr[4];
					int id;
					_itoa(groupId, idstr, 10);
					if (DBGetContactSettingTString(NULL, "CListGroups", idstr, &dbv)) break;
					id = SendDlgItemMessage(hdlg, IDC_GROUP, CB_ADDSTRING, 0, (LPARAM)(dbv.ptszVal + 1));
					SendDlgItemMessage(hdlg, IDC_GROUP, CB_SETITEMDATA, (WPARAM)id, (LPARAM)groupId + 1);
					DBFreeVariant(&dbv);
				}
			}
			SendDlgItemMessage(hdlg, IDC_GROUP, CB_INSERTSTRING, 0, (LPARAM)TranslateT("None"));
			SendDlgItemMessage(hdlg, IDC_GROUP, CB_SETCURSEL, 0, 0);

			AddContactDlgAccounts(hdlg, acs);
			// By default check these checkboxes
			CheckDlgButton(hdlg, IDC_ADDED, BST_CHECKED);
			CheckDlgButton(hdlg, IDC_AUTH, BST_CHECKED);
			AddContactDlgOpts(hdlg, acs->szProto);
			EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
			break;

		case WM_COMMAND:
			acs = (ADDCONTACTSTRUCT*)GetWindowLongPtr(hdlg, GWLP_USERDATA);
			switch (LOWORD(wparam))
			{
				case IDC_USERID:
					if (HIWORD(wparam) == EN_CHANGE)
					{
						TCHAR szUserId[256];
						if (GetDlgItemText(hdlg, IDC_USERID, szUserId, SIZEOF(szUserId)))
						{
							if (!IsWindowEnabled(GetDlgItem(hdlg, IDOK)))
								EnableWindow(GetDlgItem(hdlg, IDOK), TRUE);
						}
						else if (IsWindowEnabled(GetDlgItem(hdlg, IDOK)))
							EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
					}
					break;

				case IDC_PROTO:
					if (HIWORD(wparam) == CBN_SELCHANGE || HIWORD(wparam) == CBN_SELENDOK)
					{
						acs->szProto = (char*)SendDlgItemMessage(hdlg, IDC_PROTO, CB_GETITEMDATA, (WPARAM)SendDlgItemMessage(hdlg, IDC_PROTO, CB_GETCURSEL, 0, 0), 0);
						// TODO remember last setting for each proto?
						AddContactDlgOpts(hdlg, acs->szProto);
					}
					if (HIWORD(wparam) == CBN_CLOSEUP)
						SetFocus(GetDlgItem(hdlg, IDC_USERID));
					break;

				case IDC_ADDTEMP:
					AddContactDlgOpts(hdlg, acs->szProto, TRUE);
					break;

				case IDC_AUTH:
				{
					DWORD flags = CallProtoService(acs->szProto, PS_GETCAPS, PFLAGNUM_4,0);
					if (flags & PF4_NOCUSTOMAUTH)
					{
						EnableWindow(GetDlgItem(hdlg, IDC_AUTHREQ), FALSE);
						EnableWindow(GetDlgItem(hdlg, IDC_AUTHGB), FALSE);
					}
					else
					{
						EnableWindow(GetDlgItem(hdlg, IDC_AUTHREQ), IsDlgButtonChecked(hdlg, IDC_AUTH));
						EnableWindow(GetDlgItem(hdlg, IDC_AUTHGB), IsDlgButtonChecked(hdlg, IDC_AUTH));
					}
					break;
				}

				case IDOK:
				{
					HANDLE hContact = INVALID_HANDLE_VALUE;
					PROTOSEARCHRESULT* psr;

					TCHAR szUserId[256];
					GetDlgItemText(hdlg, IDC_USERID, szUserId, SIZEOF(szUserId));

					if (*rtrim(szUserId) == 0 ||
						(strstr(acs->szProto, "GG") && _tcstoul(szUserId, NULL, 10) > INT_MAX) || // Gadu-Gadu protocol
						((CallProtoService(acs->szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_NUMERICUSERID) && !_tcstoul(szUserId, NULL, 10)))
					{
						MessageBox(NULL,
							TranslateT("The contact cannot be added to your contact list. Please make sure the contact ID is entered correctly."),
							TranslateT("Add Contact"), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
						break;
					}

					if (strstr(acs->szProto, "MNETSEND")) // mNetSend protocol
					{
						psr = (PROTOSEARCHRESULT*)mir_calloc(sizeof(NETSENDSEARCHRESULT));
						psr->cbSize = sizeof(NETSENDSEARCHRESULT);
					}
					else if (strstr(acs->szProto, "MYSPACE")) // Myspace protocol
					{
						if (_tcstoul(szUserId, NULL, 10) > INT_MAX)
						{
							MessageBox(NULL,
								TranslateT("The contact cannot be added to your contact list. Please make sure the contact ID is entered correctly."),
								TranslateT("Add Contact"), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
							break;
						}
						psr = (PROTOSEARCHRESULT*)mir_calloc(sizeof(MYPROTOSEARCHRESULT));
						psr->cbSize = sizeof(MYPROTOSEARCHRESULT);
						((MYPROTOSEARCHRESULT*)psr)->uid = _tcstoul(szUserId, NULL, 10);
					}
					else if (strstr(acs->szProto, "TLEN")) // Tlen protocol
					{
						if (_tcschr(szUserId, '@') == NULL)
						{
							MessageBox(NULL,
								TranslateT("The contact cannot be added to your contact list. Please make sure the contact ID is entered correctly."),
								TranslateT("Add Contact"), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
							break;
						}
						psr = (PROTOSEARCHRESULT*)mir_calloc(sizeof(TLEN_SEARCH_RESULT));
						psr->cbSize = sizeof(TLEN_SEARCH_RESULT);
						mir_snprintf(((TLEN_SEARCH_RESULT*)psr)->jid, SIZEOF(((TLEN_SEARCH_RESULT*)psr)->jid), TCHAR_STR_PARAM, szUserId);
					}
					else
					{
						psr = (PROTOSEARCHRESULT*)mir_calloc(sizeof(PROTOSEARCHRESULT));
						psr->cbSize = sizeof(PROTOSEARCHRESULT);
					}

					psr->flags = PSR_TCHAR;
					psr->id = mir_tstrdup(szUserId);
					acs->psr = psr;

					hContact = (HANDLE)CallProtoService(acs->szProto, PS_ADDTOLIST, IsDlgButtonChecked(hdlg, IDC_ADDTEMP) ? PALF_TEMPORARY : 0, (LPARAM)acs->psr);

					if (hContact == NULL)
					{
						MessageBox(NULL,
							TranslateT("The contact cannot be added to your contact list. If you are not logged into the selected account, please try to do so. Also, make sure the contact ID is entered correctly."),
							TranslateT("Add Contact"), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
						break;
					}

					TCHAR szHandle[256];
					if (GetDlgItemText(hdlg, IDC_MYHANDLE, szHandle, SIZEOF(szHandle)))
						DBWriteContactSettingTString(hContact, "CList", "MyHandle", szHandle);

					int item = SendDlgItemMessage(hdlg, IDC_GROUP, CB_GETCURSEL, 0, 0);
					if (item > 0)
					{
						item = SendDlgItemMessage(hdlg, IDC_GROUP, CB_GETITEMDATA, item, 0);
						CallService(MS_CLIST_CONTACTCHANGEGROUP, (WPARAM)hContact, item);
					}

					if (!IsDlgButtonChecked(hdlg, IDC_ADDTEMP))
					{
						DBDeleteContactSetting(hContact, "CList", "NotOnList");

						if (IsDlgButtonChecked(hdlg, IDC_ADDED))
							CallContactService(hContact, PSS_ADDED, 0, 0);

						if (IsDlgButtonChecked(hdlg, IDC_AUTH)) 
						{
							DWORD flags = CallProtoService(acs->szProto, PS_GETCAPS, PFLAGNUM_4, 0);
							if (flags & PF4_NOCUSTOMAUTH)
								CallContactService(hContact, PSS_AUTHREQUESTT, 0, 0);
							else
							{
								TCHAR szReason[512];
								GetDlgItemText(hdlg, IDC_AUTHREQ, szReason, SIZEOF(szReason));
								CallContactService(hContact, PSS_AUTHREQUESTT, 0, (LPARAM)szReason);
							}
						}
					}

					if (GetAsyncKeyState(VK_CONTROL))
						CallService(MS_MSG_SENDMESSAGE, (WPARAM)hContact, (LPARAM)(const char*)NULL);
				}
				// fall through
				case IDCANCEL:
					if (GetParent(hdlg) == NULL)
						DestroyWindow(hdlg);
					else
						EndDialog(hdlg, 0);
					break;
			}
			break;

		case WM_CLOSE:
			/* if there is no parent for the dialog, its a modeless dialog and can't be killed using EndDialog() */
			if (GetParent(hdlg) == NULL)
				DestroyWindow(hdlg);
			else
				EndDialog(hdlg, 0);
			break;

		case DM_ADDCONTACT_CHANGEICONS:
			CallService(MS_SKIN2_RELEASEICONBIG, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)CallService(MS_SKIN2_GETICON, 1, (LPARAM)ICON_ADD)), 0);
			CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD)), 0);
			break;

		case DM_ADDCONTACT_CHANGEACCLIST:
		{
			acs = (ADDCONTACTSTRUCT*)GetWindowLongPtr(hdlg, GWLP_USERDATA);
			AddContactDlgAccounts(hdlg, acs);
			break;
		}

		case WM_DESTROY:
			CallService(MS_SKIN2_RELEASEICONBIG, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)NULL), 0);
			CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)NULL), 0);
			ImageList_Destroy((HIMAGELIST)SendDlgItemMessage(hdlg, IDC_PROTO, CBEM_GETIMAGELIST, 0, 0));
			acs = (ADDCONTACTSTRUCT*)GetWindowLongPtr(hdlg, GWLP_USERDATA);
			if (acs)
			{
				if (acs->psr)
				{
					mir_free(acs->psr->nick);
					mir_free(acs->psr->firstName);
					mir_free(acs->psr->lastName);
					mir_free(acs->psr->email);
					mir_free(acs->psr);
				}
				mir_free(acs);
			}
			Utils_SaveWindowPosition(hdlg, NULL, "AddContact", "");
			break;
	}

	return FALSE;
}
