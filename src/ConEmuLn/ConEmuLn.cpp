
/*
Copyright (c) 2009-2010 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifdef _DEBUG
//  �����������������, ����� ����� ����� �������� ������� �������� MessageBox, ����� ����������� ����������
//  #define SHOW_STARTED_MSGBOX
//  #define SHOW_WRITING_RECTS
#endif


#include <windows.h>
#include "../common/common.hpp"
#include "../common/pluginW1007.hpp" // ���������� �� 995 �������� SynchoApi
#include "ConEmuLn.h"

#define MAKEFARVERSION(major,minor,build) ( ((major)<<8) | (minor) | ((build)<<16))


#if defined(__GNUC__)
extern "C"{
  BOOL WINAPI DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved );
  void WINAPI SetStartupInfoW(void *aInfo);
  void WINAPI OnConEmuLoaded(struct ConEmuLoadedArg* pConEmuInfo);
};
#endif

HMODULE ghPluginModule = NULL; // ConEmuLn.dll - ��� ������
wchar_t* gszRootKey = NULL;
FarVersion gFarVersion = {{0}};
RegisterBackground_t gfRegisterBackground = NULL;

BOOL gbBackgroundEnabled = FALSE;
COLORREF gcrLinesColor = RGB(0,0,0xA8); // ���� ������� ������



// minimal(?) FAR version 2.0 alpha build FAR_X_VER
int WINAPI _export GetMinFarVersionW(void)
{
	// ACTL_SYNCHRO required
	return MAKEFARVERSION(2,0,max(1007,FAR_X_VER));
}

/* COMMON - ���� ��������� �� ����������� */
void WINAPI _export GetPluginInfoW(struct PluginInfo *pi)
{
    static WCHAR *szMenu[1], szMenu1[255];
    szMenu[0]=szMenu1;
	lstrcpynW(szMenu1, GetMsgW(CEPluginName), 240);

	pi->StructSize = sizeof(struct PluginInfo);
	pi->Flags = PF_PRELOAD;
	pi->DiskMenuStrings = NULL;
	pi->DiskMenuNumbers = 0;
	pi->PluginMenuStrings = szMenu;
	pi->PluginMenuStringsNumber = 1;
	pi->PluginConfigStrings = szMenu;
	pi->PluginConfigStringsNumber = 1;
	pi->CommandPrefix = 0;
	pi->Reserved = 0;
}


BOOL gbInfoW_OK = FALSE;



BOOL WINAPI DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
			{
				ghPluginModule = (HMODULE)hModule;
			}
			break;
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

#if defined(CRTSTARTUP)
extern "C"{
  BOOL WINAPI _DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved);
};

BOOL WINAPI _DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved)
{
  DllMain(hDll, dwReason, lpReserved);
  return TRUE;
}
#endif




BOOL LoadFarVersion()
{
	BOOL lbRc=FALSE;
	WCHAR FarPath[MAX_PATH+1];
	if (GetModuleFileName(0,FarPath,MAX_PATH)) {
		DWORD dwRsrvd = 0;
		DWORD dwSize = GetFileVersionInfoSize(FarPath, &dwRsrvd);
		if (dwSize>0) {
			void *pVerData = calloc(dwSize, 1);
			if (pVerData) {
				VS_FIXEDFILEINFO *lvs = NULL;
				UINT nLen = sizeof(lvs);
				if (GetFileVersionInfo(FarPath, 0, dwSize, pVerData)) {
					TCHAR szSlash[3]; lstrcpyW(szSlash, L"\\");
					if (VerQueryValue ((void*)pVerData, szSlash, (void**)&lvs, &nLen)) {
						gFarVersion.dwVer = lvs->dwFileVersionMS;
						gFarVersion.dwBuild = lvs->dwFileVersionLS;
						lbRc = TRUE;
					}
				}
				free(pVerData);
			}
		}
	}

	if (!lbRc) {
		gFarVersion.dwVerMajor = 2;
		gFarVersion.dwVerMinor = 0;
		gFarVersion.dwBuild = FAR_X_VER;
	}

	return lbRc;
}

void WINAPI _export SetStartupInfoW(void *aInfo)
{
	if (!gFarVersion.dwVerMajor) LoadFarVersion();

	if (gFarVersion.dwBuild>=FAR_Y_VER)
		FUNC_Y(SetStartupInfoW)(aInfo);
	else
		FUNC_X(SetStartupInfoW)(aInfo);

	//_ASSERTE(gszRootKey!=NULL && *gszRootKey!=0);

	gbInfoW_OK = TRUE;

	StartPlugin(FALSE);
}

