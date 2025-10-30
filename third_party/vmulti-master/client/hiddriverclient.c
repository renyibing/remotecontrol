// hiddriver client implementation
// This version searches for HID devices by hardware ID pattern instead of VID/PID

#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmulticlient.h"

#if __GNUC__
    #define __in
    #define __in_ecount(x)
    typedef void* PVOID;
    typedef PVOID HDEVINFO;
    WINHIDSDI BOOL WINAPI HidD_SetOutputReport(HANDLE, PVOID, ULONG);
#endif

typedef struct _vmulti_client_t
{
    HANDLE hControl;
    BYTE controlReport[CONTROL_REPORT_SIZE];
} vmulti_client_t;

// Forward declarations
HANDLE SearchHIDDriverDevice(void);
HANDLE OpenDeviceInterface(HDEVINFO HardwareDeviceInfo, PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData);
BOOLEAN CheckIfCompatibleDevice(HANDLE file);
BOOL HidOutput(BOOL useSetOutputReport, HANDLE file, PCHAR buffer, ULONG bufferSize);

typedef struct _HID_DEVICE_ATTRIBUTES {
    ULONG           Size;
    USHORT          VendorID;
    USHORT          ProductID;
    USHORT          VersionNumber;
    USHORT          Reserved[11];
} HID_DEVICE_ATTRIBUTES, * PHID_DEVICE_ATTRIBUTES;

// Client functions
pvmulti_client vmulti_alloc(void)
{
    return (pvmulti_client)malloc(sizeof(vmulti_client_t));
}

void vmulti_free(pvmulti_client vmulti)
{
    free(vmulti);
}

BOOL vmulti_connect(pvmulti_client vmulti)
{
    vmulti_client_t* client = (vmulti_client_t*)vmulti;
    
    // Search for hiddriver device
    client->hControl = SearchHIDDriverDevice();
    if (client->hControl == INVALID_HANDLE_VALUE || client->hControl == NULL)
        return FALSE;

    return TRUE;
}

void vmulti_disconnect(pvmulti_client vmulti)
{
    vmulti_client_t* client = (vmulti_client_t*)vmulti;
    if (client->hControl != NULL)
        CloseHandle(client->hControl);
    client->hControl = NULL;
}

BOOL vmulti_update_mouse(pvmulti_client vmulti, BYTE button, USHORT x, USHORT y, BYTE wheelPosition)
{
    vmulti_client_t* client = (vmulti_client_t*)vmulti;
    VMultiControlReportHeader* pReport = NULL;
    VMultiMouseReport* pMouseReport = NULL;

    if (CONTROL_REPORT_SIZE <= sizeof(VMultiControlReportHeader) + sizeof(VMultiMouseReport))
    {
        return FALSE;
    }

    pReport = (VMultiControlReportHeader*)client->controlReport;
    pReport->ReportID = REPORTID_CONTROL;
    pReport->ReportLength = sizeof(VMultiMouseReport);

    pMouseReport = (VMultiMouseReport*)(client->controlReport + sizeof(VMultiControlReportHeader));
    pMouseReport->ReportID = REPORTID_MOUSE;
    pMouseReport->Button = button;
    pMouseReport->XValue = x;
    pMouseReport->YValue = y;
    pMouseReport->WheelPosition = wheelPosition;

    return HidOutput(FALSE, client->hControl, (PCHAR)client->controlReport, CONTROL_REPORT_SIZE);
}

BOOL vmulti_update_relative_mouse(pvmulti_client vmulti, BYTE button, BYTE x, BYTE y, BYTE wheelPosition)
{
    vmulti_client_t* client = (vmulti_client_t*)vmulti;
    VMultiControlReportHeader* pReport = NULL;
    VMultiRelativeMouseReport* pMouseReport = NULL;

    if (CONTROL_REPORT_SIZE <= sizeof(VMultiControlReportHeader) + sizeof(VMultiRelativeMouseReport))
    {
        return FALSE;
    }

    pReport = (VMultiControlReportHeader*)client->controlReport;
    pReport->ReportID = REPORTID_CONTROL;
    pReport->ReportLength = sizeof(VMultiRelativeMouseReport);

    pMouseReport = (VMultiRelativeMouseReport*)(client->controlReport + sizeof(VMultiControlReportHeader));
    pMouseReport->ReportID = REPORTID_RELATIVE_MOUSE;
    pMouseReport->Button = button;
    pMouseReport->XValue = x;
    pMouseReport->YValue = y;
    pMouseReport->WheelPosition = wheelPosition;

    return HidOutput(FALSE, client->hControl, (PCHAR)client->controlReport, CONTROL_REPORT_SIZE);
}

