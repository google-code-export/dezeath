/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2009 Miranda ICQ/IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

Portions of this code modified for AddContact+ plugin
Copyright � 2007-2011 Bartosz 'Dezeath' Bia�ek

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
#include <limits.h>

// Function from miranda\src\modules\utils\utils.cpp
TCHAR* __fastcall rtrim(TCHAR *str)
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

void AddContactDlgOpts(HWND hdlg, const char *szProto)
{
	// By default check both checkboxes
	CheckDlgButton(hdlg, IDC_ADDED, BST_CHECKED);
	CheckDlgButton(hdlg, IDC_AUTH, BST_CHECKED);

	DWORD flags = (szProto) ? CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_4, 0) : 0;
	EnableWindow(GetDlgItem(hdlg, IDC_ADDED), !(flags & PF4_FORCEADDED));
	EnableWindow(GetDlgItem(hdlg, IDC_AUTH), !(flags & PF4_FORCEAUTH));
	EnableWindow(GetDlgItem(hdlg, IDC_AUTHREQ), (flags & PF4_NOCUSTOMAUTH) ? FALSE : IsDlgButtonChecked(hdlg, IDC_AUTH));
	EnableWindow(GetDlgItem(hdlg, IDC_AUTHGB), (flags & PF4_NOCUSTOMAUTH) ? FALSE : IsDlgButtonChecked(hdlg, IDC_AUTH));
	SetDlgItemText(hdlg, IDC_AUTHREQ, (flags & PF4_NOCUSTOMAUTH) ? _T("") : TranslateT("Please authorize my request and add me to your contact list."));

	flags = (szProto) ? CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) : 0;
/*	if (flags & PF1_BASICSEARCH)
	{
		char *szUniqueId = (char*)CallProtoService(szProto, PS_GETCAPS, PFLAG_UNIQUEIDTEXT, 0);
		if (szUniqueId)
		{
//			if (szUniqueId[strlen(szUniqueId)-1] != ':')
//				strcat(szUniqueId, ":");
			#ifdef _UNICODE
			{
				TCHAR* p = mir_a2u(szUniqueId);
				SetDlgItemText(hdlg, IDC_IDLABEL, p);
				mir_free(p);
			}
			#else
				SetDlgItemTextA(hdlg, IDC_IDLABEL, szUniqueId);
			#endif
		}
		else
			SetDlgItemText(hdlg, IDC_IDLABEL, TranslateT("User ID:"));
	}*/
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

#define DM_ADDCONTACT_CHANGEICONS WM_USER + 11
#define DM_ADDCONTACT_CHANGEACCLIST WM_USER + 12

