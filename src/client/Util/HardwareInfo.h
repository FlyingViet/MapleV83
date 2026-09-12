//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../Configuration.h"
#ifdef MS_PLATFORM_WASM
#include <emscripten.h>
#include <cstdlib>
#include <string>
#endif

#ifdef _WIN32
#include <Windows.h>
#include <IPHlpApi.h>
#endif

namespace ms
{
	class HardwareInfo
	{
	public:
#if defined(MS_PLATFORM_WASM)
		// The browser has no MAC/volume serial. Cosmic validates the host string as
		// "<12 hex mac>_<8 hex serial>", so synthesize a stable pseudo id per browser
		// (kept in localStorage) — distinct per device, constant across sessions.
		HardwareInfo()
		{
			char* raw = (char*)EM_ASM_PTR({
				var key = 'mw.hwid';
				var id = null;
				try { id = localStorage.getItem(key); } catch (e) {}
				if (!id || !/^[0-9A-F]{20}$/.test(id)) {
					var hex = '0123456789ABCDEF';
					id = '';
					for (var i = 0; i < 20; i++) id += hex[Math.floor(Math.random() * 16)];
					try { localStorage.setItem(key, id); } catch (e) {}
				}
				var len = lengthBytesUTF8(id) + 1;
				var ptr = _malloc(len);
				stringToUTF8(id, ptr, len);
				return ptr;
			});
			std::string id = raw ? raw : "0123456789AB01234567";
			if (raw)
				std::free(raw);
			std::string mac = id.substr(0, 12);
			std::string serial = id.substr(12, 8);
			std::string macs;
			for (size_t i = 0; i < mac.size(); i += 2)
				macs += (i ? "-" : "") + mac.substr(i, 2);
			Configuration::get().set_macs(&macs[0]);
			Configuration::get().set_hwid(&mac[0], &serial[0]);
		}
#elif !defined(_WIN32)
		// Volume serial and adapter MAC lookups are Win32 API calls. On other
		// non-Windows platforms the HWID/MAC strings keep Configuration's defaults.
		HardwareInfo() {}
#else
		HardwareInfo()
		{
			size_t size = 18;

			// Hard Drive VolumeSerialNumber
			char* volumeSerialNumber = (char*)malloc(size);

			TCHAR szVolume[MAX_PATH + 1];
			TCHAR szFileSystem[MAX_PATH + 1];

			DWORD dwSerialNumber, dwMaxLen, dwSystemFlags;

			TCHAR szDrives[MAX_PATH + 1];
			DWORD dwLen = GetLogicalDriveStrings(MAX_PATH, szDrives);
			TCHAR* pLetter = szDrives;

			BOOL bSuccess;

			bSuccess = GetVolumeInformation(pLetter, szVolume, MAX_PATH, &dwSerialNumber, &dwMaxLen, &dwSystemFlags, szFileSystem, MAX_PATH);

			if (bSuccess)
			{
				sprintf_s(volumeSerialNumber, size, "%X%X", HIWORD(dwSerialNumber), LOWORD(dwSerialNumber));
			}
			else
			{
				printf("Cannot retrieve Volume information for %s\n", pLetter);
				free(volumeSerialNumber);
				return;
			}

			// HWID/MACS
			PIP_ADAPTER_INFO AdapterInfo;
			DWORD dwBufLen = sizeof(IP_ADAPTER_INFO);
			char* hwid = (char*)malloc(size);
			char* macs = (char*)malloc(size);

			AdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));

			if (AdapterInfo == NULL)
			{
				printf("Error allocating memory needed to call GetAdaptersinfo\n");
				free(volumeSerialNumber);
				free(hwid);
				free(macs);
				return;
			}

			// Make an initial call to GetAdaptersInfo to get the necessary size into the dwBufLen variable
			if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == ERROR_BUFFER_OVERFLOW)
			{
				free(AdapterInfo);
				AdapterInfo = (IP_ADAPTER_INFO*)malloc(dwBufLen);

				if (AdapterInfo == NULL)
				{
					printf("Error allocating memory needed to call GetAdaptersinfo\n");
					free(volumeSerialNumber);
					free(hwid);
					free(macs);
					return;
				}
			}

			if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == NO_ERROR)
			{
				// Contains pointer to current adapter info
				PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;

				// Technically should look at pAdapterInfo->AddressLength and not assume it is 6
				sprintf_s(hwid, size, "%02X%02X%02X%02X%02X%02X",
					pAdapterInfo->Address[0], pAdapterInfo->Address[1],
					pAdapterInfo->Address[2], pAdapterInfo->Address[3],
					pAdapterInfo->Address[4], pAdapterInfo->Address[5]);

				Configuration::get().set_hwid(hwid, volumeSerialNumber);

				pAdapterInfo = pAdapterInfo->Next;

				// Technically should look at pAdapterInfo->AddressLength and not assume it is 6
				sprintf_s(macs, size, "%02X-%02X-%02X-%02X-%02X-%02X",
					pAdapterInfo->Address[0], pAdapterInfo->Address[1],
					pAdapterInfo->Address[2], pAdapterInfo->Address[3],
					pAdapterInfo->Address[4], pAdapterInfo->Address[5]);

				Configuration::get().set_macs(macs);
			}

			free(AdapterInfo);
			free(volumeSerialNumber);
			free(hwid);
			free(macs);
		}
#endif
	};
}