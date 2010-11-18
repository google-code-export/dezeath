/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2009 Miranda ICQ/IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

Portions of this code modified for AddContact+ plugin
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
#include <limits.h>

typedef struct	//Gadu-Gadu
{
	PROTOSEARCHRESULT hdr;
	unsigned int uin;
} GGSEARCHRESULT;

/* in m_icq.h
typedef struct {   //extended search result structure, used for all searches
  PROTOSEARCHRESULT hdr;
  DWORD uin;
  BYTE auth;
  char* uid;
} ICQSEARCHRESULT;*/

struct JABBER_SEARCH_RESULT //Jabber
{
	PROTOSEARCHRESULT hdr;
	TCHAR jid[256];
};

typedef struct	//mNetSend
{
	PROTOSEARCHRESULT hdr;
	DWORD dwIP;
} NETSENDSEARCHRESULT;

typedef struct { //MySpace
	PROTOSEARCHRESULT psr;
	int uid;
} MYPROTOSEARCHRESULT;

typedef struct { //Tlen
	PROTOSEARCHRESULT hdr;
	char jid[256];
} TLEN_SEARCH_RESULT;


#define DM_ADDCONTACT_CHANGEICONS WM_USER+11
#define DM_ADDCONTACT_CHANGEACCLIST WM_USER+12

