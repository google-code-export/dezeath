/*

Simple Status Message plugin for Miranda IM
Copyright (C) 2006-2011 Bartosz 'Dezeath' Bia�ek, (C) 2005 Harven

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
#include "commonheaders.h"
#include "simplestatusmsg.h"
#include <io.h>

HINSTANCE g_hInst;
PLUGINLINK *pluginLink;
int hLangpack;
struct MM_INTERFACE mmi;
PROTOACCOUNTS *accounts;

static int g_iIdleTime = -1;
UINT_PTR g_uUpdateMsgTimer = 0, *g_uSetStatusTimer;
static TCHAR *g_ptszWinampSong;
HANDLE hTTBButton = 0, h_statusmodechange;
HWND hwndSAMsgDialog;
static HANDLE *hProtoStatusMenuItem;

PLUGININFOEX pluginInfo = {
	sizeof(PLUGININFOEX),
#if defined(_WIN64)
	"Simple Status Message (x64)",
#elif defined(_UNICODE)
	"Simple Status Message (Unicode)",
#else
	"Simple Status Message (ANSI)",
#endif
	PLUGIN_MAKE_VERSION(1, 9, 0, 4),
	"Provides a simple way to set status and away messages",
	"Bartosz 'Dezeath' Bia�ek, Harven",
	"dezred"/*antispam*/"@"/*antispam*/"gmail"/*antispam*/"."/*antispam*/"com",
	"� 2006-2011 Bartosz Bia�ek, � 2005 Harven",
	"http://code.google.com/p/dezeath",
	UNICODE_AWARE,
	DEFMOD_SRAWAY,
#ifdef _UNICODE
	// {768CE156-34AC-45a3-B53B-0083C47615C4}
	{ 0x768ce156, 0x34ac, 0x45a3, { 0xb5, 0x3b, 0x0, 0x83, 0xc4, 0x76, 0x15, 0xc4 } }
#else
	// {7D548A69-05E7-4d00-89BC-ACCE781022C1}
	{ 0x7d548a69, 0x5e7, 0x4d00, { 0x89, 0xbc, 0xac, 0xce, 0x78, 0x10, 0x22, 0xc1 } }
#endif
};

static const MUUID interfaces[] = {MIID_SRAWAY, MIID_LAST};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	g_hInst = hinstDLL;
	return TRUE;
}

extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD mirandaVersion)
{
	if (mirandaVersion < PLUGIN_MAKE_VERSION(0, 9, 0, 0))
	{
		MessageBox(NULL, _T("The Simple Status Message plugin cannot be loaded. It requires Miranda IM 0.9.0 or later."), _T("Simple Status Message Plugin"), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
		return NULL;
	}
	return &pluginInfo;
}

extern "C" __declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}

#ifdef _DEBUG
void log2file(const char *fmt, ...)
{
	DWORD dwBytesWritten;
	va_list	va;
	char szText[1024];
	HANDLE hFile = CreateFileA("simplestatusmsg.log", GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	SetFilePointer(hFile, 0, 0, FILE_END);

	strncpy(szText, "[\0", SIZEOF(szText));
	WriteFile(hFile, szText, (DWORD)strlen(szText), &dwBytesWritten, NULL);

	GetTimeFormatA(LOCALE_USER_DEFAULT, 0, NULL, NULL, szText, SIZEOF(szText));
	WriteFile(hFile, szText, (DWORD)strlen(szText), &dwBytesWritten, NULL);

	strncpy(szText, "] \0", SIZEOF(szText));

	va_start(va, fmt);
	mir_vsnprintf(szText + strlen(szText), SIZEOF(szText) - strlen(szText), fmt, va);
	va_end(va);

	WriteFile(hFile, szText, (DWORD)strlen(szText), &dwBytesWritten, NULL);

	strncpy(szText, "\n\0", SIZEOF(szText));
	WriteFile(hFile, szText, (DWORD)strlen(szText), &dwBytesWritten, NULL);

	CloseHandle(hFile);
}
#endif

static TCHAR *GetWinampSong(void)
{
	TCHAR *szTitle, *pstr, *res = NULL;
	HWND hwndWinamp = FindWindow(_T("STUDIO"), NULL);
	int iTitleLen;

	if (hwndWinamp == NULL)
		hwndWinamp = FindWindow(_T("Winamp v1.x"), NULL);

	if (hwndWinamp == NULL)
		return NULL;

	iTitleLen = GetWindowTextLength(hwndWinamp);
	szTitle = (TCHAR *)mir_alloc((iTitleLen + 1) * sizeof(TCHAR));
	if (szTitle == NULL)
		return NULL;

	if (GetWindowText(hwndWinamp, szTitle, iTitleLen + 1) == 0)
	{
		mir_free(szTitle);
		return NULL;
	}

	pstr = _tcsstr(szTitle, _T(" - Winamp"));
	if (pstr == NULL)
	{
		mir_free(szTitle);
		return NULL;
	}

	if (pstr < szTitle + (iTitleLen / 2))
	{
		MoveMemory(szTitle, pstr + 9, _tcslen(pstr + 9) * sizeof(TCHAR));
		pstr = _tcsstr(pstr + 1, _T(" - Winamp"));
		if (pstr == NULL)
		{
			mir_free(szTitle);
			return NULL;
		}
	}
	*pstr = 0;

	pstr = _tcschr(szTitle, _T('.'));
	if (pstr == NULL)
	{
		mir_free(szTitle);
		return NULL;
	}

	pstr += 2;
	res = mir_tstrdup(pstr);
	mir_free(szTitle);

	return res;
}

TCHAR *InsertBuiltinVarsIntoMsg(TCHAR *in, const char *szProto, int status)
{
	int i, count = 0, len;
	TCHAR substituteStr[1024], *msg = mir_tstrdup(in);

	for (i = 0; msg[i]; i++)
	{
		if (msg[i] == 0x0D && DBGetContactSettingByte(NULL, "SimpleStatusMsg", "RemoveCR", 0))
		{
			TCHAR *p = msg + i;
			if (i + 1 <= 1024 && msg[i + 1])
			{
				if (msg[i + 1] == 0x0A)
				{
					if (i + 2 <= 1024 && msg[i + 2])
					{
						count++;
						MoveMemory(p, p + 1, (lstrlen(p) - 1) * sizeof(TCHAR));
					}
					else
					{
						msg[i + 1] = 0;
						msg[i] = 0x0A;
					}
				}
			}
		}

		if (msg[i] != '%')
			continue;

		if (!_tcsnicmp(msg+i, _T("%winampsong%"), 12))
		{
			TCHAR *ptszWinampTitle = GetWinampSong();

			if (ptszWinampTitle != NULL)
			{
				mir_free(g_ptszWinampSong);
				g_ptszWinampSong = mir_tstrdup(ptszWinampTitle);
			}
			else if (g_ptszWinampSong && lstrcmp(g_ptszWinampSong, _T("SimpleStatusMsg"))
				&& DBGetContactSettingByte(NULL, "SimpleStatusMsg", "AmpLeaveTitle", 1))
			{
				ptszWinampTitle = mir_tstrdup(g_ptszWinampSong);
			}
			else
				continue;

			if (lstrlen(ptszWinampTitle) > 12)
				msg = (TCHAR *)mir_realloc(msg, (lstrlen(msg) + 1 + lstrlen(ptszWinampTitle) - 12) * sizeof(TCHAR));

			MoveMemory(msg + i + lstrlen(ptszWinampTitle), msg + i + 12, (lstrlen(msg) - i - 11) * sizeof(TCHAR));
			CopyMemory(msg + i, ptszWinampTitle, lstrlen(ptszWinampTitle) * sizeof(TCHAR));

			mir_free(ptszWinampTitle);
		}
		else if (!_tcsnicmp(msg+i, _T("%fortunemsg%"), 12))
		{
			TCHAR *FortuneMsg;
#ifdef _UNICODE
			char *FortuneMsgA;
#endif
			
			if (!ServiceExists(MS_FORTUNEMSG_GETMESSAGE))
				continue;

#ifdef _UNICODE
			FortuneMsgA = (char*)CallService(MS_FORTUNEMSG_GETMESSAGE, 0, 0);
			FortuneMsg = mir_a2u(FortuneMsgA);
#else
			FortuneMsg = (char*)CallService(MS_FORTUNEMSG_GETMESSAGE, 0, 0);
#endif

			if (lstrlen(FortuneMsg) > 12)
				msg = (TCHAR *)mir_realloc(msg, (lstrlen(msg) + 1 + lstrlen(FortuneMsg) - 12) * sizeof(TCHAR));

			MoveMemory(msg + i + lstrlen(FortuneMsg), msg + i + 12, (lstrlen(msg) - i - 11) * sizeof(TCHAR));
			CopyMemory(msg + i, FortuneMsg, lstrlen(FortuneMsg) * sizeof(TCHAR));
			
#ifdef _UNICODE
			mir_free(FortuneMsg);
			CallService(MS_FORTUNEMSG_FREEMEMORY, 0, (LPARAM)FortuneMsgA);
#else
			CallService(MS_FORTUNEMSG_FREEMEMORY, 0, (LPARAM)FortuneMsg);
#endif
		}
		else if (!_tcsnicmp(msg+i, _T("%protofortunemsg%"), 17))
		{
			TCHAR *FortuneMsg;
#ifdef _UNICODE
			char *FortuneMsgA;
#endif
			
			if (!ServiceExists(MS_FORTUNEMSG_GETPROTOMSG))
				continue;

#ifdef _UNICODE
			FortuneMsgA = (char*)CallService(MS_FORTUNEMSG_GETPROTOMSG, (WPARAM)szProto, 0);
			FortuneMsg = mir_a2u(FortuneMsgA);
#else
			FortuneMsg = (char*)CallService(MS_FORTUNEMSG_GETPROTOMSG, (WPARAM)szProto, 0);
#endif

			if (lstrlen(FortuneMsg) > 17)
				msg = (TCHAR *)mir_realloc(msg, (lstrlen(msg) + 1 + lstrlen(FortuneMsg) - 17) * sizeof(TCHAR));

			MoveMemory(msg + i + lstrlen(FortuneMsg), msg + i + 17, (lstrlen(msg) - i - 16) * sizeof(TCHAR));
			CopyMemory(msg + i, FortuneMsg, lstrlen(FortuneMsg) * sizeof(TCHAR));
			
#ifdef _UNICODE
			mir_free(FortuneMsg);
			CallService(MS_FORTUNEMSG_FREEMEMORY, 0, (LPARAM)FortuneMsgA);
#else
			CallService(MS_FORTUNEMSG_FREEMEMORY, 0, (LPARAM)FortuneMsg);
#endif
		}
		else if (!_tcsnicmp(msg+i, _T("%statusfortunemsg%"), 18))
		{
			TCHAR *FortuneMsg;
#ifdef _UNICODE
			char *FortuneMsgA;
#endif
			
			if (!ServiceExists(MS_FORTUNEMSG_GETSTATUSMSG))
				continue;

#ifdef _UNICODE
			FortuneMsgA = (char*)CallService(MS_FORTUNEMSG_GETSTATUSMSG, (WPARAM)status, 0);
			FortuneMsg = mir_a2u(FortuneMsgA);
#else
			FortuneMsg = (char*)CallService(MS_FORTUNEMSG_GETSTATUSMSG, (WPARAM)status, 0);
#endif

			if (lstrlen(FortuneMsg) > 18)
				msg = (TCHAR *)mir_realloc(msg, (lstrlen(msg) + 1 + lstrlen(FortuneMsg) - 18) * sizeof(TCHAR));

			MoveMemory(msg + i + lstrlen(FortuneMsg), msg + i + 18, (lstrlen(msg) - i - 17) * sizeof(TCHAR));
			CopyMemory(msg + i, FortuneMsg, lstrlen(FortuneMsg) * sizeof(TCHAR));
			
#ifdef _UNICODE
			mir_free(FortuneMsg);
			CallService(MS_FORTUNEMSG_FREEMEMORY, 0, (LPARAM)FortuneMsgA);
#else
			CallService(MS_FORTUNEMSG_FREEMEMORY, 0, (LPARAM)FortuneMsg);
#endif
		}
		else if (!_tcsnicmp(msg + i, _T("%time%"), 6))
		{
			MIRANDA_IDLE_INFO mii = {0};
			mii.cbSize = sizeof(mii);
			CallService(MS_IDLE_GETIDLEINFO, 0, (LPARAM)&mii);

			if (mii.idleType)
			{
				int mm;
				SYSTEMTIME t;
				GetLocalTime(&t);
				if ((mm = g_iIdleTime) == -1)
				{
					mm = t.wMinute + t.wHour * 60;
					if (mii.idleType == 1)
					{
						mm -= mii.idleTime;
						if (mm < 0) mm += 60 * 24;
					}
					g_iIdleTime = mm;
				}
				t.wMinute = mm % 60;
				t.wHour = mm / 60;
				GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &t, NULL, substituteStr, SIZEOF(substituteStr));
			}
			else GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, NULL, NULL, substituteStr, SIZEOF(substituteStr));

			if (lstrlen(substituteStr) > 6)
				msg = (TCHAR *)mir_realloc(msg, (lstrlen(msg) + 1 + lstrlen(substituteStr) - 6) * sizeof(TCHAR));

			MoveMemory(msg + i + lstrlen(substituteStr), msg + i + 6, (lstrlen(msg) - i - 5) * sizeof(TCHAR));
			CopyMemory(msg + i, substituteStr, lstrlen(substituteStr) * sizeof(TCHAR));
		}
		else if (!_tcsnicmp(msg + i, _T("%date%"), 6))
		{
			GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, NULL, NULL, substituteStr, SIZEOF(substituteStr));

			if (lstrlen(substituteStr) > 6)
				msg = (TCHAR *)mir_realloc(msg, (lstrlen(msg) + 1 + lstrlen(substituteStr) - 6) * sizeof(TCHAR));

			MoveMemory(msg + i + lstrlen(substituteStr), msg + i + 6, (lstrlen(msg) - i - 5) * sizeof(TCHAR));
			CopyMemory(msg + i, substituteStr, lstrlen(substituteStr) * sizeof(TCHAR));
		}
		else if (!_tcsnicmp(msg+i, _T("%rand("), 6))
		{
			TCHAR *temp, *token;
			int ran_from, ran_to, k;

			temp = mir_tstrdup(msg + i + 6);
			token = _tcstok(temp, _T(",)"));
			ran_from = _ttoi(token);
			token = _tcstok(NULL, _T(",)%%"));
			ran_to = _ttoi(token);

			if (ran_to > ran_from)
			{
				mir_sntprintf(substituteStr, SIZEOF(substituteStr), _T("%d"), GetRandom(ran_from, ran_to));
				for (k = i + 1; msg[k]; k++) if (msg[k] == '%') { k++; break; }

				if (lstrlen(substituteStr) > k - i)
					msg = (TCHAR *)mir_realloc(msg, (lstrlen(msg) + 1 + lstrlen(substituteStr) - (k - i)) * sizeof(TCHAR));

				MoveMemory(msg + i + lstrlen(substituteStr), msg + i + (k - i), (lstrlen(msg) - i - (k - i - 1)) * sizeof(TCHAR));
				CopyMemory(msg + i, substituteStr, lstrlen(substituteStr) * sizeof(TCHAR));
			}
			mir_free(temp);
		}
		else if (!_tcsnicmp(msg+i, _T("%randmsg%"), 9))
		{
			char buff[16];
			int k, maxk, k2 = 0;
			DBVARIANT dbv;
			BOOL rmark[25];

			for (k = 0; k < 26; k++) rmark[k] = FALSE;
			maxk = DBGetContactSettingByte(NULL, "SimpleStatusMsg", "MaxHist", 10);
			if (maxk == 0) rmark[0] = TRUE;

			while (!rmark[0])
			{
				k = GetRandom(1, maxk);
				if (rmark[k]) continue;
				rmark[k] = TRUE;
				k2++;
				if (k2 == maxk || k2 > maxk) rmark[0] = TRUE;

				mir_snprintf(buff, SIZEOF(buff), "SMsg%d", k);
				if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", buff, &dbv))
				{
					if (dbv.ptszVal == NULL)
					{
						DBFreeVariant(&dbv);
						continue;
					}
					lstrcpy(substituteStr, dbv.ptszVal);
					DBFreeVariant(&dbv);
				}
				else continue;

				if (!lstrlen(substituteStr)) continue;
				if (_tcsstr(substituteStr, _T("%randmsg%")) != NULL || _tcsstr(substituteStr, _T("%randdefmsg%")) != NULL)
				{
					if (k == maxk) maxk--;
				}
				else rmark[0] = TRUE;
			}

			if (k2 == maxk || k2 > maxk) lstrcpy(substituteStr, _T(""));

			if (lstrlen(substituteStr) > 9)
				msg = (TCHAR *)mir_realloc(msg, (lstrlen(msg) + 1 + lstrlen(substituteStr) - 9) * sizeof(TCHAR));

			MoveMemory(msg + i + lstrlen(substituteStr), msg + i + 9, (lstrlen(msg) - i - 8) * sizeof(TCHAR));
			CopyMemory(msg + i, substituteStr, lstrlen(substituteStr) * sizeof(TCHAR));
		}
		else if (!_tcsnicmp(msg+i, _T("%randdefmsg%"), 12))
		{
			char buff[16];
			int k, maxk, k2 = 0;
			DBVARIANT dbv;
			BOOL rmark[25];

			for (k = 0; k < 26; k++) rmark[k] = FALSE;
			maxk = DBGetContactSettingWord(NULL, "SimpleStatusMsg", "DefMsgCount", 0);
			if (maxk == 0) rmark[0] = TRUE;

			while (!rmark[0])
			{
				k = GetRandom(1, maxk);
				if (rmark[k]) continue;
				rmark[k] = TRUE;
				k2++;
				if (k2 == maxk || k2 > maxk) rmark[0] = TRUE;

				mir_snprintf(buff, SIZEOF(buff), "DefMsg%d", k);
				if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", buff, &dbv))
				{
					if (dbv.ptszVal == NULL)
					{
						DBFreeVariant(&dbv);
						continue;
					}
					lstrcpy(substituteStr, dbv.ptszVal);
					DBFreeVariant(&dbv);
				}
				else continue;

				if (!lstrlen(substituteStr)) continue;
				if (_tcsstr(substituteStr, _T("%randmsg%")) != NULL || _tcsstr(substituteStr, _T("%randdefmsg%")) != NULL)
				{
					if (k == maxk) maxk--;
				}
				else rmark[0] = TRUE;
			}

			if (k2 == maxk || k2 > maxk) lstrcpy(substituteStr, _T(""));

			if (lstrlen(substituteStr) > 12)
				msg = (TCHAR *)mir_realloc(msg, (lstrlen(msg)+1+lstrlen(substituteStr)-12) * sizeof(TCHAR));

			MoveMemory(msg + i + lstrlen(substituteStr), msg + i + 12, (lstrlen(msg) - i - 11) * sizeof(TCHAR));
			CopyMemory(msg + i, substituteStr, lstrlen(substituteStr) * sizeof(TCHAR));
		}
	}

	if (count) msg[lstrlen(msg) - count] = 0;

	if (szProto)
	{
		char szSetting[80];
		mir_snprintf(szSetting, SIZEOF(szSetting), "Proto%sMaxLen", szProto);
		len = DBGetContactSettingWord(NULL, "SimpleStatusMsg", szSetting, 1024);
		if (len < lstrlen(msg))
		{
			msg = (TCHAR *)mir_realloc(msg, len * sizeof(TCHAR));
			msg[len] = 0;
		}
	}

	return msg;
}

