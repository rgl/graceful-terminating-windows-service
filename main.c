/* a graceful terminating windows service.
*
* Copyright (c) 2017, Rui Lopes (ruilopes.com)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*   * Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*   * Neither the name of Redis nor the names of its contributors may be used
*     to endorse or promote products derived from this software without
*     specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#define _WIN32_WINNT 0x0600 // Windows Server 2008+
#define WINVER _WIN32_WINNT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <io.h>
#ifndef SERVICE_CONTROL_PRESHUTDOWN
#define SERVICE_CONTROL_PRESHUTDOWN 0x0000000F
#endif
#ifndef SERVICE_ACCEPT_PRESHUTDOWN
#define SERVICE_ACCEPT_PRESHUTDOWN 0x00000100
#endif

void LOG(const char *format, ...) {
    time_t t;
    time(&t);
    char buffer[128];
    strftime(buffer, 128, "%Y-%m-%d %H:%M:%S ", localtime(&t));
    int l = strlen(buffer);
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer+l, 128-l, format, args);
    va_end(args);
    puts(buffer);
    l = strlen(buffer);
    buffer[l++] = '\n';
    buffer[l] = 0;
    FILE *log = fopen("graceful-terminating-windows-service.log", "a+");
    fputs(buffer, log);
    fclose(log);
}

HANDLE g_stopEvent = NULL;
HANDLE g_serviceControlHandlerHandle = NULL;
BOOL g_registerOnPreShutdown = FALSE; // register to receive SERVICE_CONTROL_PRESHUTDOWN or SERVICE_CONTROL_SHUTDOWN (default).
int g_n = 10;

DWORD signalStop() {
    SERVICE_STATUS serviceStatus = {
        .dwServiceType = SERVICE_WIN32_OWN_PROCESS,
        .dwCurrentState = SERVICE_STOP_PENDING,
    };
    if (!SetServiceStatus(g_serviceControlHandlerHandle, &serviceStatus)) {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        LOG("ERROR: Failed to SetServiceStatus Stop Pending with HREAULT 0x%x", hr);
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
    SetEvent(g_stopEvent);
    return NO_ERROR;
}

// see HandlerEx callback function at https://msdn.microsoft.com/en-us/library/windows/desktop/ms683241(v=vs.85).aspx
DWORD WINAPI serviceControlHandler(DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext) {
    switch (dwCtrl) {
        case SERVICE_CONTROL_PRESHUTDOWN:
            LOG("serviceControlHandler SERVICE_CONTROL_PRESHUTDOWN");
            return signalStop();

        case SERVICE_CONTROL_SHUTDOWN:
            LOG("serviceControlHandler SERVICE_CONTROL_SHUTDOWN");
            return signalStop();

        case SERVICE_CONTROL_STOP:
            LOG("serviceControlHandler SERVICE_CONTROL_STOP");
            return signalStop();

        case SERVICE_CONTROL_INTERROGATE:
            LOG("serviceControlHandler SERVICE_CONTROL_INTERROGATE");
            return NO_ERROR;

        default:
            LOG("serviceControlHandler 0x%x", dwCtrl);
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

VOID WINAPI serviceMain(DWORD dwArgc, LPTSTR *lpszArgv) {
    g_serviceControlHandlerHandle = RegisterServiceCtrlHandlerEx(
        L"graceful-terminating-windows-service",
        serviceControlHandler,
        NULL);
    if (!g_serviceControlHandlerHandle) {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        LOG("ERROR: Failed to RegisterServiceCtrlHandler with HREAULT 0x%x", hr);
        return;
    }

    SERVICE_STATUS serviceStatus = {
        .dwServiceType = SERVICE_WIN32_OWN_PROCESS,
        .dwCurrentState = SERVICE_RUNNING,
        .dwControlsAccepted = (g_registerOnPreShutdown ? SERVICE_ACCEPT_PRESHUTDOWN : SERVICE_ACCEPT_SHUTDOWN) | SERVICE_ACCEPT_STOP,
    };
    if (!SetServiceStatus(g_serviceControlHandlerHandle, &serviceStatus)) {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        LOG("ERROR: Failed to SetServiceStatus Running with HREAULT 0x%x", hr);
        return;
    }

    if (WAIT_OBJECT_0 != WaitForSingleObject(g_stopEvent, INFINITE)) {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        LOG("ERROR: Failed to wait for the stop event with HREAULT 0x%x", hr);
        goto stop;
    }

    for (int n = g_n; n; --n) {
        LOG("Gracefully terminating the application in T-%d...", n);
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        serviceStatus.dwControlsAccepted = 0;
        serviceStatus.dwWaitHint = n*1000;
        ++serviceStatus.dwCheckPoint;
        if (!SetServiceStatus(g_serviceControlHandlerHandle, &serviceStatus)) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            LOG("ERROR: Failed to SetServiceStatus Stop Pending T-%d with HREAULT 0x%x", n, hr);
        }
        Sleep(1000);
    }

stop:
    serviceStatus.dwCurrentState = SERVICE_STOPPED;
    serviceStatus.dwControlsAccepted = 0;
    serviceStatus.dwWaitHint = 0;
    serviceStatus.dwCheckPoint = 0;
    if (!SetServiceStatus(g_serviceControlHandlerHandle, &serviceStatus)) {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        LOG("ERROR: Failed to SetServiceStatus Stop Pending with HREAULT 0x%x", hr);
    }
}

int wmain(int argc, wchar_t *argv[]) {
    HRESULT hr = S_OK;
    g_n = argc >= 2 ? _wtoi(argv[1]) : 10;
    if (argc >= 3) {
        if (!SetCurrentDirectory(argv[2])) {
            return 9;
        }
    }
    if (argc >= 4) {
        g_registerOnPreShutdown = wcscmp(argv[3], L"t") == 0;
    }

    LOG("Running (pid=%d)...", GetCurrentProcessId());

    g_stopEvent = CreateEvent(
        NULL,	// default security attributes
        TRUE, 	// manual-reset event
        FALSE,  // non-signaled initial state
        NULL);  // event name
    if (g_stopEvent == NULL) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        LOG("ERROR: Failed to create stop event with HRESULT 0x%x", hr);
        goto cleanup;
    }

    SERVICE_TABLE_ENTRY serviceTable[] = {
        { L"graceful-terminating-windows-service", (LPSERVICE_MAIN_FUNCTION)serviceMain },
        { NULL, NULL }
    };

    // run the service and block until it exits.
    if (!StartServiceCtrlDispatcher(serviceTable)) {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        LOG("ERROR: Failed to StartServiceCtrlDispatcher with HREAULT 0x%x", hr);
    }

    LOG("Bye bye...");

cleanup:
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
    }
    return hr;
}