INT_PTR CALLBACK AddContactDlgProc(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	ADDCONTACTSTRUCT *acs;

	switch(msg) {
	case WM_INITDIALOG:
		acs = (ADDCONTACTSTRUCT*)mir_alloc(sizeof(ADDCONTACTSTRUCT));
		acs->handleType = HANDLE_SEARCHRESULT;
		acs->handle = NULL;
		acs->psr = NULL;
		SetWindowLongPtr(hdlg, GWLP_USERDATA,(LONG_PTR)acs);

		TranslateDialogDefault(hdlg);
		HookEventMessage(ME_SKIN2_ICONSCHANGED, hdlg, DM_ADDCONTACT_CHANGEICONS);
		HookEventMessage(ME_PROTO_ACCLISTCHANGED, hdlg, DM_ADDCONTACT_CHANGEACCLIST);
		SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD));
		SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD));
		
		{
			int groupId;
			for ( groupId = 0; groupId < 999; groupId++ ) {
				DBVARIANT dbv;
				char idstr[4];
				int id;
				_itoa(groupId,idstr,10);
				if(DBGetContactSettingTString(NULL,"CListGroups",idstr,&dbv)) break;
				id = SendDlgItemMessage(hdlg,IDC_GROUP,CB_ADDSTRING,0,(LPARAM)(dbv.ptszVal+1));
				SendDlgItemMessage(hdlg,IDC_GROUP,CB_SETITEMDATA ,(WPARAM)id,(LPARAM)groupId+1);
				DBFreeVariant(&dbv);
			}
		}

		SendDlgItemMessage(hdlg,IDC_GROUP,CB_INSERTSTRING,0,(LPARAM)TranslateT("None"));
		SendDlgItemMessage(hdlg,IDC_GROUP,CB_SETCURSEL,0,0);
		{
			PROTOACCOUNT **accounts;
			int proto_count, id, i;
			DWORD caps;

			ProtoEnumAccounts(&proto_count, &accounts);
			for(i=0; i<proto_count; i++) {
				if (!IsAccountEnabled(accounts[i])) continue;
				caps=(DWORD)CallProtoService(accounts[i]->szModuleName,PS_GETCAPS,PFLAGNUM_1,0);
				if (!(caps&PF1_BASICSEARCH) && !(caps&PF1_EXTSEARCH) && !(caps&PF1_SEARCHBYEMAIL) && !(caps&PF1_SEARCHBYNAME))
					continue;

				id = SendDlgItemMessage(hdlg,IDC_PROTO,CB_ADDSTRING,0,(LPARAM)accounts[i]->tszAccountName);
				SendDlgItemMessage(hdlg,IDC_PROTO,CB_SETITEMDATA,(WPARAM)id,(LPARAM)i);
			}
			SendDlgItemMessage(hdlg,IDC_PROTO,CB_SETCURSEL,0,0);
			SendMessage(hdlg, WM_COMMAND, MAKEWPARAM(IDC_PROTO, CBN_SELCHANGE), (LPARAM)GetDlgItem(hdlg, IDC_PROTO));

			i = SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETITEMDATA, (WPARAM)SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETCURSEL, 0, 0), 0);
			acs->szProto = accounts[i]->szModuleName;
		}

		{
			DWORD flags = CallProtoService(acs->szProto,PS_GETCAPS,PFLAGNUM_4,0);
			if (flags&PF4_FORCEADDED) { // force you were added requests for this protocol
				CheckDlgButton(hdlg,IDC_ADDED,BST_CHECKED);
				EnableWindow(GetDlgItem(hdlg,IDC_ADDED),FALSE);
			}
			if (flags&PF4_FORCEAUTH) { // force auth requests for this protocol
				CheckDlgButton(hdlg,IDC_AUTH,BST_CHECKED);
				EnableWindow(GetDlgItem(hdlg,IDC_AUTH),FALSE);
			}
			if (flags&PF4_NOCUSTOMAUTH) {
				EnableWindow(GetDlgItem(hdlg,IDC_AUTHREQ),FALSE);
				EnableWindow(GetDlgItem(hdlg,IDC_AUTHGB),FALSE);
			}

			flags = CallProtoService(acs->szProto,PS_GETCAPS,PFLAGNUM_1,0);
/*			if(flags&PF1_BASICSEARCH) {
				char *szUniqueId;
				szUniqueId=(char*)CallProtoService(acs->szProto,PS_GETCAPS,PFLAG_UNIQUEIDTEXT,0);
				if(szUniqueId) {
//					strcat(szUniqueId, ":");
					#ifdef _UNICODE
					{
						TCHAR* p = mir_a2u(szUniqueId);
						SetDlgItemText(hdlg,IDC_IDLABEL,p);
						mir_free(p);
					}
					#else
						SetDlgItemTextA(hdlg,IDC_IDLABEL,szUniqueId);
					#endif
				}
				else
					SetDlgItemText(hdlg,IDC_IDLABEL,TranslateT("User ID:"));

			}*/
			if(flags&PF1_NUMERICUSERID) {
				char buffer[65];
				SetWindowLongPtr(GetDlgItem(hdlg,IDC_USERID),GWL_STYLE,GetWindowLongPtr(GetDlgItem(hdlg,IDC_USERID),GWL_STYLE)|ES_NUMBER);
				if (strstr(acs->szProto, "GG") || strstr(acs->szProto, "MYSPACE"))
					_ultoa(INT_MAX, buffer, 10);
				else
					_ultoa(ULONG_MAX, buffer, 10);
				SendDlgItemMessage(hdlg, IDC_USERID, EM_LIMITTEXT, (WPARAM)strlen(buffer), 0);
			}
			else {
				SetWindowLongPtr(GetDlgItem(hdlg,IDC_USERID),GWL_STYLE,GetWindowLongPtr(GetDlgItem(hdlg,IDC_USERID),GWL_STYLE)&~ES_NUMBER);
				SendDlgItemMessage(hdlg, IDC_USERID, EM_LIMITTEXT, 255, 0);
			}
		}
		SetDlgItemText(hdlg,IDC_AUTHREQ,TranslateT("Please authorize my request and add me to your contact list."));
		EnableWindow(GetDlgItem(hdlg,IDC_AUTHREQ),IsDlgButtonChecked(hdlg,IDC_AUTH));
		EnableWindow(GetDlgItem(hdlg,IDC_AUTHGB),IsDlgButtonChecked(hdlg,IDC_AUTH));
		EnableWindow(GetDlgItem(hdlg,IDOK),FALSE);
		break;

	case WM_COMMAND:
		acs=(ADDCONTACTSTRUCT *)GetWindowLongPtr(hdlg,GWLP_USERDATA);

		switch(LOWORD(wparam)) {
		case IDC_USERID:
			switch(HIWORD(wparam)) {
				case EN_CHANGE: {
					TCHAR szUserId[256];

					if (GetDlgItemText(hdlg, IDC_USERID, szUserId, SIZEOF(szUserId))) {
						if (!IsWindowEnabled(GetDlgItem(hdlg, IDOK)))
							EnableWindow(GetDlgItem(hdlg,IDOK),TRUE);
					}
					else if (IsWindowEnabled(GetDlgItem(hdlg, IDOK)))
						EnableWindow(GetDlgItem(hdlg,IDOK),FALSE);
				}
				break;
			}
		break;
		case IDC_PROTO:
			switch(HIWORD(wparam)) {
				case CBN_SELCHANGE:
				case CBN_SELENDOK: {
					PROTOACCOUNT **accounts;
					int proto_count, i;

					ProtoEnumAccounts(&proto_count, &accounts);
					i = SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETITEMDATA, (WPARAM)SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETCURSEL, 0, 0), 0);
					acs->szProto = accounts[i]->szModuleName;

					{ //TODO: remember last setting for each proto?
						DWORD flags = CallProtoService(acs->szProto,PS_GETCAPS,PFLAGNUM_4,0);
						if (flags&PF4_FORCEADDED) { // force you were added requests for this protocol
							CheckDlgButton(hdlg,IDC_ADDED,BST_CHECKED);
							EnableWindow(GetDlgItem(hdlg,IDC_ADDED),FALSE);
						}
						else {
							CheckDlgButton(hdlg,IDC_ADDED,BST_UNCHECKED);
							EnableWindow(GetDlgItem(hdlg,IDC_ADDED),TRUE);
						}
						if (flags&PF4_FORCEAUTH) { // force auth requests for this protocol
							CheckDlgButton(hdlg,IDC_AUTH,BST_CHECKED);
							EnableWindow(GetDlgItem(hdlg,IDC_AUTH),FALSE);
						}
						else {
							CheckDlgButton(hdlg,IDC_AUTH,BST_UNCHECKED);
							EnableWindow(GetDlgItem(hdlg,IDC_AUTH),TRUE);
						}
						if (flags&PF4_NOCUSTOMAUTH) {
							EnableWindow(GetDlgItem(hdlg,IDC_AUTHREQ),FALSE);
							EnableWindow(GetDlgItem(hdlg,IDC_AUTHGB),FALSE);
						}
						else {
							EnableWindow(GetDlgItem(hdlg,IDC_AUTHREQ),TRUE);
							EnableWindow(GetDlgItem(hdlg,IDC_AUTHGB),TRUE);
						}

						EnableWindow(GetDlgItem(hdlg,IDC_AUTHREQ),IsDlgButtonChecked(hdlg,IDC_AUTH));
						EnableWindow(GetDlgItem(hdlg,IDC_AUTHGB),IsDlgButtonChecked(hdlg,IDC_AUTH));

						flags = CallProtoService(acs->szProto,PS_GETCAPS,PFLAGNUM_1,0);
/*						if(flags&PF1_BASICSEARCH) {
							char *szUniqueId;
							szUniqueId=(char*)CallProtoService(acs->szProto,PS_GETCAPS,PFLAG_UNIQUEIDTEXT,0);
							if(szUniqueId) {
//								if (szUniqueId[strlen(szUniqueId)-1] != ':')
//									strcat(szUniqueId, ":");
								#ifdef _UNICODE
								{
									TCHAR* p = mir_a2u(szUniqueId);
									SetDlgItemText(hdlg,IDC_IDLABEL,p);
									mir_free(p);
								}
								#else
									SetDlgItemTextA(hdlg,IDC_IDLABEL,szUniqueId);
								#endif
							}
							else
								SetDlgItemText(hdlg,IDC_IDLABEL,TranslateT("User ID:"));
						}*/
						if(flags&PF1_NUMERICUSERID) {
							char buffer[65];
							SetWindowLongPtr(GetDlgItem(hdlg,IDC_USERID),GWL_STYLE,GetWindowLongPtr(GetDlgItem(hdlg,IDC_USERID),GWL_STYLE)|ES_NUMBER);
							if (strstr(acs->szProto, "GG") || strstr(acs->szProto, "MYSPACE"))
								_ultoa(INT_MAX, buffer, 10);
							else
								_ultoa(ULONG_MAX, buffer, 10);
							SendDlgItemMessage(hdlg, IDC_USERID, EM_LIMITTEXT, (WPARAM)strlen(buffer), 0);
						}
						else {
							SetWindowLongPtr(GetDlgItem(hdlg,IDC_USERID),GWL_STYLE,GetWindowLongPtr(GetDlgItem(hdlg,IDC_USERID),GWL_STYLE)&~ES_NUMBER);
							SendDlgItemMessage(hdlg, IDC_USERID, EM_LIMITTEXT, 255, 0);
						}
					}


				}
				break;
			}
		break;
		case IDC_AUTH:
			{
				DWORD flags = CallProtoService(acs->szProto,PS_GETCAPS,PFLAGNUM_4,0);
				if (flags & PF4_NOCUSTOMAUTH) {
					EnableWindow(GetDlgItem(hdlg,IDC_AUTHREQ),FALSE);
					EnableWindow(GetDlgItem(hdlg,IDC_AUTHGB),FALSE);
				}
				else {
					EnableWindow(GetDlgItem(hdlg,IDC_AUTHREQ),IsDlgButtonChecked(hdlg,IDC_AUTH));
					EnableWindow(GetDlgItem(hdlg,IDC_AUTHGB),IsDlgButtonChecked(hdlg,IDC_AUTH));
				}
			}
			break;
		case IDOK:
			{
				HANDLE hcontact = INVALID_HANDLE_VALUE;
				TCHAR szUserId[256];

				GetDlgItemText(hdlg, IDC_USERID, szUserId, SIZEOF(szUserId));

				if (strstr(acs->szProto, "GG")) { //Gadu-Gadu
					GGSEARCHRESULT *ggpsr;

					if ( _tcstoul(szUserId, NULL, 10) > INT_MAX ) {
						MessageBox( NULL,
							TranslateT("The contact cannot be added to your contact list. Make sure the User ID is entered properly."),
							TranslateT("Add Contact"), MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST);
						break;
					}

					ggpsr = (GGSEARCHRESULT *)mir_alloc(sizeof(GGSEARCHRESULT));
					ggpsr->hdr.cbSize = sizeof(GGSEARCHRESULT);
					ggpsr->uin = _tcstoul(szUserId, NULL, 10);

					acs->psr = (PROTOSEARCHRESULT *)mir_alloc(ggpsr->hdr.cbSize);
					memmove(acs->psr, &ggpsr->hdr, ggpsr->hdr.cbSize);
					mir_free(ggpsr);
				}
				else if (strstr(acs->szProto, "ICQ")) {	//ICQ
					ICQSEARCHRESULT *icqpsr;

					icqpsr = (ICQSEARCHRESULT *)mir_alloc(sizeof(ICQSEARCHRESULT));
					icqpsr->hdr.cbSize = sizeof(ICQSEARCHRESULT);
					icqpsr->uin = _tcstoul(szUserId, NULL, 10);

					acs->psr = (PROTOSEARCHRESULT *)mir_alloc(icqpsr->hdr.cbSize);
					memmove(acs->psr, &icqpsr->hdr, icqpsr->hdr.cbSize);
					mir_free(icqpsr);
				}
				else if (strstr(acs->szProto, "JABBER") || strstr(acs->szProto, "GMAIL")) { //Jabber/JGMal
					struct JABBER_SEARCH_RESULT *jpsr;

					jpsr = (struct JABBER_SEARCH_RESULT *)mir_alloc(sizeof(struct JABBER_SEARCH_RESULT));
					jpsr->hdr.cbSize = sizeof(struct JABBER_SEARCH_RESULT);
					_tcscpy(jpsr->jid, szUserId);

					acs->psr = (PROTOSEARCHRESULT *)mir_alloc(jpsr->hdr.cbSize);
					memmove(acs->psr, &jpsr->hdr, jpsr->hdr.cbSize);
					mir_free(jpsr);
				}
				else if (strstr(acs->szProto, "TLEN")) { //Tlen
					TLEN_SEARCH_RESULT *jpsr;

					jpsr = (TLEN_SEARCH_RESULT *)mir_alloc(sizeof(TLEN_SEARCH_RESULT));
					jpsr->hdr.cbSize = sizeof(TLEN_SEARCH_RESULT);
					#ifdef _UNICODE
						WideCharToMultiByte(CP_ACP, 0, szUserId, -1, jpsr->jid, 256, NULL, NULL);
					#else
						strcpy(jpsr->jid, szUserId);
					#endif

					acs->psr = (PROTOSEARCHRESULT *)mir_alloc(jpsr->hdr.cbSize);
					memmove(acs->psr, &jpsr->hdr, jpsr->hdr.cbSize);
					mir_free(jpsr);
				}
				else if (strstr(acs->szProto, "MSN")) { //MSN
					PROTOSEARCHRESULT *ppsr;

					ppsr = (PROTOSEARCHRESULT *)mir_alloc(sizeof(PROTOSEARCHRESULT));
					ppsr->cbSize = sizeof(PROTOSEARCHRESULT);
					ppsr->email = mir_t2a(szUserId);

					acs->psr = (PROTOSEARCHRESULT *)mir_alloc(ppsr->cbSize);
					memmove(acs->psr, ppsr, ppsr->cbSize);
					mir_free(ppsr);
				}
				else if (strstr(acs->szProto, "MNETSEND")) { //mNetSend
					NETSENDSEARCHRESULT *nspsr;

					nspsr = (NETSENDSEARCHRESULT *)mir_alloc(sizeof(NETSENDSEARCHRESULT));
					nspsr->hdr.cbSize = sizeof(NETSENDSEARCHRESULT);
					nspsr->dwIP = 0;

					acs->psr = (PROTOSEARCHRESULT *)mir_alloc(nspsr->hdr.cbSize);
					memmove(acs->psr, &nspsr->hdr, nspsr->hdr.cbSize);
					mir_free(nspsr);
				}
				else if (strstr(acs->szProto, "MYSPACE")) { //MySpace
					MYPROTOSEARCHRESULT *mypsr;

					if ( _tcstoul(szUserId, NULL, 10) > INT_MAX ) {
						MessageBox( NULL,
							TranslateT("The contact cannot be added to your contact list. Make sure the User ID is entered properly."),
							TranslateT("Add Contact"), MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST);
						break;
					}

					mypsr = (MYPROTOSEARCHRESULT *)mir_alloc(sizeof(MYPROTOSEARCHRESULT));
					mypsr->psr.cbSize = sizeof(MYPROTOSEARCHRESULT);
					mypsr->uid = _tcstoul(szUserId, NULL, 10);

					acs->psr = (PROTOSEARCHRESULT *)mir_alloc(mypsr->psr.cbSize);
					memmove(acs->psr, &mypsr->psr, mypsr->psr.cbSize);
					mir_free(mypsr);
				}
				else { //IRC, Yahoo, ...
					PROTOSEARCHRESULT *ppsr;

					ppsr = (PROTOSEARCHRESULT *)mir_alloc(sizeof(PROTOSEARCHRESULT));
					ppsr->cbSize = sizeof(PROTOSEARCHRESULT);

					acs->psr = (PROTOSEARCHRESULT *)mir_alloc(ppsr->cbSize);
					memmove(acs->psr, ppsr, ppsr->cbSize);
					mir_free(ppsr);
				}	

				if ( acs->psr ) {
					acs->psr->nick = mir_t2a(szUserId);
					acs->psr->firstName = mir_strdup("");
					acs->psr->lastName = mir_strdup("");
					if (!strstr(acs->szProto, "MSN"))
						acs->psr->email = mir_strdup("");
				}

				hcontact = (HANDLE)CallProtoService(acs->szProto,PS_ADDTOLIST,0,(LPARAM)acs->psr);

				if ( hcontact == NULL ) {
					MessageBox( NULL,
						TranslateT("The contact cannot be added to your contact list. If you are not connected to that network, try to connect. Also, make sure the User ID is entered properly."),
						TranslateT("Add Contact"), MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST);
					break;
				}

				{	TCHAR szHandle[256]; int item;
					if ( GetDlgItemText( hdlg, IDC_MYHANDLE, szHandle, SIZEOF(szHandle)))
						DBWriteContactSettingTString( hcontact, "CList", "MyHandle", szHandle );

					item = SendDlgItemMessage(hdlg, IDC_GROUP, CB_GETCURSEL, 0, 0);
					if (item > 0) {
						item = SendDlgItemMessage(hdlg, IDC_GROUP, CB_GETITEMDATA, item, 0);
						CallService(MS_CLIST_CONTACTCHANGEGROUP, (WPARAM)hcontact, item);
					}
				}

				if ( IsDlgButtonChecked( hdlg, IDC_ADDED ))
					CallContactService( hcontact, PSS_ADDED, 0, 0 );

				if ( IsDlgButtonChecked( hdlg, IDC_AUTH )) {
					DWORD flags = CallProtoService( acs->szProto, PS_GETCAPS, PFLAGNUM_4, 0 );
					if ( flags & PF4_NOCUSTOMAUTH )
						CallContactService( hcontact, PSS_AUTHREQUEST, 0, (LPARAM)"" );
					else {
						char szReason[256];
						GetDlgItemTextA(hdlg,IDC_AUTHREQ,szReason,256);
						CallContactService(hcontact,PSS_AUTHREQUEST,0,(LPARAM)szReason);
				}	}

				DBDeleteContactSetting(hcontact,"CList","NotOnList");
			}
			// fall through
		case IDCANCEL:
			if ( GetParent( hdlg ) == NULL)
				DestroyWindow( hdlg );
			else
				EndDialog( hdlg, 0 );
			break;
		}
		break;

	case WM_CLOSE:
		/* if there is no parent for the dialog, its a modeless dialog and can't be killed using EndDialog() */
		if ( GetParent( hdlg ) == NULL )
			DestroyWindow(hdlg);
		else
			EndDialog( hdlg, 0 );
		break;

	case DM_ADDCONTACT_CHANGEICONS:
		CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD)), 0);
		CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)CallService(MS_SKIN2_GETICON, 0, (LPARAM)ICON_ADD)), 0);
		break;

	case DM_ADDCONTACT_CHANGEACCLIST:
		{
			PROTOACCOUNT **accounts;
			int proto_count, ProtoCount = 0, id, i;
			DWORD caps;

			acs=(ADDCONTACTSTRUCT *)GetWindowLongPtr(hdlg,GWLP_USERDATA);
			SendDlgItemMessage(hdlg,IDC_PROTO,CB_RESETCONTENT,0,0);
			ProtoEnumAccounts(&proto_count, &accounts);
			for(i=0; i<proto_count; i++) {
				if (!IsAccountEnabled(accounts[i])) continue;
				caps=(DWORD)CallProtoService(accounts[i]->szModuleName,PS_GETCAPS,PFLAGNUM_1,0);
				if (!(caps&PF1_BASICSEARCH) && !(caps&PF1_EXTSEARCH) && !(caps&PF1_SEARCHBYEMAIL) && !(caps&PF1_SEARCHBYNAME))
					continue;

				ProtoCount++;
				id = SendDlgItemMessage(hdlg,IDC_PROTO,CB_ADDSTRING,0,(LPARAM)accounts[i]->tszAccountName);
				SendDlgItemMessage(hdlg,IDC_PROTO,CB_SETITEMDATA ,(WPARAM)id,(LPARAM)i);
			}

			if (!ProtoCount) {
				if ( GetParent( hdlg ) == NULL )
					DestroyWindow(hdlg);
				else
					EndDialog( hdlg, 0 );
				break;
			}
			SendDlgItemMessage(hdlg,IDC_PROTO,CB_SETCURSEL,0,0);
			SendMessage(hdlg, WM_COMMAND, MAKEWPARAM(IDC_PROTO, CBN_SELCHANGE), (LPARAM)GetDlgItem(hdlg, IDC_PROTO));

			i = SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETITEMDATA, (WPARAM)SendMessage(GetDlgItem(hdlg, IDC_PROTO), CB_GETCURSEL, 0, 0), 0);
			acs->szProto = accounts[i]->szModuleName;
		}
		break;

	case WM_DESTROY:
		CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)NULL), 0);
		CallService(MS_SKIN2_RELEASEICON, (WPARAM)SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)NULL), 0);
		acs = ( ADDCONTACTSTRUCT* )GetWindowLongPtr(hdlg,GWLP_USERDATA);
		if (acs) {
			if (acs->psr) {
				if (acs->psr->nick)
					mir_free(acs->psr->nick);
				if (acs->psr->firstName)
					mir_free(acs->psr->firstName);
				if (acs->psr->lastName)
					mir_free(acs->psr->lastName);
				if (acs->psr->email)
					mir_free(acs->psr->email);
				mir_free(acs->psr);
			}
			mir_free(acs);
		}
		break;
	}

	return FALSE;
}