TCHAR *InsertVarsIntoMsg(TCHAR *tszMsg, const char *szProto, int iStatus, HANDLE hContact)
{
	if (ServiceExists(MS_VARS_FORMATSTRING) && DBGetContactSettingByte(NULL, "SimpleStatusMsg", "EnableVariables", 1))
	{
		FORMATINFO fInfo = {0};
		fInfo.cbSize = sizeof(fInfo);
		fInfo.flags = FIF_TCHAR;
		fInfo.tszFormat = tszMsg;
		fInfo.hContact = hContact;
		TCHAR *tszVarsMsg = (TCHAR *)CallService(MS_VARS_FORMATSTRING, (WPARAM)&fInfo, 0);
		if (tszVarsMsg != NULL)
		{
			TCHAR *format = InsertBuiltinVarsIntoMsg(tszVarsMsg, szProto, iStatus);
			CallService(MS_VARS_FREEMEMORY, (WPARAM)tszVarsMsg, 0);
			return format;
		}
	}

	return InsertBuiltinVarsIntoMsg(tszMsg, szProto, iStatus);
}

static TCHAR *GetAwayMessageFormat(int iStatus, const char *szProto)
{
	DBVARIANT dbv, dbv2;
	int flags;
	char szSetting[80];
	TCHAR *format;

	mir_snprintf(szSetting, SIZEOF(szSetting), "%sFlags", szProto ? szProto : "");
	flags = DBGetContactSettingByte(NULL, "SimpleStatusMsg", (char *)StatusModeToDbSetting(iStatus, szSetting), STATUS_DEFAULT);

	if (flags & STATUS_EMPTY_MSG)
		return mir_tstrdup(_T(""));

	if (flags & STATUS_LAST_STATUS_MSG)
	{
		if (szProto)
			mir_snprintf(szSetting, SIZEOF(szSetting), "%sMsg", szProto);
		else
			mir_snprintf(szSetting, SIZEOF(szSetting), "Msg");

		if (DBGetContactSettingTString(NULL, "SRAway", StatusModeToDbSetting(iStatus, szSetting), &dbv))
			return NULL; //mir_tstrdup(_T(""));

		format = mir_tstrdup(dbv.ptszVal);
		DBFreeVariant(&dbv);
	}
	else if (flags & STATUS_LAST_MSG)
	{
		if (szProto)
			mir_snprintf(szSetting, SIZEOF(szSetting), "Last%sMsg", szProto);
		else
			mir_snprintf(szSetting, SIZEOF(szSetting), "LastMsg");

		if (DBGetContactSetting(NULL, "SimpleStatusMsg", szSetting, &dbv2))
			return NULL; //mir_tstrdup(_T(""));

		if (DBGetContactSettingTString(NULL, "SimpleStatusMsg", dbv2.pszVal, &dbv))
		{
			DBFreeVariant(&dbv2);
			return NULL; //mir_tstrdup(_T(""));
		}

		format = mir_tstrdup(dbv.ptszVal);
		DBFreeVariant(&dbv);
		DBFreeVariant(&dbv2);
	}
	else if (flags & STATUS_THIS_MSG)
	{
		if (szProto)
			mir_snprintf(szSetting, SIZEOF(szSetting), "%sDefault", szProto);
		else
			mir_snprintf(szSetting, SIZEOF(szSetting), "Default");

		if (DBGetContactSettingTString(NULL, "SRAway", StatusModeToDbSetting(iStatus, szSetting), &dbv))
			return mir_tstrdup(_T(""));

		format = mir_tstrdup(dbv.ptszVal);
		DBFreeVariant(&dbv);
	}
	else
		format = mir_tstrdup(GetDefaultMessage(iStatus));

	return format;
}

