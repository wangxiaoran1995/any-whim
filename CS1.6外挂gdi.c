#ifndef UNICODE
#define UNICODE
#endif
#include "time.h"
#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <dwmapi.h>
#define YSUCCESS  0
#define YERROR    -1
#define _In_
#define _In_opt_
#define _Out_opt_
typedef struct _LSA_UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;
typedef enum _MEMORY_INFORMATION_CLASS {
    MemoryBasicInformation,
    MemoryWorkingSetList,
    MemorySectionName
} MEMORY_INFORMATION_CLASS;
typedef NTSTATUS (WINAPI *ZwQueryVirtualMemoryfn) (
    _In_      HANDLE                   ProcessHandle,
    _In_opt_  PVOID                    BaseAddress,
    _In_      MEMORY_INFORMATION_CLASS MemoryInformationClass,
    _Out_     PVOID                    MemoryInformation,
    _In_      SIZE_T                   MemoryInformationLength,
    _Out_opt_ PSIZE_T                  ReturnLength
);
typedef struct {
    UNICODE_STRING SectionFileName;
    WCHAR NameBuffer[MAX_PATH * 2 + 2];
} MEMORY_SECTION_NAME, *PMEMORY_SECTION_NAME;
static ZwQueryVirtualMemoryfn g_ZwQueryVirtualMemoryPtr = NULL;
static int WINAPI YZwQueryVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, MEMORY_INFORMATION_CLASS MemoryInformationClass, PVOID MemoryInformation, ULONG MemoryInformationLength){
    NTSTATUS status = 0;
    if(g_ZwQueryVirtualMemoryPtr == NULL) {
        g_ZwQueryVirtualMemoryPtr = (ZwQueryVirtualMemoryfn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "ZwQueryVirtualMemory");
        if(g_ZwQueryVirtualMemoryPtr == NULL) {
            return YERROR;
        }
    }
    status = g_ZwQueryVirtualMemoryPtr(ProcessHandle, BaseAddress, MemoryInformationClass, MemoryInformation, MemoryInformationLength, NULL);
    if(status >= 0) return YSUCCESS;
    return YERROR;
}

static wchar_t *tofilename(wchar_t *path) {
    wchar_t *p = wcsrchr(path, L'\\');
    if(p) return ++ p;
    else return path;
}

static unsigned long enum_process_module(unsigned int pid, wchar_t *modulename) {
    HANDLE hProcess;
    unsigned long queryaddr = 0;
    MEMORY_BASIC_INFORMATION mbi;
    MEMORY_SECTION_NAME SectionName;
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION+PROCESS_VM_READ, TRUE, pid);
    if(hProcess == NULL) {
        wprintf(L"[-]OpenProcess %d with error %u\n", pid, GetLastError());
        return 0;
    }
    while(queryaddr < 0x80000000) {
        if(YZwQueryVirtualMemory(hProcess, (PVOID)queryaddr, MemoryBasicInformation, &mbi, sizeof(mbi)) == YSUCCESS) {
            if(mbi.Type == MEM_IMAGE) {
                if(YZwQueryVirtualMemory(hProcess, (PVOID)queryaddr, MemorySectionName, &SectionName, sizeof(SectionName) - 2) == YSUCCESS) {
                    SectionName.SectionFileName.Buffer[SectionName.SectionFileName.Length] = L'\0';
                    if(wcsicmp(tofilename(SectionName.SectionFileName.Buffer), modulename) == 0) {
                        CloseHandle(hProcess);
                        return queryaddr;
                    }
                }
            }
            queryaddr += mbi.RegionSize;
        } else {
            queryaddr += 0x1000;
        }
    }
    CloseHandle(hProcess);
    return 0;
}

