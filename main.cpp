#include <Windows.h>
#include <iphlpapi.h>

#include <string>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <sstream>

typedef struct IFEntry {
  std::string strDescr;
  IF_INDEX dwIndex;
  IFTYPE dwType;
  DWORD dwInOctets;
  DWORD dwOutOctets;
  DWORD dwAdminStatus;
  DWORD dwOperStatus;
  DWORD dwSpeed;
}IFEntry;

typedef std::vector<IFEntry> IFEntryArray;
typedef std::vector<std::string> StringArray;

typedef struct OctetsData {
  DWORD down;
  DWORD up;
}OctetsData;

static HANDLE _consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

static StringArray getAdapterNames() {
  PIP_ADAPTER_INFO pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(sizeof(IP_ADAPTER_INFO));
  if (!pIpAdapterInfo) return {};
  ULONG dwSize = sizeof(IP_ADAPTER_INFO);

  DWORD dwResultCode = GetAdaptersInfo(pIpAdapterInfo, &dwSize);
  if (ERROR_BUFFER_OVERFLOW == dwResultCode) {
    free(pIpAdapterInfo);
    pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(dwSize);
    if (!pIpAdapterInfo) return {};
    dwResultCode = GetAdaptersInfo(pIpAdapterInfo, &dwSize);
  }

  if (ERROR_SUCCESS != dwResultCode) {
    free(pIpAdapterInfo);
    return {};
  }

  StringArray res;
  PIP_ADAPTER_INFO cursor = pIpAdapterInfo;
  while (cursor) {
    if (cursor->Type == MIB_IF_TYPE_ETHERNET || cursor->Type == IF_TYPE_IEEE80211) {
      res.push_back(cursor->Description);
    }
    cursor = cursor->Next;
  }
  free(pIpAdapterInfo);
  return res;
}

static int32_t indexOf(const StringArray& arr, const std::string& target) {
  int32_t len = (int32_t)arr.size();
  for (int32_t i = 0; i < len; i++) {
    if (arr[i] == target) {
      return i;
    }
  }
  return -1;
}

static IFEntryArray getOperationalEntries(const StringArray& names) {
  MIB_IFTABLE* pMibIfTable = (MIB_IFTABLE*)malloc(sizeof(MIB_IFTABLE));
  if (!pMibIfTable) return {};
  ULONG dwSize = sizeof(MIB_IFTABLE);
  DWORD dwResultCode = GetIfTable(pMibIfTable, &dwSize, TRUE);

  if (ERROR_INSUFFICIENT_BUFFER == dwResultCode) {
    free(pMibIfTable);
    pMibIfTable = (MIB_IFTABLE*)malloc(dwSize);
    if (!pMibIfTable) return {};
  }

  dwResultCode = GetIfTable(pMibIfTable, &dwSize, TRUE);
  if (NO_ERROR != dwResultCode) {
    free(pMibIfTable);
    return {};
  }

  IFEntryArray res;
  for (DWORD i = 0; i < pMibIfTable->dwNumEntries; i++) {
    MIB_IFROW* pRow = &pMibIfTable->table[i];
    if ((pRow->dwType == IF_TYPE_IEEE80211 || pRow->dwType == IF_TYPE_ETHERNET_CSMACD) && pRow->dwOperStatus == IF_OPER_STATUS_OPERATIONAL && indexOf(names, (char*)pRow->bDescr) != -1) {
      IFEntry entry;
      entry.strDescr = (char*)pRow->bDescr;
      entry.dwIndex = pRow->dwIndex;
      entry.dwType = pRow->dwType;
      entry.dwInOctets = pRow->dwInOctets;
      entry.dwOutOctets = pRow->dwOutOctets;
      entry.dwAdminStatus = pRow->dwAdminStatus;
      entry.dwOperStatus = pRow->dwOperStatus;
      entry.dwSpeed = pRow->dwSpeed;
      res.push_back(entry);
    }
  }
  free(pMibIfTable);

  return res;
}

static ULONG delta(DWORD a, DWORD b) {
  if (a >= b) return a - b;
  else return (0xFFFFFFFF - b) + a;
}

static std::string toSpeedString(DWORD dwBytes) {
  std::ostringstream os;
  if (dwBytes < 1024) {
    os << (double)dwBytes << " B";
  } else if (dwBytes < 1024 * 1024) {
    os << floor((double)dwBytes / 1024 * 100) / 100 << " KB";
  } else {
    os << floor((double)dwBytes / 1024 / 1024 * 100) / 100 << " MB";
  }
  return os.str();
}

static void clearLine(uint16_t lineNumber) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(_consoleHandle, &csbi)) return;
  short tmp = csbi.dwCursorPosition.Y - lineNumber;
  COORD targetFirstCellPosition = { 0, tmp < 0 ? 0 : tmp };
  DWORD size = csbi.dwSize.X * lineNumber;
  DWORD cCharsWritten;

  if (!FillConsoleOutputCharacter(_consoleHandle, (TCHAR)' ', size, targetFirstCellPosition, &cCharsWritten)) return;
  if (!GetConsoleScreenBufferInfo(_consoleHandle, &csbi)) return;
  if (!FillConsoleOutputAttribute(_consoleHandle, csbi.wAttributes, size, targetFirstCellPosition, &cCharsWritten)) return;
  SetConsoleCursorPosition(_consoleHandle, targetFirstCellPosition);
}

int __cdecl wmain() {
  StringArray names = getAdapterNames();
  OctetsData data;
  data.down = 0;
  data.up = 0;
  IFEntryArray table = getOperationalEntries(names);
  for (uint32_t i = 0; i < table.size(); i++) {
    data.down += table[i].dwInOctets;
    data.up += table[i].dwOutOctets;
  }

  while (true) {
    DWORD cDown = 0;
    DWORD cUp = 0;
    IFEntryArray table = getOperationalEntries(names);
    uint32_t adapter = table.size();
    for (uint32_t i = 0; i < adapter; i++) {
      cDown += table[i].dwInOctets;
      cUp += table[i].dwOutOctets;
    }
    std::ostringstream os;
    os << "Connected Adapters: " << adapter << std::endl;
    os << "Upload Speed: " << toSpeedString(delta(cUp, data.up)) << std::endl;
    os << "Download Speed: " << toSpeedString(delta(cDown, data.down)) << std::endl;
    clearLine(3);
    std::cout << os.str();
    data.down = cDown;
    data.up = cUp;
    Sleep(1000);
  }

  return 0;
}