void DBWriteMessage(char *szSetting, TCHAR *tszMsg)
{
	if (tszMsg && lstrlen(tszMsg))
		DBWriteContactSettingTString(NULL, "SimpleStatusMsg", szSetting, tszMsg);
	else
		DBDeleteContactSetting(NULL, "SimpleStatusMsg", szSetting);
}

void SaveMessageToDB(const char *szProto, TCHAR *tszMsg, BOOL bIsFormat)
{
	char szSetting[80];
		
	if (!szProto)
	{
		for (int i = 0; i < accounts->count; ++i)
		{
			if (!IsAccountEnabled(accounts->pa[i]))
				continue;

			if (!CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0))
				continue;
			
			if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
				continue;

			mir_snprintf(szSetting, SIZEOF(szSetting), bIsFormat ? "FCur%sMsg" : "Cur%sMsg", accounts->pa[i]->szModuleName);
			DBWriteMessage(szSetting, tszMsg);
#ifdef _DEBUG
			if (bIsFormat)
				log2file("SaveMessageToDB(): Set \"" TCHAR_STR_PARAM "\" status message (without inserted vars) for %s.", tszMsg, accounts->pa[i]->szModuleName);
			else
				log2file("SaveMessageToDB(): Set \"" TCHAR_STR_PARAM "\" status message for %s.", tszMsg, accounts->pa[i]->szModuleName);
#endif
		}
	}
	else
	{
		if (!(CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
			return;

		mir_snprintf(szSetting, SIZEOF(szSetting), bIsFormat ? "FCur%sMsg" : "Cur%sMsg", szProto);
		DBWriteMessage(szSetting, tszMsg);
#ifdef _DEBUG
		if (bIsFormat)
			log2file("SaveMessageToDB(): Set \"" TCHAR_STR_PARAM "\" status message (without inserted vars) for %s.", tszMsg, szProto);
		else
			log2file("SaveMessageToDB(): Set \"" TCHAR_STR_PARAM "\" status message for %s.", tszMsg, szProto);
#endif
	}
}

void SaveStatusAsCurrent(const char *szProto, int iStatus)
{
	char szSetting[80];
	mir_snprintf(szSetting, SIZEOF(szSetting), "Cur%sStatus", szProto);
	DBWriteContactSettingWord(NULL, "SimpleStatusMsg", szSetting, (WORD)iStatus);
}

static TCHAR *GetAwayMessage(int iStatus, const char *szProto, BOOL bInsertVars, HANDLE hContact)
{
	TCHAR *format = NULL;
	char szSetting[80];

	if ((!iStatus || iStatus == ID_STATUS_CURRENT) && szProto)
	{
		DBVARIANT dbv;
		mir_snprintf(szSetting, SIZEOF(szSetting), "FCur%sMsg", szProto);
		if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", szSetting, &dbv))
		{
			format = mir_tstrdup(dbv.ptszVal);
			DBFreeVariant(&dbv);
		}
		//else
		//	format = mir_tstrdup(_T(""));
	}
	else
	{
		int flags;

		if (!iStatus || iStatus == ID_STATUS_CURRENT)
			iStatus = GetCurrentStatus(szProto);

		if (szProto && !(CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_3, 0) & Proto_Status2Flag(iStatus)))
			return NULL;

		mir_snprintf(szSetting, SIZEOF(szSetting), "Proto%sFlags", szProto ? szProto : "");
		flags = DBGetContactSettingByte(NULL, "SimpleStatusMsg", szSetting, PROTO_DEFAULT);

		//if (flags & PROTO_NO_MSG)
		//{
		//	format = mir_tstrdup(_T(""));
		//}
		//else
		if (flags & PROTO_THIS_MSG)
		{
			DBVARIANT dbv;
			mir_snprintf(szSetting, SIZEOF(szSetting), "Proto%sDefault", szProto);
			if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", szSetting, &dbv))
			{
				format = mir_tstrdup(dbv.ptszVal);
				DBFreeVariant(&dbv);
			}
			else
				format = mir_tstrdup(_T(""));
		}
		else if (flags & PROTO_NOCHANGE && szProto)
		{
			DBVARIANT dbv;
			mir_snprintf(szSetting, SIZEOF(szSetting), "FCur%sMsg", szProto);
			if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", szSetting, &dbv))
			{
				format = mir_tstrdup(dbv.ptszVal);
				DBFreeVariant(&dbv);
			}
			//else
			//	format = mir_tstrdup(_T(""));
		}
		else if (flags & PROTO_POPUPDLG)
			format = GetAwayMessageFormat(iStatus, szProto);
	}
#ifdef _DEBUG
	log2file("GetAwayMessage(): %s has %s status and \"" TCHAR_STR_PARAM "\" status message.", szProto, StatusModeToDbSetting(iStatus, ""), format);
#endif

	if (bInsertVars && format != NULL)
	{
		TCHAR *tszVarsMsg = InsertVarsIntoMsg(format, szProto, iStatus, hContact); // TODO random values not the same!
		mir_free(format);
		return tszVarsMsg;
	}

	return format;
}

int CheckProtoSettings(const char *szProto, int iInitialStatus)
{
	int	iSetting = DBGetContactSettingWord(NULL, szProto, "LeaveStatus", -1); //GG settings
	if (iSetting != -1)
		return iSetting ? iSetting : iInitialStatus;
	iSetting = DBGetContactSettingWord(NULL, szProto, "OfflineMessageOption", -1); //TLEN settings
	if (iSetting != -1)
	{
		switch (iSetting)
		{
			case 1: return ID_STATUS_ONLINE;
			case 2: return ID_STATUS_AWAY;
			case 3: return ID_STATUS_NA;
			case 4: return ID_STATUS_DND;
			case 5: return ID_STATUS_FREECHAT;
			case 6: return ID_STATUS_INVISIBLE;
			default: return iInitialStatus;
		}
	}
	return iInitialStatus;
}

static void Proto_SetAwayMsgT(const char *szProto, int iStatus, TCHAR *tszMsg)
{
	if (!(CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_INDIVMODEMSG))
	{
#ifdef _UNICODE
		if (CallProtoService(szProto, PS_SETAWAYMSGW, (WPARAM)iStatus, (LPARAM)tszMsg) == CALLSERVICE_NOTFOUND)
		{
			char *szMsg = mir_u2a(tszMsg);
			CallProtoService(szProto, PS_SETAWAYMSG, (WPARAM)iStatus, (LPARAM)szMsg);
			mir_free(szMsg);
		}
#else
		CallProtoService(szProto, PS_SETAWAYMSG, (WPARAM)iStatus, (LPARAM)tszMsg);
#endif
	}
}

static void Proto_SetStatus(const char *szProto, int iInitialStatus, int iStatus, TCHAR *tszMsg)
{
	if (iStatus == ID_STATUS_OFFLINE && iStatus != iInitialStatus)
	{
		// ugly hack to set offline status message
		if (!(CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_INDIVMODEMSG))
		{
			int iMsgStatus = CheckProtoSettings(szProto, iInitialStatus);
#ifdef _UNICODE
			if (CallProtoService(szProto, PS_SETAWAYMSGW, (WPARAM)iMsgStatus, (LPARAM)tszMsg) == CALLSERVICE_NOTFOUND)
			{
				char *szMsg = mir_u2a(tszMsg);
				CallProtoService(szProto, PS_SETAWAYMSG, (WPARAM)iMsgStatus, (LPARAM)szMsg);
				mir_free(szMsg);
			}
#else
			CallProtoService(szProto, PS_SETAWAYMSG, (WPARAM)iMsgStatus, (LPARAM)tszMsg);
#endif
			CallProtoService(szProto, PS_SETSTATUS, (WPARAM)iMsgStatus, 0);
		}
		if (ServiceExists(MS_KS_ANNOUNCESTATUSCHANGE))
			announce_status_change((char*)szProto, ID_STATUS_OFFLINE, NULL);
		CallProtoService(szProto, PS_SETSTATUS, ID_STATUS_OFFLINE, 0);
		return;
	}

	Proto_SetAwayMsgT(szProto, iStatus, tszMsg /* ? tszMsg : _T("")*/);
	if (iStatus != iInitialStatus)
		CallProtoService(szProto, PS_SETSTATUS, iStatus, 0);
}

int HasProtoStaticStatusMsg(const char *szProto, int iInitialStatus, int iStatus)
{
	char szSetting[80];
	int flags;

	mir_snprintf(szSetting, SIZEOF(szSetting), "Proto%sFlags", szProto);
	flags = DBGetContactSettingByte(NULL, "SimpleStatusMsg", szSetting, PROTO_DEFAULT);

	if (flags & PROTO_NO_MSG)
	{
		Proto_SetStatus(szProto, iInitialStatus, iStatus, NULL);
		SaveMessageToDB(szProto, NULL, TRUE);
		SaveMessageToDB(szProto, NULL, FALSE);
		return 1;
	}
	else if (flags & PROTO_THIS_MSG)
	{
		DBVARIANT dbv;
		TCHAR *msg;

		mir_snprintf(szSetting, SIZEOF(szSetting), "Proto%sDefault", szProto);
		if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", szSetting, &dbv))
		{
			SaveMessageToDB(szProto, dbv.ptszVal, TRUE);
			msg = InsertVarsIntoMsg(dbv.ptszVal, szProto, iStatus, NULL);
			DBFreeVariant(&dbv);
			Proto_SetStatus(szProto, iInitialStatus, iStatus, msg);
			SaveMessageToDB(szProto, msg, FALSE);
			mir_free(msg);
		}
		else
		{
			Proto_SetStatus(szProto, iInitialStatus, iStatus, _T(""));
			SaveMessageToDB(szProto, _T(""), TRUE);
			SaveMessageToDB(szProto, _T(""), FALSE);
		}
		return 1;
	}
	return 0;
}