int WINAPI UpdateConEmuBackground(struct UpdateBackgroundArg* pBk)
{
	DWORD nPanelBackIdx = (pBk->nFarColors[COL_PANELTEXT] & 0xF0) >> 4;

	if (pBk->nLevel == 0)
	{
		HBRUSH hBr = CreateSolidBrush(pBk->crPalette[nPanelBackIdx]);
		if (pBk->LeftPanel.bVisible)
			FillRect(pBk->hdc, &pBk->rcDcLeft, hBr);
		if (pBk->RightPanel.bVisible)
			FillRect(pBk->hdc, &pBk->rcDcRight, hBr);
		DeleteObject(hBr);
	}

	if ((pBk->LeftPanel.bVisible || pBk->RightPanel.bVisible) /*&& pBk->MainFont.nFontHeight>0*/)
	{
		HPEN hPen = CreatePen(PS_SOLID, 1, gcrLinesColor);
		HPEN hOldPen = (HPEN)SelectObject(pBk->hdc, hPen);

		int nCellHeight = 12;
		if (pBk->LeftPanel.bVisible)
			nCellHeight = pBk->rcDcLeft.bottom / (pBk->LeftPanel.rcPanelRect.bottom - pBk->LeftPanel.rcPanelRect.top + 1);
		else
			nCellHeight = pBk->rcDcRight.bottom / (pBk->RightPanel.rcPanelRect.bottom - pBk->RightPanel.rcPanelRect.top + 1);

		int nY1 = (pBk->LeftPanel.bVisible) ? pBk->rcDcLeft.top : pBk->rcDcRight.top;
		int nY2 = (pBk->rcDcLeft.bottom >= pBk->rcDcRight.bottom) ? pBk->rcDcLeft.bottom : pBk->rcDcRight.bottom;
		int nX1 = (pBk->LeftPanel.bVisible) ? 0 : pBk->rcDcRight.left;
		int nX2 = (pBk->RightPanel.bVisible) ? pBk->rcDcRight.right : pBk->rcDcLeft.right;
		for (int Y = nY1; Y < nY2; Y += nCellHeight)
		{
			MoveToEx(pBk->hdc, nX1, Y, NULL);
			LineTo(pBk->hdc, nX2, Y);
		}

		SelectObject(pBk->hdc, hOldPen);
		DeleteObject(hPen);
	}

	return TRUE;
}

void WINAPI OnConEmuLoaded(struct ConEmuLoadedArg* pConEmuInfo)
{
	gfRegisterBackground = pConEmuInfo->RegisterBackground;
	ghPluginModule = pConEmuInfo->hPlugin;
	if (gfRegisterBackground && gbBackgroundEnabled)
	{
		StartPlugin(FALSE);
	}
}

void StartPlugin(BOOL bConfigure)
{
	if (!bConfigure)
	{
		HKEY hkey = NULL;
		if (!RegOpenKeyExW(HKEY_CURRENT_USER, gszRootKey, 0, KEY_READ, &hkey))
		{
			DWORD nVal, nType, nSize; BYTE cVal;
			if (!RegQueryValueExW(hkey, L"PluginEnabled", 0, &(nType = REG_BINARY), &cVal, &(nSize = sizeof(cVal))))
				gbBackgroundEnabled = (cVal != 0);
			if (!RegQueryValueExW(hkey, L"LinesColor", 0, &(nType = REG_DWORD), (LPBYTE)&nVal, &(nSize = sizeof(nVal))))
				gcrLinesColor = nVal;
			RegCloseKey(hkey);
		}
	}

	if (gfRegisterBackground)
	{
		if (gbBackgroundEnabled)
		{
			if (bConfigure)
			{
				BackgroundInfo upd = {sizeof(BackgroundInfo), rbc_Redraw};
				gfRegisterBackground(&upd);
			}
			else
			{
				BackgroundInfo reg = {sizeof(BackgroundInfo), rbc_Register, ghPluginModule, UpdateConEmuBackground, NULL, bup_Panels, 100};
				gfRegisterBackground(&reg);
			}
		}
		else
		{
			BackgroundInfo unreg = {sizeof(BackgroundInfo), rbc_Unregister, ghPluginModule};
			gfRegisterBackground(&unreg);
		}
	}
}

void ExitPlugin(void)
{
	if (gfRegisterBackground != NULL)
	{
		BackgroundInfo inf = {sizeof(BackgroundInfo), rbc_Unregister, ghPluginModule};
		gfRegisterBackground(&inf);
	}
	if (gszRootKey)
	{
		free(gszRootKey);
	}
}

void   WINAPI _export ExitFARW(void)
{
	ExitPlugin();

	if (gFarVersion.dwBuild>=FAR_Y_VER)
		FUNC_Y(ExitFARW)();
	else
		FUNC_X(ExitFARW)();
}


LPCWSTR GetMsgW(int aiMsg)
{
	if (gFarVersion.dwVerMajor==1)
		return L"";
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		return FUNC_Y(GetMsgW)(aiMsg);
	else
		return FUNC_X(GetMsgW)(aiMsg);
}


int WINAPI ConfigureW(int ItemNumber)
{
	if (gFarVersion.dwVerMajor==1)
		return false;
	else if (gFarVersion.dwBuild>=FAR_Y_VER)
		return FUNC_Y(ConfigureW)(ItemNumber);
	else
		return FUNC_X(ConfigureW)(ItemNumber);
}

HANDLE WINAPI _export OpenPluginW(int OpenFrom,INT_PTR Item)
{
	if (!gbInfoW_OK)
		return INVALID_HANDLE_VALUE;

	ConfigureW(0);

	return INVALID_HANDLE_VALUE;
}