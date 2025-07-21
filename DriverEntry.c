#include <ndis.h>
#include "Aq_utils.h"  // Custom header for AQtion defines

#pragma warning(disable:4100)  // Unreferenced params

NDIS_HANDLE g_DriverHandle = NULL;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NDIS_STATUS Status;
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS MChars;

    NdisZeroMemory(&MChars, sizeof(MChars));
    MChars.Header.Size = sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS);
    MChars.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
    MChars.MajorNdisVersion = 6;
    MChars.MinorNdisVersion = 30;  // NDIS 6.30 for Windows 10+
    MChars.MajorDriverVersion = 1;
    MChars.MinorDriverVersion = 0;

    MChars.InitializeHandlerEx = MiniportInitializeEx;
    MChars.HaltHandlerEx = MiniportHaltEx;
    MChars.UnloadHandler = MiniportUnload;
    MChars.OidRequestHandler = MiniportOidRequest;
    MChars.CancelOidRequestHandler = MiniportCancelOidRequest;
    MChars.SendNetBufferListsHandler = MiniportSendNetBufferLists;
    MChars.ReturnNetBufferListsHandler = MiniportReturnNetBufferLists;
    MChars.CancelSendHandler = MiniportCancelSend;
    MChars.DevicePnPEventNotifyHandler = MiniportDevicePnPEventNotify;
    MChars.ShutdownHandlerEx = MiniportShutdownEx;
    MChars.CheckForHangHandlerEx = MiniportCheckForHangEx;
    MChars.ResetHandlerEx = MiniportResetEx;
    MChars.PauseHandler = MiniportPause;
    MChars.RestartHandler = MiniportRestart;
    MChars.InterruptHandlerEx = MiniportInterrupt;
    MChars.DisableInterruptHandlerEx = MiniportDisableInterruptEx;
    MChars.EnableInterruptHandlerEx = MiniportEnableInterruptEx;
    MChars.MessageInterruptHandlerEx = MiniportMessageInterrupt;
    MChars.DisableMessageInterruptHandlerEx = MiniportDisableMessageInterruptEx;
    MChars.EnableMessageInterruptHandlerEx = MiniportEnableMessageInterruptEx;

    Status = NdisMRegisterMiniportDriver(DriverObject, RegistryPath, NULL, NULL, &g_DriverHandle, &MChars);
    if (Status != NDIS_STATUS_SUCCESS) {
        return STATUS_UNSUCCESSFUL;
    }
    return STATUS_SUCCESS;
}

VOID MiniportUnload(PDRIVER_OBJECT DriverObject) {
    if (g_DriverHandle != NULL) {
        NdisMDeregisterMiniportDriver(g_DriverHandle);
        g_DriverHandle = NULL;
    }
}