INT_PTR SetStatusModeFromExtern(WPARAM wParam, LPARAM lParam)
{
	if ((wParam < ID_STATUS_OFFLINE && wParam != 0) || (wParam > ID_STATUS_OUTTOLUNCH && wParam != ID_STATUS_CURRENT))
		return 0;

	int newStatus = (int)wParam;

	for (int i = 0; i < accounts->count; ++i)
	{
		if (!IsAccountEnabled(accounts->pa[i]))
			continue;

		if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0) &~ CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0)))
			continue;

		if (DBGetContactSettingByte(NULL, accounts->pa[i]->szModuleName, "LockMainStatus", 0))
			continue;
	
		if (wParam == ID_STATUS_CURRENT || wParam == 0)
			newStatus = GetCurrentStatus(accounts->pa[i]->szModuleName);

		if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
		{
			CallProtoService(accounts->pa[i]->szModuleName, PS_SETSTATUS, newStatus, 0);
			continue;
		}
			
		int status_modes_msg = CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0);

		if ((Proto_Status2Flag(newStatus) & status_modes_msg) || (newStatus == ID_STATUS_OFFLINE && (Proto_Status2Flag(ID_STATUS_INVISIBLE) & status_modes_msg)))
		{
			TCHAR *msg = NULL;

			if (HasProtoStaticStatusMsg(accounts->pa[i]->szModuleName, GetCurrentStatus(accounts->pa[i]->szModuleName), newStatus))
				continue;

			if (lParam)
				msg = InsertVarsIntoMsg((TCHAR *)lParam, accounts->pa[i]->szModuleName, newStatus, NULL);

			SaveMessageToDB(accounts->pa[i]->szModuleName, (TCHAR *)lParam, TRUE);
			SaveMessageToDB(accounts->pa[i]->szModuleName, msg, FALSE);
			Proto_SetStatus(accounts->pa[i]->szModuleName, GetCurrentStatus(accounts->pa[i]->szModuleName), newStatus, msg /*? msg : _T("")*/);
			mir_free(msg);
		}
		else
			CallProtoService(accounts->pa[i]->szModuleName, PS_SETSTATUS, newStatus, 0);
	}

	return 0;
}

int ChangeStatusMessage(WPARAM wParam, LPARAM lParam);

void SetStatusMessage(const char *szProto, int iInitialStatus, int iStatus, TCHAR *message, BOOL bOnStartup)
{
	TCHAR *msg = NULL;
#ifdef _DEBUG
	log2file("SetStatusMessage(\"%s\", %d, %d, \"" TCHAR_STR_PARAM "\", %d)", szProto, iInitialStatus, iStatus, message, bOnStartup);
#endif
	if (szProto)
	{
		if (bOnStartup && accounts->statusCount > 1) // TODO not only at startup?
		{
			int status;
			for (int i = 0; i < accounts->count; ++i)
			{
				if (!IsAccountEnabled(accounts->pa[i]))
					continue;

				if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0)))
					continue;

				status = iStatus == ID_STATUS_CURRENT ? GetStartupStatus(accounts->pa[i]->szModuleName) : iStatus;

				if (!CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0) ||
					!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
				{
					if (!(bOnStartup && status == ID_STATUS_OFFLINE) && GetCurrentStatus(accounts->pa[i]->szModuleName) != status)
						CallProtoService(accounts->pa[i]->szModuleName, PS_SETSTATUS, (WPARAM)status, 0);
				}
			}
		}

		if (message)
			msg = InsertVarsIntoMsg(message, szProto, iStatus, NULL);

		SaveMessageToDB(szProto, message, TRUE);
		SaveMessageToDB(szProto, msg, FALSE);

		if (iInitialStatus == ID_STATUS_CURRENT)
			iInitialStatus = bOnStartup ? ID_STATUS_OFFLINE : GetCurrentStatus(szProto);

		Proto_SetStatus(szProto, iInitialStatus, iStatus, msg);
		mir_free(msg);
	}
	else
	{
		int iProfileStatus = iStatus > ID_STATUS_CURRENT ? iStatus : 0;
		BOOL bIsStatusCurrent = iStatus == ID_STATUS_CURRENT;
		BOOL bIsInitialStatusCurrent = iInitialStatus == ID_STATUS_CURRENT;

		for (int i = 0; i < accounts->count; ++i)
		{
			if (!IsAccountEnabled(accounts->pa[i]))
				continue;

			if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0)))
				continue;

			if (!bOnStartup && DBGetContactSettingByte(NULL, accounts->pa[i]->szModuleName, "LockMainStatus", 0))
				continue;

			if (iProfileStatus)
			{
				int iProfileNumber = iStatus - 40083;
				char szSetting[128];
				mir_snprintf(szSetting, SIZEOF(szSetting), "%d_%s", iProfileNumber, accounts->pa[i]->szModuleName);
				iStatus = DBGetContactSettingWord(NULL, "StartupStatus", szSetting, ID_STATUS_OFFLINE);
				if (iStatus == ID_STATUS_IDLE) // the same as ID_STATUS_LAST in StartupStatus
				{
					mir_snprintf(szSetting, SIZEOF(szSetting), "last_%s", accounts->pa[i]->szModuleName);
					iStatus = DBGetContactSettingWord(NULL, "StartupStatus", szSetting, ID_STATUS_OFFLINE);
				}
				else if (iStatus == ID_STATUS_CURRENT)
					iStatus = GetCurrentStatus(accounts->pa[i]->szModuleName);
			}

			if (bIsStatusCurrent)
				iStatus = bOnStartup ? GetStartupStatus(accounts->pa[i]->szModuleName) : GetCurrentStatus(accounts->pa[i]->szModuleName);

			if (bIsInitialStatusCurrent)
				iInitialStatus = bOnStartup ? ID_STATUS_OFFLINE : GetCurrentStatus(accounts->pa[i]->szModuleName);

			if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0) & Proto_Status2Flag(iStatus)) ||
				!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
			{
				if (!(bOnStartup && iStatus == ID_STATUS_OFFLINE) && GetCurrentStatus(accounts->pa[i]->szModuleName) != iStatus && iStatus != iInitialStatus)
				{
					CallProtoService(accounts->pa[i]->szModuleName, PS_SETSTATUS, (WPARAM)iStatus, 0);
#ifdef _DEBUG
					log2file("SetStatusMessage(): Set %s status for %s.", StatusModeToDbSetting(iStatus, ""), accounts->pa[i]->szModuleName);
#endif
				}
				continue;
			}

			if (HasProtoStaticStatusMsg(accounts->pa[i]->szModuleName, iInitialStatus, iStatus))
				continue;

			if (message)
				msg = InsertVarsIntoMsg(message, accounts->pa[i]->szModuleName, iStatus, NULL);

			SaveMessageToDB(accounts->pa[i]->szModuleName, message, TRUE);
			SaveMessageToDB(accounts->pa[i]->szModuleName, msg, FALSE);

			Proto_SetStatus(accounts->pa[i]->szModuleName, iInitialStatus, iStatus, msg);
			mir_free(msg);
		}

		if (GetCurrentStatus(NULL) != iStatus && !bIsStatusCurrent && !iProfileStatus)
		{
			// not so nice...
			UnhookEvent(h_statusmodechange);
			CallService(MS_CLIST_SETSTATUSMODE, (WPARAM)iStatus, 0);
			h_statusmodechange = HookEvent(ME_CLIST_STATUSMODECHANGE, ChangeStatusMessage);
		}
	}
}

INT_PTR ShowStatusMessageDialogInternal(WPARAM wParam, LPARAM lParam)
{
	struct MsgBoxInitData *box_data;
	BOOL idvstatusmsg = FALSE;
	
	if (Miranda_Terminated()) return 0;
		
	if (hTTBButton)
	{
		CallService(MS_TTB_SETBUTTONSTATE, (WPARAM)hTTBButton, (LPARAM)TTBST_RELEASED);
		CallService(MS_TTB_SETBUTTONOPTIONS, MAKEWPARAM((WORD)TTBO_TIPNAME, (WORD)hTTBButton), (LPARAM)Translate("Change Status Message"));
	}

	box_data = (struct MsgBoxInitData *)mir_alloc(sizeof(struct MsgBoxInitData));

	if (accounts->statusMsgCount == 1)
	{
		for (int i = 0; i < accounts->count; ++i)
		{
			if (!IsAccountEnabled(accounts->pa[i]))
				continue;

			if (!CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0))
				continue;

			if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
				continue;

			box_data->m_szProto = accounts->pa[i]->szModuleName;
			box_data->m_iStatusModes = CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0);
			box_data->m_iStatusMsgModes = CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0);
			break;
		}
	}
	else
	{
		for (int i = 0; i < accounts->count; ++i)
		{
			if (!IsAccountEnabled(accounts->pa[i]))
				continue;

			if (!CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0))
				continue;

			if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
				continue;

			if (!accounts->pa[i]->bIsVisible)
				continue;

			if (hProtoStatusMenuItem[i] == (HANDLE)lParam)
			{
				box_data->m_szProto = accounts->pa[i]->szModuleName;
				box_data->m_iStatusModes = CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0);
				box_data->m_iStatusMsgModes = CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0);

				idvstatusmsg = TRUE;
				break;
			}
		}
		if (!idvstatusmsg)
		{
			box_data->m_szProto = NULL;
			box_data->m_iStatusModes = accounts->statusFlags;
			box_data->m_iStatusMsgModes = accounts->statusMsgFlags;
		}
	}
	box_data->m_iStatus = ID_STATUS_CURRENT;
	box_data->m_bOnEvent = FALSE;
	box_data->m_bOnStartup = FALSE;

	if (hwndSAMsgDialog)
		DestroyWindow(hwndSAMsgDialog);
	hwndSAMsgDialog = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_AWAYMSGBOX), NULL, AwayMsgBoxDlgProc, (LPARAM)box_data);
	return 0;
}

INT_PTR ShowStatusMessageDialog(WPARAM wParam, LPARAM lParam)
{
	struct MsgBoxInitData *box_data;
	BOOL idvstatusmsg = FALSE;
	
	if (Miranda_Terminated()) return 0;
		
	box_data = (struct MsgBoxInitData *)mir_alloc(sizeof(struct MsgBoxInitData));

	for (int i = 0; i < accounts->count; ++i)
	{
		if (!IsAccountEnabled(accounts->pa[i]))
			continue;

		if (!CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0))
			continue;

		if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
			continue;

		if (!accounts->pa[i]->bIsVisible)
			continue;
	
		if (!strcmp(accounts->pa[i]->szModuleName, (char *)lParam))
		{
			box_data->m_szProto = accounts->pa[i]->szModuleName;
			box_data->m_iStatusModes = CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0);
			box_data->m_iStatusMsgModes = CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0);

			idvstatusmsg = TRUE;
			break;
		}
	}
	if (!idvstatusmsg)
	{
		box_data->m_szProto = NULL;
		box_data->m_iStatusModes = accounts->statusFlags;
		box_data->m_iStatusMsgModes = accounts->statusMsgFlags;
	}
	box_data->m_iStatus = ID_STATUS_CURRENT;
	box_data->m_bOnEvent = FALSE;
	box_data->m_bOnStartup = FALSE;

	if (hwndSAMsgDialog)
		DestroyWindow(hwndSAMsgDialog);
	hwndSAMsgDialog = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_AWAYMSGBOX), NULL, AwayMsgBoxDlgProc, (LPARAM)box_data);

	return 0;
}