unsigned long enum_process(wchar_t *processname, wchar_t *modulename, int* pid)
{
    HANDLE hSnap;
    PROCESSENTRY32 pe32 = {0};
    unsigned long addr = 0;
    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(hSnap == INVALID_HANDLE_VALUE) {
        wprintf(L"[-]CreateToolhelp32Snapshot with error %u\n", GetLastError());
        return 0;
    }
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if(Process32First(hSnap, &pe32)) {
        do {
            if(wcsicmp(tofilename(pe32.szExeFile), processname) == 0) {
                addr = enum_process_module(pe32.th32ProcessID, modulename);
                if(addr != 0) {
                    if (pid != NULL){
                        *pid = pe32.th32ProcessID;
                    }
                    // wprintf(L"%u:%s:0x%.8x\n", pe32.th32ProcessID, modulename, addr);
                    return addr;
                }
            }
        } while(Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);
}

static int enable_debug_priv() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    LUID Luid;
    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        wprintf(L"[-] OpenProcessToken error with %u\n", GetLastError());
        return YERROR;
    }
    if(!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &Luid )) {
        wprintf(L"[-] LookupPrivilegeValue error with %u\n", GetLastError());
        CloseHandle(hToken);
        return YERROR;
    }
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Luid = Luid;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
 
    if(!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL)) {
        wprintf(L"[-] AdjustTokenPrivileges error with %u\n", GetLastError());
        CloseHandle(hToken);
        return YERROR;
    }
    return YSUCCESS;
}

HANDLE getWindowHandleByPid(int pid){
    HWND curr;
    HANDLE top = GetTopWindow(NULL);
    int toggle;
    while (top){
        if (GetWindowThreadProcessId(top, &curr) != 0 && pid == (int)curr)
            if (!GetParent(top) && IsWindowVisible(top)) break;
        top = GetWindow(top, GW_HWNDNEXT);
    }
    return top;
}

LPCVOID getAddressPointer(HANDLE handle, LPCVOID addr){
    LPCVOID point;
    ReadProcessMemory(handle, addr, &point, sizeof(point), NULL);
    return point;
}

float getFloatByPoint(HANDLE handle, LPCVOID addr){
    float f;
    ReadProcessMemory(handle, addr, &f, sizeof(f), NULL);
    return f;
}

float getIntByPoint(HANDLE handle, LPCVOID addr){
    int i;
    ReadProcessMemory(handle, addr, &i, sizeof(i), NULL);
    return i;
}

float getByteByPoint(HANDLE handle, LPCVOID addr){
    byte i;
    ReadProcessMemory(handle, addr, &i, sizeof(i), NULL);
    return i;
}

int initMatrixByPoint_4x4(HANDLE handle, LPCVOID addr, float* M){
    return ReadProcessMemory(handle, addr, M, sizeof(float)*4*4, NULL);
}

int drawRect(HDC windc, RECT *rect, int pen){
    FrameRect(windc, rect, GetStockObject(pen));
    return 0;
}

int drawRedRect(HDC windc, RECT *rect, HBRUSH brush){
    FrameRect(windc, rect, brush);
    return 0;
}

void getRect(HANDLE windc, RECT *rect){
    DwmGetWindowAttribute((HWND)windc, 9, rect, sizeof(RECT));
    // GetWindowRect(windc, rect);
}

// 太抽象了，常量一定要用常量的方式定义，否则不能用。
const int gapif  = 0x324;
const int gapsd  = 0x68;
const int baseif = 0x011544A0;
const int basesd = 0x0054BC5C;
HDC windc;
RECT rect;
RECT drawrect;
float L, T, R, B, Gx, Gy, Bx, By, By2, Px, Py, Pz, H, head, feet, wid, VieW;
int x1, y1, x2, y2;
struct Player {
    float x;
    float y;
    float z;
    float hp;
    int side;
} ;