INT_PTR CALLBACK AddContactDlgProc(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	ADDCONTACTSTRUCT* acs;
	switch (msg)
	{
		case WM_INITDIALOG:
			acs = (ADDCONTACTSTRUCT*)mir_alloc(sizeof(ADDCONTACTSTRUCT));
			acs->handleType = HANDLE_SEARCHRESULT;
			acs->handle = NULL;
			acs->psr = NULL;
			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR)acs);

			TranslateDialogDefault(hdlg);
			HookEventMessage(ME_SKIN2_ICONSCHANGED, hdlg, DM_ADDCONTACT_CHANGEICONS);
			HookEventMessage(ME_PROTO_ACCLISTCHANGED, hdlg, DM_ADDCONTACT_CHANGEACCLIST);
			SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD));
			SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD));
			
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
			{
				PROTOACCOUNT** accounts;
				int proto_count, id, i;
				DWORD caps;

				ProtoEnumAccounts(&proto_count, &accounts);
				for (i = 0; i < proto_count; i++)
				{
					if (!IsAccountEnabled(accounts[i])) continue;
					caps = (DWORD)CallProtoService(accounts[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1,0);
					if (!(caps & PF1_BASICSEARCH) && !(caps & PF1_EXTSEARCH) && !(caps & PF1_SEARCHBYEMAIL) && !(caps & PF1_SEARCHBYNAME))
						continue;

					id = SendDlgItemMessage(hdlg, IDC_PROTO, CB_ADDSTRING, 0, (LPARAM)accounts[i]->tszAccountName);
					SendDlgItemMessage(hdlg, IDC_PROTO, CB_SETITEMDATA, (WPARAM)id, (LPARAM)i);
				}
				SendDlgItemMessage(hdlg, IDC_PROTO, CB_SETCURSEL, 0, 0);
				SendMessage(hdlg, WM_COMMAND, MAKEWPARAM(IDC_PROTO, CBN_SELCHANGE), (LPARAM)GetDlgItem(hdlg, IDC_PROTO));

				i = SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETITEMDATA, (WPARAM)SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETCURSEL, 0, 0), 0);
				acs->szProto = accounts[i]->szModuleName;
			}

			AddContactDlgOpts(hdlg, acs->szProto);
			EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
			break;

		case WM_COMMAND:
			acs = (ADDCONTACTSTRUCT *)GetWindowLongPtr(hdlg, GWLP_USERDATA);
			switch (LOWORD(wparam))
			{
				case IDC_USERID:
					if (HIWORD(wparam) == EN_CHANGE)
					{
						TCHAR szUserId[256];
						if (GetDlgItemText(hdlg, IDC_USERID, szUserId, SIZEOF(szUserId)))
						{
							if (!IsWindowEnabled(GetDlgItem(hdlg, IDOK)))
								EnableWindow(GetDlgItem(hdlg,IDOK),TRUE);
						}
						else if (IsWindowEnabled(GetDlgItem(hdlg, IDOK)))
							EnableWindow(GetDlgItem(hdlg,IDOK),FALSE);
					}
					break;

				case IDC_PROTO:
					if (HIWORD(wparam) == CBN_SELCHANGE || HIWORD(wparam) == CBN_SELENDOK)
					{
						PROTOACCOUNT **accounts;
						int proto_count, i;
						ProtoEnumAccounts(&proto_count, &accounts);
						i = SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETITEMDATA, (WPARAM)SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETCURSEL, 0, 0), 0);
						acs->szProto = accounts[i]->szModuleName;
						// TODO remember last setting for each proto?
						AddContactDlgOpts(hdlg, acs->szProto);
					}
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
						(strstr(acs->szProto, "GG") && _tcstoul(szUserId, NULL, 10) > INT_MAX)) // Gadu-Gadu protocol
					{
						MessageBox( NULL,
							TranslateT("The contact cannot be added to your contact list. Make sure the User ID is entered properly."),
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
								TranslateT("The contact cannot be added to your contact list. Make sure the User ID is entered properly."),
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
							MessageBox( NULL,
								TranslateT("The contact cannot be added to your contact list. Make sure the User ID is entered properly."),
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

					hContact = (HANDLE)CallProtoService(acs->szProto, PS_ADDTOLIST, 0, (LPARAM)acs->psr);

					if (hContact == NULL)
					{
						MessageBox(NULL,
							TranslateT("The contact cannot be added to your contact list. If you are not connected to that network, try to connect. Also, make sure the User ID is entered properly."),
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
			CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD)), 0);
			CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD)), 0);
			break;

		case DM_ADDCONTACT_CHANGEACCLIST:
		{
			PROTOACCOUNT** accounts;
			int proto_count, ProtoCount = 0, id, i;
			DWORD caps;

			acs = (ADDCONTACTSTRUCT*)GetWindowLongPtr(hdlg, GWLP_USERDATA);
			SendDlgItemMessage(hdlg, IDC_PROTO, CB_RESETCONTENT, 0, 0);
			ProtoEnumAccounts(&proto_count, &accounts);
			for (i = 0; i < proto_count; i++)
			{
				if (!IsAccountEnabled(accounts[i])) continue;
				caps = (DWORD)CallProtoService(accounts[i]->szModuleName, PS_GETCAPS,PFLAGNUM_1, 0);
				if (!(caps & PF1_BASICSEARCH) && !(caps & PF1_EXTSEARCH) && !(caps & PF1_SEARCHBYEMAIL) && !(caps & PF1_SEARCHBYNAME))
					continue;

				ProtoCount++;
				id = SendDlgItemMessage(hdlg, IDC_PROTO, CB_ADDSTRING, 0, (LPARAM)accounts[i]->tszAccountName);
				SendDlgItemMessage(hdlg, IDC_PROTO, CB_SETITEMDATA, (WPARAM)id, (LPARAM)i);
			}

			if (!ProtoCount)
			{
				if (GetParent(hdlg) == NULL)
					DestroyWindow(hdlg);
				else
					EndDialog( hdlg, 0 );
				break;
			}
			SendDlgItemMessage(hdlg, IDC_PROTO, CB_SETCURSEL, 0, 0);
			SendMessage(hdlg, WM_COMMAND, MAKEWPARAM(IDC_PROTO, CBN_SELCHANGE), (LPARAM)GetDlgItem(hdlg, IDC_PROTO));

			i = SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETITEMDATA, (WPARAM)SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETCURSEL, 0, 0), 0);
			acs->szProto = accounts[i]->szModuleName;
			break;
		}

		case WM_DESTROY:
			CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)NULL), 0);
			CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)NULL), 0);
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
			break;
	}

	return FALSE;
}