static int ChangeStatusMessage(WPARAM wParam, LPARAM lParam)
{
	int iStatus = (int)wParam;
	char *szProto = (char*)lParam;
	int iDlgFlags;
	BOOL bShowDlg, bOnStartup = FALSE, bGlobalStartupStatus = TRUE, bScreenSaverRunning = FALSE;
	char szSetting[80];

	if (Miranda_Terminated()) return 0;

	// TODO this could be done better
	if (szProto && !strcmp(szProto, "SimpleStatusMsgGlobalStartupStatus"))
	{
		szProto = NULL;
		bOnStartup = TRUE;
	}

	if (accounts->statusMsgCount == 1 && !szProto)
	{
		for (int i = 0; i < accounts->count; ++i)
		{
			if (!IsAccountEnabled(accounts->pa[i]))
				continue;

			if (!CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0))
				continue;

			if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
				continue;

			szProto = accounts->pa[i]->szModuleName;
			if (bOnStartup && iStatus == ID_STATUS_CURRENT)
			{
				iStatus = GetStartupStatus(accounts->pa[i]->szModuleName);
				bGlobalStartupStatus = FALSE;
			}
			break;
		}
	}

	mir_snprintf(szSetting, SIZEOF(szSetting), "%sFlags", szProto ? szProto : "");
	iDlgFlags = DBGetContactSettingByte(NULL, "SimpleStatusMsg", (char *)StatusModeToDbSetting(iStatus, szSetting), STATUS_DEFAULT);
	bShowDlg = iDlgFlags & STATUS_SHOW_DLG || bOnStartup;
	SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &bScreenSaverRunning, 0);

	if (szProto)
	{
		struct MsgBoxInitData *box_data;
		int status_modes = 0, status_modes_msg = 0, iProtoFlags;

		status_modes = CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_5, 0);
		if (!(Proto_Status2Flag(iStatus) & status_modes) && iStatus != ID_STATUS_OFFLINE)
			return 0;

		status_modes_msg = CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_3, 0);
		if (!(Proto_Status2Flag(iStatus) & status_modes_msg) || !(CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
		{
			if (bOnStartup && GetCurrentStatus(szProto) != iStatus)
			{
				CallProtoService(szProto, PS_SETSTATUS, iStatus, 0);
#ifdef _DEBUG
				log2file("ChangeStatusMessage(): Set %s status for %s.", StatusModeToDbSetting(iStatus, ""), szProto);
#endif
			}
			return 0;
		}

		mir_snprintf(szSetting, SIZEOF(szSetting), "Proto%sFlags", szProto);
		iProtoFlags = DBGetContactSettingByte(NULL, "SimpleStatusMsg", szSetting, PROTO_DEFAULT);
		if (iProtoFlags & PROTO_NO_MSG || iProtoFlags & PROTO_THIS_MSG)
		{
			if (HasProtoStaticStatusMsg(szProto, iStatus, iStatus))
				return 1;
		}
		else if (iProtoFlags & PROTO_NOCHANGE && !bOnStartup)
		{
			DBVARIANT dbv;
			TCHAR *msg = NULL;

			mir_snprintf(szSetting, SIZEOF(szSetting), "FCur%sMsg", szProto);
			if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", szSetting, &dbv))
			{
				msg = mir_tstrdup(dbv.ptszVal);
				DBFreeVariant(&dbv);
			}
			//else
			//	msg = mir_tstrdup(_T(""));
#ifdef _DEBUG
			log2file("ChangeStatusMessage(): Set %s status and \"" TCHAR_STR_PARAM "\" status message for %s.", StatusModeToDbSetting(iStatus, ""), msg, szProto);
#endif
			SetStatusMessage(szProto, iStatus, iStatus, msg, FALSE);
			if (msg) mir_free(msg);
			return 1;
		}

		if (!bShowDlg || bScreenSaverRunning)
		{
			TCHAR *msg = GetAwayMessageFormat(iStatus, szProto);
#ifdef _DEBUG
			log2file("ChangeStatusMessage(): Set %s status and \"" TCHAR_STR_PARAM "\" status message for %s.", StatusModeToDbSetting(iStatus, ""), msg, szProto);
#endif
			SetStatusMessage(szProto, iStatus, iStatus, msg, FALSE);
			if (msg) mir_free(msg);
			return 1;
		}

		box_data = (struct MsgBoxInitData *) mir_alloc(sizeof(struct MsgBoxInitData));
		box_data->m_szProto = szProto;

		if (!bOnStartup)
			SaveStatusAsCurrent(szProto, iStatus);

		if (GetCurrentStatus(szProto) == iStatus || (bOnStartup && !bGlobalStartupStatus))
			box_data->m_iStatus = ID_STATUS_CURRENT;
		else
			box_data->m_iStatus = iStatus;

		box_data->m_iStatusModes = status_modes;
		box_data->m_iStatusMsgModes = status_modes_msg;
		box_data->m_bOnEvent = TRUE;
		box_data->m_bOnStartup = bOnStartup;

		if (hwndSAMsgDialog)
			DestroyWindow(hwndSAMsgDialog);
		hwndSAMsgDialog = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_AWAYMSGBOX), NULL, AwayMsgBoxDlgProc, (LPARAM)box_data);
	}
	else
	{
		struct MsgBoxInitData *box_data;
		int iProtoFlags;

		// iStatus == ID_STATUS_CURRENT only when bOnStartup == TRUE
		if (iStatus == ID_STATUS_OFFLINE || (!(accounts->statusMsgFlags & Proto_Status2Flag(iStatus)) && iStatus != ID_STATUS_CURRENT))
			return 0;

		iProtoFlags = DBGetContactSettingByte(NULL, "SimpleStatusMsg", "ProtoFlags", PROTO_DEFAULT);
		if (!bShowDlg || bScreenSaverRunning || (iProtoFlags & PROTO_NOCHANGE && !bOnStartup))
		{
			TCHAR *msg = NULL;
			for (int i = 0; i < accounts->count; ++i)
			{
				if (!IsAccountEnabled(accounts->pa[i]))
					continue;

				if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0)))
					continue;

				if (DBGetContactSettingByte(NULL, accounts->pa[i]->szModuleName, "LockMainStatus", 0))
					continue;

				if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0) & Proto_Status2Flag(iStatus)) ||
					!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
					continue;

				if (iProtoFlags & PROTO_NOCHANGE)
				{
					DBVARIANT dbv;
					mir_snprintf(szSetting, SIZEOF(szSetting), "FCur%sMsg", accounts->pa[i]->szModuleName);
					if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", szSetting, &dbv))
					{
						msg = mir_tstrdup(dbv.ptszVal);
						DBFreeVariant(&dbv);
					}
					//else
					//	msg = mir_tstrdup(_T(""));
				}
				else
					msg = GetAwayMessageFormat(iStatus, NULL);
#ifdef _DEBUG
				log2file("ChangeStatusMessage(): Set %s status and \"" TCHAR_STR_PARAM "\" status message for %s.", StatusModeToDbSetting(iStatus, ""), msg, accounts->pa[i]->szModuleName);
#endif
				SetStatusMessage(accounts->pa[i]->szModuleName, iStatus, iStatus, msg, FALSE);
				if (msg) { mir_free(msg); msg = NULL; }
			}
			return 1;
		}

		box_data = (struct MsgBoxInitData *)mir_alloc(sizeof(struct MsgBoxInitData));
		box_data->m_szProto = NULL;
		box_data->m_iStatus = iStatus;
		box_data->m_iStatusModes = accounts->statusFlags;
		box_data->m_iStatusMsgModes = accounts->statusMsgFlags;
		box_data->m_bOnEvent = TRUE;
		box_data->m_bOnStartup = bOnStartup;

		if (hwndSAMsgDialog)
			DestroyWindow(hwndSAMsgDialog);
		hwndSAMsgDialog = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_AWAYMSGBOX), NULL, AwayMsgBoxDlgProc, (LPARAM)box_data);
	}
	return 0;
}

static INT_PTR ChangeStatusMsg(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage(wParam, lParam);
	return 0;
}

static INT_PTR SetOfflineStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_OFFLINE, (LPARAM)NULL);
	return 0;
}

static INT_PTR SetOnlineStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_ONLINE, (LPARAM)NULL);
	return 0;
}

static INT_PTR SetAwayStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_AWAY, (LPARAM)NULL);
	return 0;
}

static INT_PTR SetDNDStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_DND, (LPARAM)NULL);
	return 0;
}

static INT_PTR SetNAStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_NA, (LPARAM)NULL);
	return 0;
}

static INT_PTR SetOccupiedStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_OCCUPIED, (LPARAM)NULL);
	return 0;
}

static INT_PTR SetFreeChatStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_FREECHAT, (LPARAM)NULL);
	return 0;
}

static INT_PTR SetInvisibleStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_INVISIBLE, (LPARAM)NULL);
	return 0;
}

static INT_PTR SetOnThePhoneStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_ONTHEPHONE, (LPARAM)NULL);
	return 0;
}

static INT_PTR SetOutToLunchStatus(WPARAM wParam, LPARAM lParam)
{
	ChangeStatusMessage((WPARAM)ID_STATUS_OUTTOLUNCH, (LPARAM)NULL);
	return 0;
}

static int ProcessProtoAck(WPARAM wParam,LPARAM lParam)
{
	ACKDATA *ack = (ACKDATA *)lParam;

	if (!ack || !ack->szModule)
		return 0;

	if (ack->type == ACKTYPE_AWAYMSG && ack->result == ACKRESULT_SENTREQUEST && !ack->lParam)
	{
		TCHAR *tszMsg = GetAwayMessage(CallProtoService((char *)ack->szModule, PS_GETSTATUS, 0, 0), (char *)ack->szModule, TRUE, NULL);
#ifdef _UNICODE
		{
			char *szMsg = mir_u2a(tszMsg);
			CallContactService(ack->hContact, PSS_AWAYMSG, (WPARAM)(HANDLE)ack->hProcess, (LPARAM)szMsg);
			if (szMsg) mir_free(szMsg);
		}
#else
		CallContactService(ack->hContact, PSS_AWAYMSG, (WPARAM)(HANDLE)ack->hProcess, (LPARAM)tszMsg);
#endif
#ifdef _DEBUG
		log2file("ProcessProtoAck(): Send away message \"" TCHAR_STR_PARAM "\" reply.", tszMsg);
#endif
		if (tszMsg) mir_free(tszMsg);
		return 0;
	}

	if (ack->type != ACKTYPE_STATUS || ack->result != ACKRESULT_SUCCESS || ack->hContact != NULL)
		return 0;

	if (ack->lParam >= ID_STATUS_CONNECTING && ack->lParam < ID_STATUS_CONNECTING + MAX_CONNECT_RETRIES)
		ack->lParam = ID_STATUS_OFFLINE;

	SaveStatusAsCurrent(ack->szModule, (int)ack->lParam);
#ifdef _DEBUG
	log2file("ProcessProtoAck(): Set %s (%d) status for %s.", StatusModeToDbSetting((int)ack->lParam, ""), (int)ack->lParam, (char *)ack->szModule);
#endif

	return 0;
}