int main(int argc, char const *argv[]) {
    if(enable_debug_priv() != YSUCCESS) {
        printf("error.\n");
        return 0;
    }
    int *pid;
    HANDLE camera,handle,basead,whandle;
    basead = (HANDLE)enum_process(L"cstrike.exe", L"cstrike.exe", &pid);
    camera = (HANDLE)enum_process(L"cstrike.exe", L"particleman.dll", NULL);
    whandle = getWindowHandleByPid(pid);
    handle = OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid);
    windc = GetDC(NULL);
    printf("pid: %d\n", pid);
    printf("handle: %d\n", handle);
    printf("handle_window: %d\n", whandle);
    printf("base_cstrike.exe:%x\n", basead);
    printf("base_particleman.dll:%x\n", camera);

    LPCVOID point_matrix;
    point_matrix = getAddressPointer(handle, (LPCVOID)(basead+0x00946830));
    printf("Matrix addr: %x\n", point_matrix);

    int i, j;
    float M[4][4];
    if (initMatrixByPoint_4x4(handle, (LPCVOID)((int)point_matrix), (float*)&M) != 0){
        printf("read Matrix ok.\n");
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 4; ++j){
                printf("%12.7f ", M[i][j]);
            }
            printf("\n");
        }
    }
    

    // 读取目标地址的坐标
    LPCVOID point_info, point_side;
    LPCVOID i_point, s_point;
    int infos[4] = { 0x3ac, 0x3ac+0x4, 0x3ac+0x8, 0x3ac+0x158 };
    int sides[1] = { 0x4e };
    int s;
    int myside;
    float f;
    point_info = getAddressPointer(handle, (LPCVOID)(basead+baseif));
    point_side = getAddressPointer(handle, (LPCVOID)(basead+basesd));
    struct Player P[16];
    int number;
    head = 25;
    feet = -30;
    H = 15;

    HBRUSH brush = CreateSolidBrush(RGB(200, 0, 0));
    while(1){
        // 不断获取窗口大小以兼容窗口移动
        getRect(whandle, &rect);
        L = (float)rect.left;
        T = (float)rect.top;
        R = (float)rect.right;
        B = (float)rect.bottom;
        Gx = (R-L)/2;
        Gy = (B-T)/2;

        // 获取所有人物坐标信息，hp信息，身份信息
        number = 0;
        for (int i = 0; i < 16; ++i) {
            // printf("player %2d ", i);
            for (int j = 0; j < 5; ++j) {
                if (j < 4) {
                    i_point = (int)(point_info + infos[j] + gapif*i);
                    f = getFloatByPoint(handle, (LPCVOID)i_point);
                    // printf("%12.5f ", f);
                    if (j == 0){ P[i].x = f; };
                    if (j == 1){ P[i].y = f; };
                    if (j == 2){ P[i].z = f; };
                    if (j == 3){ P[i].hp = f; };
                } else {
                    s_point = (int)(point_side + sides[j-4] + gapsd*i);
                    s = getByteByPoint(handle, (LPCVOID)s_point);
                    // printf("%4d\n", s);
                    if (s != 1 && s != 2){ break; };
                    if (j == 4){ P[i].side = s; };
                    if (i == 0 && j == 4){ myside = s; };
                }
            }
            number += 1;
        }

        // 获取镜头矩阵，用人物参数计算需要画的部分
        initMatrixByPoint_4x4(handle, (LPCVOID)((int)point_matrix), (float*)&M);
        for (i = 1; i < 16; ++i) {
            Px = P[i].x;
            Py = P[i].y;
            Pz = P[i].z;
            VieW = Px*M[0][3] + Py*M[1][3] + Pz*M[2][3] + M[3][3];
            VieW = 1 / VieW;
            Bx  = Gx + (Px*M[0][0] + Py*M[1][0] + Pz       *M[2][0] + M[3][0])*VieW*Gx;
            By  = Gy - (Px*M[0][1] + Py*M[1][1] + (Pz+head)*M[2][1] + M[3][1])*VieW*Gy;
            By2 = Gy - (Px*M[0][1] + Py*M[1][1] + (Pz+feet)*M[2][1] + M[3][1])*VieW*Gy;
            wid = abs(By - By2)*.25;
            drawrect.left = (int)(Bx-wid+L);
            drawrect.top = (int)(By+T+H);
            drawrect.right = (int)(Bx+wid+L);
            drawrect.bottom = (int)(By2+T+H);
            if (P[i].hp > 1){
                if (P[i].side == myside){
                    // drawRect(windc, &drawrect, BLACK_BRUSH);
                }else{
                    // 只画敌人
                    drawRedRect(windc, &drawrect, brush);
                }
            }
        // Sleep(100);
        }
    }
}