BOOL vmulti_update_keyboard(pvmulti_client vmulti, BYTE shiftKeyFlags, BYTE keyCodes[KBD_KEY_CODES])
{
    vmulti_client_t* client = (vmulti_client_t*)vmulti;
    VMultiControlReportHeader* pReport = NULL;
    VMultiKeyboardReport* pKeyboardReport = NULL;

    if (CONTROL_REPORT_SIZE <= sizeof(VMultiControlReportHeader) + sizeof(VMultiKeyboardReport))
    {
        return FALSE;
    }

    pReport = (VMultiControlReportHeader*)client->controlReport;
    pReport->ReportID = REPORTID_CONTROL;
    pReport->ReportLength = sizeof(VMultiKeyboardReport);

    pKeyboardReport = (VMultiKeyboardReport*)(client->controlReport + sizeof(VMultiControlReportHeader));
    pKeyboardReport->ReportID = REPORTID_KEYBOARD;
    pKeyboardReport->ShiftKeyFlags = shiftKeyFlags;
    memcpy(pKeyboardReport->KeyCodes, keyCodes, KBD_KEY_CODES);

    return HidOutput(FALSE, client->hControl, (PCHAR)client->controlReport, CONTROL_REPORT_SIZE);
}

// Helper functions
HANDLE SearchHIDDriverDevice(void)
{
    HDEVINFO hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    SP_DEVINFO_DATA devInfoData;
    GUID hidguid;
    int i;

    HidD_GetHidGuid(&hidguid);

    hardwareDeviceInfo = SetupDiGetClassDevs((LPGUID)&hidguid, NULL, NULL, (DIGCF_PRESENT | DIGCF_INTERFACEDEVICE));

    if (INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
        printf("SetupDiGetClassDevs failed: %x\n", GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    printf("\n....looking for hiddriver HID device\n");

    for (i = 0; SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, 0, (LPGUID)&hidguid, i, &deviceInterfaceData); i++)
    {
        HANDLE file = OpenDeviceInterface(hardwareDeviceInfo, &deviceInterfaceData);

        if (file != INVALID_HANDLE_VALUE)
        {
            SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
            printf("Success: Found hiddriver device\n");
            return file;
        }
    }

    printf("Failure: Could not find hiddriver device\n");
    SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
    return INVALID_HANDLE_VALUE;
}

HANDLE OpenDeviceInterface(HDEVINFO hardwareDeviceInfo, PSP_DEVICE_INTERFACE_DATA deviceInterfaceData)
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = NULL;
    DWORD predictedLength = 0;
    DWORD requiredLength = 0;
    HANDLE file = INVALID_HANDLE_VALUE;

    SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, deviceInterfaceData, NULL, 0, &requiredLength, NULL);

    predictedLength = requiredLength;
    deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(predictedLength);

    if (!deviceInterfaceDetailData)
    {
        goto cleanup;
    }

    deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    if (!SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, deviceInterfaceData, deviceInterfaceDetailData, predictedLength, &requiredLength, NULL))
    {
        free(deviceInterfaceDetailData);
        goto cleanup;
    }

    // Check if the device path contains "hiddriver" or "xrcloud"
    if (strstr(deviceInterfaceDetailData->DevicePath, "hiddriver") != NULL ||
        strstr(deviceInterfaceDetailData->DevicePath, "xrcloud") != NULL)
    {
        file = CreateFile(deviceInterfaceDetailData->DevicePath, 
                         GENERIC_READ | GENERIC_WRITE, 
                         FILE_SHARE_READ | FILE_SHARE_WRITE, 
                         NULL, 
                         OPEN_EXISTING, 
                         0, 
                         NULL);

        if (file != INVALID_HANDLE_VALUE)
        {
            if (CheckIfCompatibleDevice(file))
            {
                printf("Found compatible device: %s\n", deviceInterfaceDetailData->DevicePath);
                goto cleanup;
            }
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
        }
    }

cleanup:
    free(deviceInterfaceDetailData);
    return file;
}

BOOLEAN CheckIfCompatibleDevice(HANDLE file)
{
    PHIDP_PREPARSED_DATA ppd = NULL;
    HID_DEVICE_ATTRIBUTES attributes;
    HIDP_CAPS caps;
    BOOLEAN result = FALSE;

    if (!HidD_GetPreparsedData(file, &ppd))
    {
        goto cleanup;
    }

    if (!HidD_GetAttributes(file, &attributes))
    {
        goto cleanup;
    }

    if (!HidP_GetCaps(ppd, &caps))
    {
        goto cleanup;
    }

    // Check if device supports output reports (required for input injection)
    if (caps.NumberOutputDataIndices > 0 || caps.OutputReportByteLength > 0)
    {
        printf("Device VID:0x%04X PID:0x%04X OutputReportLen:%d\n", 
               attributes.VendorID, 
               attributes.ProductID,
               caps.OutputReportByteLength);
        result = TRUE;
    }

cleanup:
    if (ppd != NULL)
    {
        HidD_FreePreparsedData(ppd);
    }

    return result;
}

BOOL HidOutput(BOOL useSetOutputReport, HANDLE file, PCHAR buffer, ULONG bufferSize)
{
    ULONG bytesWritten;
    if (useSetOutputReport)
    {
        if (!HidD_SetOutputReport(file, buffer, bufferSize))
        {
            return FALSE;
        }
    }
    else
    {
        if (!WriteFile(file, buffer, bufferSize, &bytesWritten, NULL))
        {
            return FALSE;
        }
    }

    return TRUE;
}