int SetStartupStatus(int i)
{
	int flags;
	char szSetting[80];
	TCHAR *fmsg = NULL, *msg = NULL;
	int iStatus = GetStartupStatus(accounts->pa[i]->szModuleName);

	if (iStatus == ID_STATUS_OFFLINE)
		return -1;

	if (!CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0) || 
		!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
	{
		CallProtoService(accounts->pa[i]->szModuleName, PS_SETSTATUS, (WPARAM)iStatus, 0);
		return -1;
	}

	mir_snprintf(szSetting, SIZEOF(szSetting), "Proto%sFlags", accounts->pa[i]->szModuleName);
	flags = DBGetContactSettingByte(NULL, "SimpleStatusMsg", szSetting, PROTO_DEFAULT);
	if (flags & PROTO_NO_MSG || flags & PROTO_THIS_MSG)
	{
		if (HasProtoStaticStatusMsg(accounts->pa[i]->szModuleName, ID_STATUS_OFFLINE, iStatus))
			return 0;
	}
	else if (flags & PROTO_NOCHANGE)
	{
		DBVARIANT dbv;
		mir_snprintf(szSetting, SIZEOF(szSetting), "FCur%sMsg", accounts->pa[i]->szModuleName);
		if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", szSetting, &dbv))
		{
			fmsg = mir_tstrdup(dbv.ptszVal);
			DBFreeVariant(&dbv);
		}
		//else
		//	fmsg = mir_tstrdup(_T(""));
	}
	else
		fmsg = GetAwayMessageFormat(iStatus, accounts->pa[i]->szModuleName);

#ifdef _DEBUG
	log2file("SetStartupStatus(): Set %s status and \"" TCHAR_STR_PARAM "\" status message for %s.", StatusModeToDbSetting(iStatus, ""), fmsg, accounts->pa[i]->szModuleName);
#endif

	if (fmsg)
		msg = InsertVarsIntoMsg(fmsg, accounts->pa[i]->szModuleName, iStatus, NULL);
			
	SaveMessageToDB(accounts->pa[i]->szModuleName, fmsg, TRUE);
	SaveMessageToDB(accounts->pa[i]->szModuleName, msg, FALSE);

	if (fmsg)
		mir_free(fmsg);

	Proto_SetStatus(accounts->pa[i]->szModuleName, ID_STATUS_OFFLINE, iStatus, msg /*? msg : _T("")*/);
	mir_free(msg);

	return 0;
}

VOID CALLBACK SetStartupStatusGlobal(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	int prev_status_mode = -1, status_mode, temp_status_mode = ID_STATUS_OFFLINE, i;
	BOOL globalstatus = TRUE;

	KillTimer(hwnd, idEvent);

	// is global status mode going to be set?
	for (i = 0; i < accounts->count; ++i)
	{
		if (!IsAccountEnabled(accounts->pa[i]))
			continue;

		if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0)))
			continue;

		status_mode = GetStartupStatus(accounts->pa[i]->szModuleName);

		if (status_mode != ID_STATUS_OFFLINE)
			temp_status_mode = status_mode;

		if (status_mode != prev_status_mode && prev_status_mode != -1)
		{
			globalstatus = FALSE;
			break;
		}

		prev_status_mode = status_mode;
	}

	// popup status msg dialog at startup?
	if (DBGetContactSettingByte(NULL, "SimpleStatusMsg", "StartupPopupDlg", 1) && accounts->statusMsgFlags)
	{
		if (globalstatus)
		{
			ChangeStatusMessage((WPARAM)status_mode, (LPARAM)"SimpleStatusMsgGlobalStartupStatus");
		}
		else
		{
			// pseudo-currentDesiredStatusMode ;-)
			DBWriteContactSettingWord(NULL, "SimpleStatusMsg", "StartupStatus", (WORD)temp_status_mode);
			ChangeStatusMessage((WPARAM)ID_STATUS_CURRENT, (LPARAM)"SimpleStatusMsgGlobalStartupStatus");
		}
		return;
	}

	for (i = 0; i < accounts->count; ++i)
	{
		if (!IsAccountEnabled(accounts->pa[i]))
			continue;

		if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0)))
			continue;

//		if (DBGetContactSettingByte(NULL, accounts->pa[i]->szModuleName, "LockMainStatus", 0))
//			continue;

		SetStartupStatus(i);
	}
}

VOID CALLBACK SetStartupStatusProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	BOOL found = FALSE;
	int i;

	for (i = 0; i < accounts->count; ++i)
	{
		if (!IsAccountEnabled(accounts->pa[i]))
			continue;

		if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0)&~CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0)))
			continue;

		if (g_uSetStatusTimer[i] == idEvent)
		{
			KillTimer(NULL, g_uSetStatusTimer[i]);
			found = TRUE;
			break;
		}
	}

	if (!found)
	{
		KillTimer(hwnd, idEvent);
		return;
	}

	SetStartupStatus(i);
}

VOID CALLBACK UpdateMsgTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	MIRANDA_IDLE_INFO mii = {0};
	mii.cbSize = sizeof(mii);
	CallService(MS_IDLE_GETIDLEINFO, 0, (LPARAM)&mii);
	if (DBGetContactSettingByte(NULL, "SimpleStatusMsg", "NoUpdateOnIdle", 1) && mii.idleType)
		return;

	if (!hwndSAMsgDialog)
	{
		char szBuffer[64];
		DBVARIANT dbv;
		TCHAR *tszMsg;
		int iCurrentStatus;

		for (int i = 0; i < accounts->count; ++i)
		{
			if (!IsAccountEnabled(accounts->pa[i]))
				continue;

			if (!CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0))
				continue;

			if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
				continue;

			iCurrentStatus = CallProtoService(accounts->pa[i]->szModuleName, PS_GETSTATUS, 0, 0);
			if (iCurrentStatus < ID_STATUS_ONLINE)
				continue;

			mir_snprintf(szBuffer, SIZEOF(szBuffer), "FCur%sMsg", accounts->pa[i]->szModuleName);
			if (DBGetContactSettingTString(NULL, "SimpleStatusMsg", szBuffer, &dbv))
				continue;

			tszMsg = InsertVarsIntoMsg(dbv.ptszVal, accounts->pa[i]->szModuleName, iCurrentStatus, NULL);
			DBFreeVariant(&dbv);

			mir_snprintf(szBuffer, SIZEOF(szBuffer), "Cur%sMsg", accounts->pa[i]->szModuleName);
			if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", szBuffer, &dbv))
			{
				if (tszMsg && dbv.ptszVal && !lstrcmp(tszMsg, dbv.ptszVal) || !tszMsg && !dbv.ptszVal)
				{
					DBFreeVariant(&dbv);
					mir_free(tszMsg);
					continue;
				}
				DBFreeVariant(&dbv);
			}

			if (tszMsg && lstrlen(tszMsg))
			{
#ifdef _DEBUG
				log2file("UpdateMsgTimerProc(): Set %s status and \"" TCHAR_STR_PARAM "\" status message for %s.", StatusModeToDbSetting(iCurrentStatus, ""), tszMsg, accounts->pa[i]->szModuleName);
#endif
				Proto_SetStatus(accounts->pa[i]->szModuleName, iCurrentStatus, iCurrentStatus, tszMsg);
				SaveMessageToDB(accounts->pa[i]->szModuleName, tszMsg, FALSE);
			}
			mir_free(tszMsg);
		}
	}
}

static int AddTopToolbarButton(WPARAM wParam, LPARAM lParam)
{
	TTBButtonV2 ttbb = {0};

	ttbb.cbSize = sizeof(ttbb);
	ttbb.hIconUp = ttbb.hIconDn = LoadIconEx("csmsg");
	ttbb.pszServiceUp = ttbb.pszServiceDown = MS_SIMPLESTATUSMSG_SHOWDIALOGINT;
	ttbb.dwFlags = TTBBF_VISIBLE | TTBBF_SHOWTOOLTIP;
	ttbb.name = Translate("Change Status Message");
	hTTBButton = (HANDLE)CallService(MS_TTB_ADDBUTTON, (WPARAM)&ttbb, 0);

	if (hTTBButton != (HANDLE)-1)
		CallService(MS_TTB_SETBUTTONOPTIONS, MAKEWPARAM((WORD)TTBO_TIPNAME, (WORD)hTTBButton), (LPARAM)Translate("Change Status Message"));
	ReleaseIconEx("csmsg");

	return 0;
}

void AddToolbarButton(void)
{
	TBButton tbb = {0};

	tbb.cbSize = sizeof(tbb);
	tbb.tbbFlags = TBBF_VISIBLE | TBBF_SHOWTOOLTIP;
	tbb.pszButtonID = "sachmsg_btn";
	tbb.pszButtonName = Translate("Change Status Message");
	tbb.pszServiceName = MS_SIMPLESTATUSMSG_SHOWDIALOGINT;
	tbb.pszTooltipUp = Translate("Change Status Message");
	tbb.hPrimaryIconHandle = GetIconHandle(IDI_CSMSG);
	tbb.defPos = 11000;
	CallService(MS_TB_ADDBUTTON, 0, (LPARAM)&tbb);
}

void RegisterHotkey(void)
{
	HOTKEYDESC hkd = {0};

	hkd.cbSize = sizeof(hkd);
	hkd.dwFlags = HKD_TCHAR;
	hkd.pszName = "SimpleStatusMsg_OpenDialog";
	hkd.ptszDescription = _T("Open Status Message Dialog");
	hkd.ptszSection = _T("Status Message");
	hkd.pszService = MS_SIMPLESTATUSMSG_SHOWDIALOGINT;
	hkd.DefHotKey = HOTKEYCODE(HOTKEYF_CONTROL, VK_OEM_3);
	CallService(MS_HOTKEY_REGISTER, 0, (LPARAM)&hkd);
}

static int OnIconsChanged(WPARAM wParam, LPARAM lParam)
{
	if (hTTBButton)
	{
		CallService(MS_TTB_REMOVEBUTTON, (WPARAM)hTTBButton, (LPARAM)0);
		AddTopToolbarButton(0, 0);
	}
	return 0;
}

