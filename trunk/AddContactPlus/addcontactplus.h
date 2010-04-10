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

// this plugin requires Miranda 0.8 or newer
#define MIRANDA_VER 0x0800

#if defined(UNICODE) && !defined(_UNICODE)
	#define _UNICODE
#endif

#include <stdio.h>
#include <windows.h>
#include "../../include/win2k.h"
#include "../../include/newpluginapi.h"
#include "../../include/m_system.h"
#include "../../include/m_protocols.h"
#include "../../include/m_protosvc.h"
#include "../../include/m_database.h"
#include "../../include/m_clist.h"
#include "../../include/m_genmenu.h"
#include "../../include/m_skin.h"
#include "../../include/m_icolib.h"
#include "../../include/m_langpack.h"
#include "../../include/m_addcontact.h"
#include "../../include/m_icq.h"
#include "../../include/m_utils.h"
#include "m_toolbar.h"
#include "m_addcontactplus.h"
#include "resource.h"

#define	ICON_ADD	"AddContactPlus_Icon"

BOOL CALLBACK AddContactDlgProc(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam);
extern HINSTANCE	hInst;
