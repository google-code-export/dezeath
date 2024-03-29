/*
    AdvancedAutoAway Plugin for Miranda-IM (www.miranda-im.org)
    KeepStatus Plugin for Miranda-IM (www.miranda-im.org)
    StartupStatus Plugin for Miranda-IM (www.miranda-im.org)
    Copyright 2003-2006 P. Boon

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef __M_STATUSPLUGINS
#define __M_STATUSPLUGINS

// -- common status -- (all three plugins)
typedef struct {
	int cbSize;
	char *szName;	// pointer to protocol modulename
	char *szMsg;	// pointer to the status message (may be NULL)
	WORD status;	// the status
	WORD lastStatus;// last status
	TCHAR *tszAccName;
} PROTOCOLSETTINGEX;

// wParam = PROTOCOLSETTINGEX*** (keep it like this for compatibility)
// lParam = 0
// returns 0 on success
#define MS_CS_SETSTATUSEX				"CommonStatus/SetStatusEx"

// wParam = PROTOCOLSETTINGEX*** (keep it like this for compatibility)
// lParam = timeout
// returns hwnd
#define MS_CS_SHOWCONFIRMDLGEX			"CommonStatus/ShowConfirmDialogEx"

// wParam = 0
// lParam = 0
// returns the number of protocols registerd
#define MS_CS_GETPROTOCOUNT				"CommonStatus/GetProtocolCount" // added dec '04

// wParam = PROTOCOLSETTINGEX*** (keep it like this for compatibility)
// lParam = 0
#define ME_CS_STATUSCHANGEEX			"CommonStatus/StatusChangeEx"

// -- startup status --
// wParam = profile number (set to -1 to get default profile)
// lParam = PROTOCOLSETTINGEX***  (keep for... )(memory must be allocated protoCount*PROTOCOLSETTINGEX* and protoCount*PROTOCOLSETTINGEX)
// szMsg member does not have to be freed
// returns 0 on success
#define MS_SS_GETPROFILE				"StartupStatus/GetProfile" // don't use this > jan '05, internal use only

// wParam = profile number
// lParam = 0
// return 0 on success
#define MS_SS_LOADANDSETPROFILE			"StartupStatus/LoadAndSetProfile" // you can use this

// wParam = int*, maybe NULL sets this int to the default profile number
// lParam = 0
// returns profile count
#define MS_SS_GETPROFILECOUNT			"StartupStatus/GetProfileCount"

// wParam = profile number
// lParam = char* (must be allocated, size = 128)
// returns 0 on success
#define MS_SS_GETPROFILENAME			"StartupStatus/GetProfileName"

// -- AdvancedAutoAway --
typedef enum { 
			ACTIVE, // user is active
			STATUS1_SET, // first status change happened 
			STATUS2_SET, // second status change happened
			SET_ORGSTATUS, // user was active again, original status will be restored
			HIDDEN_ACTIVE // user is active, but this is not shown to the outside world
} STATES;

typedef struct {
	PROTOCOLSETTINGEX* protocolSetting;
	int originalStatusMode;	// this is set only when going from ACTIVE to STATUS1_SET (or to STATUS2_SET)
							// (note: this is therefore not always valid)
	STATES 
		oldState,			// state before the call
		curState;			// current state
	BOOL bStatusChanged;		// the status of the protocol will actually be changed
							// (note: unlike the name suggests, the status is changed AFTER this hook is called)
	BOOL bManual;			// state changed becuase status was changed manually
} AUTOAWAYSETTING;
// wParam = 0;
// lParam = AUTOAWAYSETTING*
// Called when a protocol's state in AAA is changed this does NOT necessary means the status was changed
// note: this hook is called for each protocol seperately
#define ME_AAA_STATECHANGED				"AdvancedAutoAway/StateChanged"


// -- KeepStatus --
#define KS_CONN_STATE_LOST				1		// lParam = protocol
#define KS_CONN_STATE_OTHERLOCATION		2		// lParam = protocol
#define KS_CONN_STATE_RETRY				3		// lParam = nth retry
#define KS_CONN_STATE_STOPPEDCHECKING	4		// lParam = TRUE if success, FALSE if failed
#define KS_CONN_STATE_LOGINERROR		5		// lParam = protocol, only if selected in options
#define KS_CONN_STATE_RETRYNOCONN		6		// lParam = nth try, a connection attempt will not be made
// wParam = one of above
// lParam depends on wParam
#define ME_KS_CONNECTIONEVENT			"KeepStatus/ConnectionEvent"

// wParam = 0
// lParam = 0
// returns 0 on succes, nonzero on failure, probably keepstatus wasn't reconnecting
#define MS_KS_STOPRECONNECTING			"KeepStatus/StopReconnecting"

// wParam = TRUE to enable checking a protocol, FALSE to disable checking a protocol
// lParam = protocol
// return 0 on success, nonzero on failure, probably the protocol is 'hard' disabled or not found
// note: you cannot enable a protocol that is disabled in the options screen, you can disable a protocol
// if it's enabled in the option screen.
#define MS_KS_ENABLEPROTOCOL			"KeepStatus/EnableProtocol"

// wParam = 0
// lParam = protocol
// returns TRUE if protocol is enabled for checked, FALSE otherwise
#define MS_KS_ISPROTOCOLENABLED			"KeepStatus/IsProtocolEnabled"

// Indicate the status will be changed which will not be regarded as a connection failure.
// wParam = 0
// lParam = PROTOCOLSETTINGEX* of the new situation
// returns 0
#define MS_KS_ANNOUNCESTATUSCHANGE		"KeepStatus/AnnounceStatusChange"

__inline static int announce_status_change(char *szProto, int newstatus, char *szMsg) {

	PROTOCOLSETTINGEX ps;

	ZeroMemory(&ps, sizeof(PROTOCOLSETTINGEX));
	ps.cbSize = sizeof(PROTOCOLSETTINGEX);
	if (szProto != NULL) {
		ps.lastStatus = CallProtoService(szProto, PS_GETSTATUS, 0, 0);
	} else {
		ps.lastStatus = CallService(MS_CLIST_GETSTATUSMODE, 0, 0);
	}
	ps.status = newstatus;
	ps.szMsg = szMsg;
	ps.szName = szProto;

	return CallService(MS_KS_ANNOUNCESTATUSCHANGE, 0, (LPARAM)&ps);
}

#endif // __M_STATUSPLUGINS