static int ChangeStatusMsgPrebuild(WPARAM wParam, LPARAM lParam)
{
#ifdef _DEBUG
	log2file("ChangeStatusMsgPrebuild()");
#endif
	PROTOACCOUNT **pa;
	int iStatusMenuItemCount = 0, count, i;
	DWORD iStatusMsgFlags = 0;

	ProtoEnumAccounts(&count, &pa);
	hProtoStatusMenuItem = (HANDLE *)mir_realloc(hProtoStatusMenuItem, sizeof(HANDLE) * count);
	for (i = 0; i < count; ++i)
	{
		if (!IsAccountEnabled(pa[i]))
			continue;

		if (CallProtoService(pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND)
			iStatusMsgFlags |= CallProtoService(pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3,0);

		if (!pa[i]->bIsVisible)
			continue;

		iStatusMenuItemCount++;
	}

	if (!iStatusMsgFlags || !iStatusMenuItemCount)
		return 0;

	CLISTMENUITEM mi = {0};
	mi.cbSize = sizeof(mi);
	mi.flags = CMIF_ICONFROMICOLIB | CMIF_TCHAR;
	if (!DBGetContactSettingByte(NULL, "SimpleStatusMsg", "ShowStatusMenuItem", 1))
		mi.flags |= CMIF_HIDDEN;
	mi.icolibItem = GetIconHandle(IDI_CSMSG);
	mi.pszService = MS_SIMPLESTATUSMSG_SHOWDIALOGINT;
	mi.ptszName = LPGENT("Status Message...");
	mi.position = 2000200000;
	CallService(MS_CLIST_ADDSTATUSMENUITEM, 0, (LPARAM)&mi);

	mi.popupPosition = 500084000;
	mi.position = 2000040000;

	for (i = 0; i < count; ++i)
	{
		char szSetting[80];
		TCHAR szBuffer[256];
		int iProtoFlags;

		if (!IsAccountEnabled(pa[i]))
			continue;

		if (!CallProtoService(pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0))
			continue;

		if (!(CallProtoService(pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
			continue;

		if (!pa[i]->bIsVisible)
			continue;

		mir_snprintf(szSetting, SIZEOF(szSetting), "Proto%sFlags", pa[i]->szModuleName);
		iProtoFlags = DBGetContactSettingByte(NULL, "SimpleStatusMsg", szSetting, PROTO_DEFAULT);
		if (iProtoFlags & PROTO_NO_MSG || iProtoFlags & PROTO_THIS_MSG)
			continue;

		if (DBGetContactSettingByte(NULL, pa[i]->szModuleName, "LockMainStatus", 0) &&
			CallService(MS_SYSTEM_GETVERSION, 0, 0) >= PLUGIN_MAKE_VERSION(0, 9, 0, 10))
		{
			mir_sntprintf(szBuffer, SIZEOF(szBuffer), TranslateT("%s (locked)"), pa[i]->tszAccountName);
			mi.ptszPopupName = szBuffer;
		}
		else mi.ptszPopupName = pa[i]->tszAccountName;
		hProtoStatusMenuItem[i] = (HANDLE)CallService(MS_CLIST_ADDSTATUSMENUITEM, 0, (LPARAM)&mi);
	}

	return 0;
}

static int OnIdleChanged(WPARAM, LPARAM lParam)
{
#ifdef _DEBUG
	log2file("OnIdleChanged()");
#endif
	if (!(lParam & IDF_ISIDLE))
		g_iIdleTime = -1;

	MIRANDA_IDLE_INFO mii = {0};
	mii.cbSize = sizeof(mii);
	CallService(MS_IDLE_GETIDLEINFO, 0, (LPARAM)&mii);
	if (mii.aaStatus == 0)
	{
#ifdef _DEBUG
		log2file("OnIdleChanged(): AutoAway disabled");
#endif
		return 0;
	}

	for (int i = 0; i < accounts->count; ++i)
	{
		if (!IsAccountEnabled(accounts->pa[i]))
			continue;

		if (DBGetContactSettingByte(NULL, accounts->pa[i]->szModuleName, "LockMainStatus", 0))
			continue;

		int iStatusBits = CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0);
		int iStatus = mii.aaStatus;
		if (!(iStatusBits & Proto_Status2Flag(iStatus)))
		{
			if (iStatusBits & Proto_Status2Flag(ID_STATUS_AWAY))
				iStatus = ID_STATUS_AWAY;
			else
				continue;
		}

		int iCurrentStatus = CallProtoService(accounts->pa[i]->szModuleName, PS_GETSTATUS, 0, 0);
		if (iCurrentStatus < ID_STATUS_ONLINE || iCurrentStatus == ID_STATUS_INVISIBLE)
			continue;

		if ((lParam & IDF_ISIDLE && (DBGetContactSettingByte(NULL, "AutoAway", accounts->pa[i]->szModuleName, 0) ||
			iCurrentStatus == ID_STATUS_ONLINE || iCurrentStatus == ID_STATUS_FREECHAT)) ||
			(!(lParam & IDF_ISIDLE) && !mii.aaLock))
		{
			if (!(lParam & IDF_ISIDLE))
				iStatus = ID_STATUS_ONLINE;
			TCHAR *tszMsg = GetAwayMessage(iStatus, accounts->pa[i]->szModuleName, FALSE, NULL);
			TCHAR *tszVarsMsg = InsertVarsIntoMsg(tszMsg, accounts->pa[i]->szModuleName, iStatus, NULL);
			SaveMessageToDB(accounts->pa[i]->szModuleName, tszMsg, TRUE);
			SaveMessageToDB(accounts->pa[i]->szModuleName, tszVarsMsg, FALSE);
			mir_free(tszMsg);
			mir_free(tszVarsMsg);
		}
	}

	return 0;
}

static int CSStatusChange(WPARAM wParam, LPARAM lParam)
{
	PROTOCOLSETTINGEX** ps = *(PROTOCOLSETTINGEX***)wParam;
	int status_mode, CSProtoCount;
	char szSetting[80];
	TCHAR *msg = NULL;

	if (ps == NULL) return -1;

	CSProtoCount = CallService(MS_CS_GETPROTOCOUNT, 0, 0);
	for (int i = 0; i < CSProtoCount; ++i)
	{
		if (ps[i]->szName == NULL || !*ps[i]->szName) continue;
		if (ps[i]->status == ID_STATUS_IDLE)
			status_mode = ps[i]->lastStatus;
		else if (ps[i]->status == ID_STATUS_CURRENT)
			status_mode = CallProtoService(ps[i]->szName, PS_GETSTATUS, 0, 0);
		else
			status_mode = ps[i]->status;

		SaveStatusAsCurrent(ps[i]->szName, status_mode);
#ifdef _DEBUG
		log2file("CSStatusChange(): Set %s status for %s.", StatusModeToDbSetting(status_mode, ""), ps[i]->szName);
#endif

		// TODO SaveMessageToDB also when NULL?
		if (ps[i]->szMsg)
		{
			int max_hist_msgs, j;
			DBVARIANT dbv;
			char buff[80];
			BOOL found = FALSE;
#ifdef _UNICODE
			wchar_t *szMsgW = mir_a2u(ps[i]->szMsg);
#endif

#ifdef _DEBUG
			log2file("CSStatusChange(): Set \"%s\" status message for %s.", ps[i]->szMsg, ps[i]->szName);
#endif
			max_hist_msgs = DBGetContactSettingByte(NULL, "SimpleStatusMsg", "MaxHist", 10);
			for (j = 1; j <= max_hist_msgs; j++)
			{
				mir_snprintf(buff, SIZEOF(buff), "SMsg%d", j);
				if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", buff, &dbv))
				{
#ifdef _UNICODE
					if (!lstrcmp(dbv.ptszVal, szMsgW))
#else
					if (!lstrcmp(dbv.ptszVal, ps[i]->szMsg))
#endif
					{
						found = TRUE;
						mir_snprintf(szSetting, SIZEOF(szSetting), "Last%sMsg", ps[i]->szName);
						DBWriteContactSettingString(NULL, "SimpleStatusMsg", szSetting, buff);
						DBFreeVariant(&dbv);
						break;
					}
				}
			}

			if (!found)
			{
				mir_snprintf(buff, SIZEOF(buff), "FCur%sMsg", ps[i]->szName);
				mir_snprintf(szSetting, SIZEOF(szSetting), "Last%sMsg", ps[i]->szName);
				DBWriteContactSettingString(NULL, "SimpleStatusMsg", szSetting, buff);
			}

			mir_snprintf(szSetting, SIZEOF(szSetting), "%sMsg", ps[i]->szName);
#ifdef _UNICODE
			DBWriteContactSettingWString(NULL, "SRAway", StatusModeToDbSetting(status_mode, szSetting), szMsgW);
			msg = InsertVarsIntoMsg(szMsgW, ps[i]->szName, status_mode, NULL);
			SaveMessageToDB(ps[i]->szName, szMsgW, TRUE);
			mir_free(szMsgW);
#else
			DBWriteContactSettingString(NULL, "SRAway", StatusModeToDbSetting(status_mode, szSetting), ps[i]->szMsg);
			msg = InsertVarsIntoMsg(ps[i]->szMsg, ps[i]->szName, status_mode, NULL);
			SaveMessageToDB(ps[i]->szName, ps[i]->szMsg, TRUE);
#endif
			SaveMessageToDB(ps[i]->szName, msg, FALSE);
			mir_free(msg);
		}
	}

	return 0;
}

static TCHAR *ParseWinampSong(ARGUMENTSINFO *ai)
{
	TCHAR *ptszWinampTitle;

	if (ai->argc != 1)
		return NULL;

	ai->flags |= AIF_DONTPARSE;
	ptszWinampTitle = GetWinampSong();

	if (ptszWinampTitle != NULL)
	{
		mir_free(g_ptszWinampSong);
		g_ptszWinampSong = mir_tstrdup(ptszWinampTitle);
	}
	else if (g_ptszWinampSong && lstrcmp(g_ptszWinampSong, _T("SimpleStatusMsg")) && DBGetContactSettingByte(NULL, "SimpleStatusMsg", "AmpLeaveTitle", 1))
		ptszWinampTitle = mir_tstrdup(g_ptszWinampSong);

	return ptszWinampTitle;
}

static TCHAR *ParseDate(ARGUMENTSINFO *ai)
{
	TCHAR szStr[128] = {0};

	if (ai->argc != 1)
		return NULL;

	ai->flags |= AIF_DONTPARSE;
	GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, NULL, NULL, szStr, SIZEOF(szStr));

	return mir_tstrdup(szStr);
}

int ICQMsgTypeToStatus(int iMsgType)
{
	switch (iMsgType)
	{
		case MTYPE_AUTOONLINE: return ID_STATUS_ONLINE;
		case MTYPE_AUTOAWAY: return ID_STATUS_AWAY;
		case MTYPE_AUTOBUSY: return ID_STATUS_OCCUPIED;
		case MTYPE_AUTONA: return ID_STATUS_NA;
		case MTYPE_AUTODND: return ID_STATUS_DND;
		case MTYPE_AUTOFFC: return ID_STATUS_FREECHAT;
		default: return ID_STATUS_OFFLINE;
	}
}

static int OnICQStatusMsgRequest(WPARAM wParam, LPARAM lParam, LPARAM lMirParam)
{
#ifdef _DEBUG
	log2file("OnICQStatusMsgRequest(): UIN: %d on %s", (int)lParam, (char *)lMirParam);
#endif

	if (DBGetContactSettingByte(NULL, "SimpleStatusMsg", "NoUpdateOnICQReq", 1))
		return 0;

	HANDLE hContact;
	char *szProto;
	BOOL bContactFound = FALSE;

	hContact = (HANDLE)CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
	while (hContact)
	{
		szProto = (char *)CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM)hContact, 0);
		if (szProto != NULL && !strcmp(szProto, (char *)lMirParam) && DBGetContactSettingDword(hContact, szProto, "UIN", 0) == (DWORD)lParam)
		{
			bContactFound = TRUE;
			break;
		}
		hContact = (HANDLE)CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM)hContact, 0);
	}
	if (!bContactFound)
		return 0;

	int iStatus = ICQMsgTypeToStatus(wParam);
	TCHAR *tszMsg = GetAwayMessage(iStatus, szProto, TRUE, hContact);
	Proto_SetAwayMsgT(szProto, iStatus, tszMsg);
	mir_free(tszMsg);

	return 0;
}

static int OnAccListChanged(WPARAM wParam, LPARAM lParam)
{
#ifdef _DEBUG
	log2file("OnAccListChanged()");
#endif
	accounts->statusFlags = 0;
	accounts->statusCount = 0;
	accounts->statusMsgFlags = 0;
	accounts->statusMsgCount = 0;
	UnhookProtoEvents();

	ProtoEnumAccounts(&accounts->count, &accounts->pa);
	for (int i = 0; i < accounts->count; ++i)
	{
		if (!IsAccountEnabled(accounts->pa[i]))
			continue;

		if (!strcmp(accounts->pa[i]->szProtoName, "ICQ"))
			HookProtoEvent(accounts->pa[i]->szModuleName, ME_ICQ_STATUSMSGREQ, OnICQStatusMsgRequest);

		accounts->statusFlags |= (CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0) &~ CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0));
		
		if (CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0) &~ CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0))
			accounts->statusCount++;

		if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_MODEMSGSEND))
			continue;

		accounts->statusMsgFlags |= CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3,0);

		if (!CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_3, 0))
			continue;

		accounts->statusMsgCount++;
	}

	return 0;
}

static int OnModulesLoaded(WPARAM wParam, LPARAM lParam)
{
#ifdef _DEBUG
	log2file("### Session started ###");
#endif
	// known modules list
	if (ServiceExists("DBEditorpp/RegisterSingleModule"))
		CallService("DBEditorpp/RegisterSingleModule", (WPARAM)"SimpleStatusMsg", 0);

	if (ServiceExists(MS_UPDATE_REGISTERFL))
#if defined(_WIN64)
		CallService(MS_UPDATE_REGISTERFL, 4322, (LPARAM)&pluginInfo);
#elif defined(_UNICODE)
		CallService(MS_UPDATE_REGISTERFL, 4321, (LPARAM)&pluginInfo);
#else
		CallService(MS_UPDATE_REGISTERFL, 4320, (LPARAM)&pluginInfo);
#endif

	IconsInit();
	HookEventEx(ME_SKIN2_ICONSCHANGED, OnIconsChanged);
	OnAccListChanged(0, 0);

	LoadAwayMsgModule();

	HookEventEx(ME_TTB_MODULELOADED, AddTopToolbarButton);
	if (ServiceExists(MS_TB_ADDBUTTON))
		AddToolbarButton();

	RegisterHotkey();

	HookEventEx(ME_OPT_INITIALISE, InitOptions);
	h_statusmodechange = HookEvent(ME_CLIST_STATUSMODECHANGE, ChangeStatusMessage);
	HookEventEx(ME_PROTO_ACK, ProcessProtoAck);
	HookEventEx(ME_IDLE_CHANGED, OnIdleChanged);

	HookEventEx(ME_CLIST_PREBUILDSTATUSMENU, ChangeStatusMsgPrebuild);
	ChangeStatusMsgPrebuild(0, 0);

	if (ServiceExists(MS_VARS_REGISTERTOKEN))
	{
		TOKENREGISTER tr = {0};
		tr.cbSize = sizeof(TOKENREGISTER);
		tr.memType = TR_MEM_MIRANDA;
		tr.flags = TRF_FREEMEM | TRF_FIELD | TRF_TCHAR | TRF_PARSEFUNC;
		tr.tszTokenString = _T("winampsong");
		tr.parseFunctionT = ParseWinampSong;
		tr.szHelpText = LPGEN("External Applications\tretrieves song name of the song currently playing in Winamp (Simple Status Message compatible)");
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);

		if (DBGetContactSettingByte(NULL, "SimpleStatusMsg", "ExclDateToken", 0) != 0)
		{
			tr.tszTokenString = _T("date");
			tr.parseFunctionT = ParseDate;
			tr.szHelpText = LPGEN("Miranda Related\tget the date (Simple Status Message compatible)");
			CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);
		}
	}

/*	if (DBGetContactSettingByte(NULL, "SimpleStatusMsg", "AmpLeaveTitle", 1))*/ {
		DBVARIANT dbv;

		if (!DBGetContactSettingTString(NULL, "SimpleStatusMsg", "AmpLastTitle", &dbv))
		{
			g_ptszWinampSong = mir_tstrdup(dbv.ptszVal);
			DBFreeVariant(&dbv);
		}
		else
			g_ptszWinampSong = mir_tstrdup(_T("SimpleStatusMsg"));
	}
/*	else
		g_ptszWinampSong = mir_tstrdup(_T("SimpleStatusMsg"));*/

	if (DBGetContactSettingByte(NULL, "SimpleStatusMsg", "UpdateMsgOn", 1))
		g_uUpdateMsgTimer = SetTimer(NULL, 0, DBGetContactSettingWord(NULL, "SimpleStatusMsg", "UpdateMsgInt", 10) * 1000, (TIMERPROC)UpdateMsgTimerProc);

	if (ServiceExists(MS_CS_SETSTATUSEX))
		HookEventEx(ME_CS_STATUSCHANGEEX, CSStatusChange);

	if (accounts->statusCount == 0)
		return 0;

	if (!ServiceExists(MS_SS_GETPROFILECOUNT))
	{
		if (DBGetContactSettingByte(NULL, "SimpleStatusMsg", "GlobalStatusDelay", 1))
		{
			SetTimer(NULL, 0, DBGetContactSettingWord(NULL, "SimpleStatusMsg", "SetStatusDelay", 300), (TIMERPROC)SetStartupStatusGlobal);
		}
		else
		{
			char szSetting[80];

			g_uSetStatusTimer = (UINT_PTR*)mir_alloc(sizeof(UINT_PTR) * accounts->count);
			for (int i = 0; i < accounts->count; ++i)
			{
				if (!IsAccountEnabled(accounts->pa[i]))
					continue;

				if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0) &~ CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0)))
					continue;

				mir_snprintf(szSetting, SIZEOF(szSetting), "Set%sStatusDelay", accounts->pa[i]->szModuleName);
				g_uSetStatusTimer[i] = SetTimer(NULL, 0, DBGetContactSettingWord(NULL, "SimpleStatusMsg", szSetting, 300), (TIMERPROC)SetStartupStatusProc);
			}
		}
	}

	return 0;
}

static int OnOkToExit(WPARAM wParam, LPARAM lParam)
{
	if (accounts->statusCount)
	{
		char szSetting[80];
		
		for (int i = 0; i < accounts->count; ++i)
		{
			if (!IsAccountEnabled(accounts->pa[i]))
				continue;

			if (!(CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_2, 0) &~ CallProtoService(accounts->pa[i]->szModuleName, PS_GETCAPS, PFLAGNUM_5, 0)))
				continue;
			
			mir_snprintf(szSetting, SIZEOF(szSetting), "Last%sStatus", accounts->pa[i]->szModuleName);
			DBWriteContactSettingWord(NULL, "SimpleStatusMsg", szSetting, (WORD)CallProtoService(accounts->pa[i]->szModuleName, PS_GETSTATUS, 0, 0));
		}

		if (g_ptszWinampSong && lstrcmp(g_ptszWinampSong, _T("SimpleStatusMsg")) /*&& DBGetContactSettingByte(NULL, "SimpleStatusMsg", "AmpLeaveTitle", 1)*/)
			DBWriteMessage("AmpLastTitle", g_ptszWinampSong);
	}

	return 0;
}

static int OnPreShutdown(WPARAM wParam, LPARAM lParam)
{
	if (!accounts->statusMsgFlags)
		return 0;
	
	AwayMsgPreShutdown();
	if (hwndSAMsgDialog) DestroyWindow(hwndSAMsgDialog);
	if (hProtoStatusMenuItem) mir_free(hProtoStatusMenuItem);
	if (g_uSetStatusTimer) mir_free(g_uSetStatusTimer);
	if (g_ptszWinampSong) mir_free(g_ptszWinampSong);
	if (g_uUpdateMsgTimer) KillTimer(NULL, g_uUpdateMsgTimer);

	return 0;
}

static INT_PTR IsSARunning(WPARAM wParam, LPARAM lParam)
{
	return 1;
}

//remember to mir_free() the return value
static INT_PTR sttGetAwayMessageT(WPARAM wParam, LPARAM lParam)
{
	return (INT_PTR)GetAwayMessage((int)wParam, (char*)lParam, TRUE, NULL);
}

#ifdef UNICODE
static INT_PTR sttGetAwayMessage(WPARAM wParam, LPARAM lParam)
{
	TCHAR* msg = GetAwayMessage((int)wParam, (char*)lParam, TRUE, NULL);
	char*  res = mir_t2a(msg);
	mir_free(msg);
	return (INT_PTR)res;
}
#endif

extern "C" int __declspec(dllexport) Load(PLUGINLINK *link)
{
	pluginLink = link;

	mir_getMMI(&mmi);
	mir_getLP(&pluginInfo);
	hwndSAMsgDialog	= NULL;
	accounts = (PROTOACCOUNTS *)mir_alloc(sizeof(PROTOACCOUNTS));

	DBWriteContactSettingWord(NULL, "CList", "Status", (WORD)ID_STATUS_OFFLINE);
	HookEventEx(ME_SYSTEM_MODULESLOADED, OnModulesLoaded);
	HookEventEx(ME_PROTO_ACCLISTCHANGED, OnAccListChanged);

#ifdef UNICODE
	CreateServiceFunctionEx(MS_AWAYMSG_GETSTATUSMSG, sttGetAwayMessage);
	CreateServiceFunctionEx(MS_AWAYMSG_GETSTATUSMSGW, sttGetAwayMessageT);
#else
	CreateServiceFunctionEx(MS_AWAYMSG_GETSTATUSMSG, sttGetAwayMessageT);
#endif
	CreateServiceFunctionEx(MS_SIMPLESTATUSMSG_SETSTATUS, SetStatusModeFromExtern);
	CreateServiceFunctionEx(MS_SIMPLESTATUSMSG_SHOWDIALOG, ShowStatusMessageDialog);
	CreateServiceFunctionEx(MS_SIMPLESTATUSMSG_CHANGESTATUSMSG, ChangeStatusMsg);
	CreateServiceFunctionEx(MS_SIMPLESTATUSMSG_SHOWDIALOGINT, ShowStatusMessageDialogInternal); // internal use ONLY

	// Deprecated SimpleAway services
	CreateServiceFunctionEx(MS_SA_ISSARUNNING, IsSARunning);
	CreateServiceFunctionEx(MS_SA_CHANGESTATUSMSG, ChangeStatusMsg);
	CreateServiceFunctionEx(MS_SA_TTCHANGESTATUSMSG, ShowStatusMessageDialogInternal);
	CreateServiceFunctionEx(MS_SA_SHOWSTATUSMSGDIALOG, ShowStatusMessageDialog);
	CreateServiceFunctionEx(MS_SA_SETSTATUSMODE, SetStatusModeFromExtern);

	CreateServiceFunctionEx(MS_SA_SETOFFLINESTATUS, SetOfflineStatus);
	CreateServiceFunctionEx(MS_SA_SETONLINESTATUS, SetOnlineStatus);
	CreateServiceFunctionEx(MS_SA_SETAWAYSTATUS, SetAwayStatus);
	CreateServiceFunctionEx(MS_SA_SETDNDSTATUS, SetDNDStatus);
	CreateServiceFunctionEx(MS_SA_SETNASTATUS, SetNAStatus);
	CreateServiceFunctionEx(MS_SA_SETOCCUPIEDSTATUS, SetOccupiedStatus);
	CreateServiceFunctionEx(MS_SA_SETFREECHATSTATUS, SetFreeChatStatus);
	CreateServiceFunctionEx(MS_SA_SETINVISIBLESTATUS, SetInvisibleStatus);
	CreateServiceFunctionEx(MS_SA_SETONTHEPHONESTATUS, SetOnThePhoneStatus);
	CreateServiceFunctionEx(MS_SA_SETOUTTOLUNCHSTATUS, SetOutToLunchStatus);

	HookEventEx(ME_SYSTEM_OKTOEXIT, OnOkToExit);
	HookEventEx(ME_SYSTEM_PRESHUTDOWN, OnPreShutdown);

	return 0;
}

extern "C" int __declspec(dllexport) Unload(void)
{
	UnhookEvents();
	UnhookEvent(h_statusmodechange);
	UnhookProtoEvents();
	DestroyServiceFunctionsEx();
	mir_free(accounts);

#ifdef _DEBUG
	log2file("### Session ended ###");
#endif

	return 0;
}
