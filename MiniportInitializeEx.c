/*
 * MiniportInitializeEx.c - AQtion NDIS Miniport Initialization
 *
 * Called by NDIS when the adapter is first loaded. Performs:
 *   1. Adapter context allocation
 *   2. PCI config read (VID/DID, BAR0)
 *   3. BAR0 MMIO mapping
 *   4. Chip detection and FW version validation
 *   5. Soft reset (RBL/FLB detection)
 *   6. MAC address retrieval from eFuse via FW mailbox
 *   7. TX/RX DMA descriptor ring allocation
 *   8. RX data buffer pre-allocation
 *   9. MSI-X interrupt registration
 *  10. Miniport general/registration attributes
 *
 * Ported from Linux atlantic driver (GPL-2.0)
 * Copyright (C) 2014-2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 */

#include "Aq_utils.h"

#pragma warning(disable:4100)  /* unreferenced formal parameter */

/* ===================================================================
 * Internal Helpers (forward declarations)
 * =================================================================== */

static NDIS_STATUS AqMapPciResources(PAQ_ADAPTER Adapter,
                                     PNDIS_RESOURCE_LIST ResList);

static NDIS_STATUS AqDetectChip(PAQ_ADAPTER Adapter);
static NDIS_STATUS AqSoftReset(PAQ_ADAPTER Adapter);
static NDIS_STATUS AqReadMacAddress(PAQ_ADAPTER Adapter);
static NDIS_STATUS AqAllocDmaRings(PAQ_ADAPTER Adapter);
static VOID        AqFreeDmaRings(PAQ_ADAPTER Adapter);
static NDIS_STATUS AqAllocRxBuffers(PAQ_ADAPTER Adapter, UINT32 RingIdx);
static NDIS_STATUS AqRegisterInterrupt(PAQ_ADAPTER Adapter);
static NDIS_STATUS AqInitHardware(PAQ_ADAPTER Adapter);

/* FW mailbox DRAM read helper */
static NDIS_STATUS AqFwDownloadDwords(PAQ_ADAPTER Adapter,
                                      UINT32 FwAddr,
                                      UINT32 *Data,
                                      UINT32 Count);

/* ===================================================================
 * MiniportInitializeEx
 * =================================================================== */

NDIS_STATUS
MiniportInitializeEx(
    NDIS_HANDLE NdisMiniportHandle,
    NDIS_HANDLE MiniportDriverContext,
    PNDIS_MINIPORT_INIT_PARAMETERS InitParameters)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PAQ_ADAPTER Adapter = NULL;
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES RegAttrs;
    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES GenAttrs;
    NDIS_SG_DMA_DESCRIPTION DmaDesc;

    UNREFERENCED_PARAMETER(MiniportDriverContext);

    /* -----------------------------------------------------------------
     * 1. Allocate adapter context
     * ----------------------------------------------------------------- */

    Adapter = (PAQ_ADAPTER)NdisAllocateMemoryWithTagPriority(
        NdisMiniportHandle,
        sizeof(AQ_ADAPTER),
        'AQTX',
        NormalPoolPriority);

    if (Adapter == NULL) {
        Status = NDIS_STATUS_RESOURCES;
        goto fail;
    }

    NdisZeroMemory(Adapter, sizeof(AQ_ADAPTER));
    Adapter->AdapterHandle = NdisMiniportHandle;
    NdisAllocateSpinLock(&Adapter->Lock);

    /* Defaults */
    Adapter->Mtu = AQ_MTU_DEFAULT;
    Adapter->LinkSpeedMask = AQ_LINK_ALL;
    Adapter->NumTxRings = 1;
    Adapter->NumRxRings = 1;

    /* -----------------------------------------------------------------
     * 2. Set registration attributes (gives NDIS our context pointer)
     * ----------------------------------------------------------------- */

    NdisZeroMemory(&RegAttrs, sizeof(RegAttrs));
    RegAttrs.Header.Type =
        NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
    RegAttrs.Header.Size =
        NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;
    RegAttrs.Header.Revision =
        NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;
    RegAttrs.MiniportAdapterContext = (NDIS_HANDLE)Adapter;
    RegAttrs.AttributeFlags =
        NDIS_MINIPORT_ATTRIBUTES_HARDWARE_DEVICE |
        NDIS_MINIPORT_ATTRIBUTES_BUS_MASTER;
    RegAttrs.InterfaceType = NdisInterfacePci;

    Status = NdisMSetMiniportAttributes(
        NdisMiniportHandle,
        (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&RegAttrs);

    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* -----------------------------------------------------------------
     * 3. Map PCI resources (BAR0 MMIO)
     * ----------------------------------------------------------------- */

    Status = AqMapPciResources(
        Adapter,
        &InitParameters->AllocatedResources->List[0].PartialResourceList);

    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* -----------------------------------------------------------------
     * 4. Chip detection and FW version read
     * ----------------------------------------------------------------- */

    Status = AqDetectChip(Adapter);
    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* -----------------------------------------------------------------
     * 5. Soft reset
     * ----------------------------------------------------------------- */

    Status = AqSoftReset(Adapter);
    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* -----------------------------------------------------------------
     * 6. Read MAC address from eFuse via FW
     * ----------------------------------------------------------------- */

    Status = AqReadMacAddress(Adapter);
    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* -----------------------------------------------------------------
     * 7. Register DMA (scatter/gather for 64-bit addressing)
     * ----------------------------------------------------------------- */

    NdisZeroMemory(&DmaDesc, sizeof(DmaDesc));
    DmaDesc.Header.Type = NDIS_OBJECT_TYPE_SG_DMA_DESCRIPTION;
    DmaDesc.Header.Revision = NDIS_SG_DMA_DESCRIPTION_REVISION_1;
    DmaDesc.Header.Size = sizeof(NDIS_SG_DMA_DESCRIPTION);
    DmaDesc.Flags = NDIS_SG_DMA_64_BIT_ADDRESS;
    DmaDesc.MaximumPhysicalMapping = AQ_MTU_JUMBO + 14 + 4; /* eth frame */
    DmaDesc.ProcessSGListHandler = NULL; /* not using SG DMA path directly */
    DmaDesc.SharedMemAllocateCompleteHandler = NULL;

    Status = NdisMRegisterScatterGatherDma(
        NdisMiniportHandle, &DmaDesc, &Adapter->DmaHandle);

    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* -----------------------------------------------------------------
     * 8. Allocate TX/RX descriptor rings and RX buffers
     * ----------------------------------------------------------------- */

    Status = AqAllocDmaRings(Adapter);
    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* -----------------------------------------------------------------
     * 9. Register MSI-X interrupt
     * ----------------------------------------------------------------- */

    Status = AqRegisterInterrupt(Adapter);
    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* -----------------------------------------------------------------
     * 10. Initialize HW paths (TX/RX DMA engines, interrupts, link)
     * ----------------------------------------------------------------- */

    Status = AqInitHardware(Adapter);
    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* -----------------------------------------------------------------
     * 11. Set general attributes
     * ----------------------------------------------------------------- */

    NdisZeroMemory(&GenAttrs, sizeof(GenAttrs));
    GenAttrs.Header.Type =
        NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
    GenAttrs.Header.Size =
        NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_2;
    GenAttrs.Header.Revision =
        NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_2;

    GenAttrs.MediaType = NdisMedium802_3;
    GenAttrs.PhysicalMediumType = NdisPhysicalMedium802_3;
    GenAttrs.MtuSize = Adapter->Mtu;
    GenAttrs.MaxXmitLinkSpeed = 10000000000ULL;  /* 10 Gbps */
    GenAttrs.XmitLinkSpeed    = 10000000000ULL;
    GenAttrs.MaxRcvLinkSpeed  = 10000000000ULL;
    GenAttrs.RcvLinkSpeed     = 10000000000ULL;
    GenAttrs.MediaConnectState = MediaConnectStateDisconnected;
    GenAttrs.MediaDuplexState = MediaDuplexStateFull;
    GenAttrs.LookaheadSize = Adapter->Mtu;
    GenAttrs.MacOptions =
        NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
        NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
        NDIS_MAC_OPTION_NO_LOOPBACK;
    GenAttrs.SupportedPacketFilters =
        NDIS_PACKET_TYPE_DIRECTED      |
        NDIS_PACKET_TYPE_MULTICAST     |
        NDIS_PACKET_TYPE_ALL_MULTICAST |
        NDIS_PACKET_TYPE_BROADCAST     |
        NDIS_PACKET_TYPE_PROMISCUOUS;
    GenAttrs.MaxMulticastListSize = AQ_MAX_MULTICAST;
    GenAttrs.MacAddressLength = ETH_LENGTH_OF_ADDRESS;

    NdisMoveMemory(GenAttrs.PermanentMacAddress,
                   Adapter->PermanentMac, ETH_LENGTH_OF_ADDRESS);
    NdisMoveMemory(GenAttrs.CurrentMacAddress,
                   Adapter->CurrentMac, ETH_LENGTH_OF_ADDRESS);

    GenAttrs.RecvScaleCapabilities = NULL;  /* TODO: RSS caps */
    GenAttrs.AccessType = NET_IF_ACCESS_BROADCAST;
    GenAttrs.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
    GenAttrs.ConnectionType = NET_IF_CONNECTION_DEDICATED;
    GenAttrs.IfType = IF_TYPE_ETHERNET_CSMACD;
    GenAttrs.IfConnectorPresent = TRUE;
    GenAttrs.SupportedStatistics =
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV  |
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT |
        NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV             |
        NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT;
    GenAttrs.SupportedOidList = NULL;   /* TODO: OID table */
    GenAttrs.SupportedOidListLength = 0;

    Status = NdisMSetMiniportAttributes(
        NdisMiniportHandle,
        (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&GenAttrs);

    if (Status != NDIS_STATUS_SUCCESS) {
        goto fail;
    }

    /* Mark adapter as running */
    Adapter->Flags |= AQ_FLAG_STARTED;
    Adapter->Flags |= AQ_FLAG_LINK_DOWN;

    return NDIS_STATUS_SUCCESS;

fail:
    if (Adapter != NULL) {
        /* Unwind allocations */
        if (Adapter->Interrupt.InterruptHandle) {
            NdisMDeregisterInterruptEx(Adapter->Interrupt.InterruptHandle);
        }
        AqFreeDmaRings(Adapter);
        if (Adapter->DmaHandle) {
            NdisMDeregisterScatterGatherDma(Adapter->DmaHandle);
        }
        if (Adapter->IoBase) {
            NdisMUnmapIoSpace(NdisMiniportHandle,
                              Adapter->IoBase, Adapter->IoLength);
        }
        NdisFreeSpinLock(&Adapter->Lock);
        NdisFreeMemory(Adapter, sizeof(AQ_ADAPTER), 0);
    }
    return Status;
}


/* ===================================================================
 * AqMapPciResources - Walk CM_RESOURCE_LIST, map BAR0
 * =================================================================== */

static NDIS_STATUS
AqMapPciResources(PAQ_ADAPTER Adapter, PNDIS_RESOURCE_LIST ResList)
{
    UINT32 i;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc;

    for (i = 0; i < ResList->Count; i++) {
        Desc = &ResList->PartialDescriptors[i];

        if (Desc->Type == CmResourceTypeMemory) {
            /* First memory BAR is BAR0 (register space) */
            Adapter->IoPhysBase = Desc->u.Memory.Start;
            Adapter->IoLength = Desc->u.Memory.Length;

            NDIS_STATUS Status = NdisMMapIoSpace(
                &Adapter->IoBase,
                Adapter->AdapterHandle,
                Adapter->IoPhysBase,
                Adapter->IoLength);

            if (Status != NDIS_STATUS_SUCCESS) {
                return Status;
            }

            return NDIS_STATUS_SUCCESS;
        }
    }

    /* No memory BAR found */
    return NDIS_STATUS_RESOURCE_CONFLICT;
}


/* ===================================================================
 * AqDetectChip - Read MIF ID, determine chip revision, read FW ver
 * =================================================================== */

static NDIS_STATUS
AqDetectChip(PAQ_ADAPTER Adapter)
{
    UINT32 MifId;
    UINT32 FwVer;

    MifId = AqReadReg(Adapter, AQ_GLB_MIF_ID_ADR);

    /* Determine chip type from MIF ID register */
    Adapter->ChipFeatures = AQ_CHIP_ATLANTIC;

    if ((MifId & 0xFFFF) == 0x00) {
        /* Antigua variant */
        Adapter->ChipFeatures |= AQ_CHIP_ANTIGUA;
    }

    /* Read firmware version from MPI state register */
    FwVer = AqReadReg(Adapter, AQ_MPI_STATE_ADR);
    Adapter->FwMbox.FwVersion = FwVer;

    /* Read PCI vendor/device for validation */
    /* (populated via PCI config space in a full implementation;
     *  here we verify the chip is alive via MIF read) */
    if (MifId == 0xFFFFFFFF || MifId == 0x00000000) {
        /* Chip not responding - likely hot-unplug or BAR mapping failure */
        return NDIS_STATUS_ADAPTER_NOT_FOUND;
    }

    return NDIS_STATUS_SUCCESS;
}


/* ===================================================================
 * AqSoftReset - Reset chip, detect RBL vs FLB boot path
 *
 * Ported from hw_atl_utils_soft_reset():
 *   - Polls daisy chain status + boot exit code
 *   - Determines RBL (ROM bootloader) vs FLB (flash bootloader)
 *   - Issues global soft reset via appropriate path
 * =================================================================== */

/* Registers used by soft reset (from hw_atl_llh_internal.h) */
#define AQ_MPI_DAISY_CHAIN_STATUS  0x00000704
#define AQ_MPI_BOOT_EXIT_CODE      0x00000388
#define AQ_MIF_RESET_TIMEOUT       0x00000348

/* RBL reset path */
#define AQ_GLB_CTL2_ADR            0x00000404
#define AQ_GLB_CTL2_RESET_MSK      0x00000001

/* FLB reset path */
#define AQ_FLB_CTL_ADR             0x00000404
#define AQ_SEM_RESET1_ADR          AQ_GLB_CPU_SEM_ADR(0)
#define AQ_SEM_RESET2_ADR          AQ_GLB_CPU_SEM_ADR(1)

static NDIS_STATUS
AqSoftReset(PAQ_ADAPTER Adapter)
{
    UINT32 FlbStatus;
    UINT32 BootExitCode = 0;
    UINT32 k;

    /* Wait for FW to exit boot phase (up to ~100ms) */
    for (k = 0; k < 1000; k++) {
        FlbStatus = AqReadReg(Adapter, AQ_MPI_DAISY_CHAIN_STATUS);
        BootExitCode = AqReadReg(Adapter, AQ_MPI_BOOT_EXIT_CODE);

        if (FlbStatus != 0x06000000 || BootExitCode != 0) {
            break;
        }
        NdisStallExecution(100);  /* 100 us */
    }

    if (k == 1000) {
        /* Neither RBL nor FLB firmware started */
        return NDIS_STATUS_ADAPTER_NOT_READY;
    }

    Adapter->RblEnabled = (BootExitCode != 0) ? TRUE : FALSE;

    if (Adapter->RblEnabled) {
        /* RBL path: set reset bit, wait for FW to re-init */
        AqWriteReg(Adapter, AQ_GLB_CTL2_ADR, AQ_GLB_CTL2_RESET_MSK);
        NdisStallExecution(1000);

        /* Wait for boot exit code to appear again */
        for (k = 0; k < 2000; k++) {
            BootExitCode = AqReadReg(Adapter, AQ_MPI_BOOT_EXIT_CODE);
            if (BootExitCode != 0) {
                break;
            }
            NdisStallExecution(100);
        }
    } else {
        /* FLB path: assert global soft reset */
        AqWriteReg(Adapter, AQ_GLB_SOFT_RESET_ADR, AQ_GLB_SOFT_RESET_MSK);
        NdisStallExecution(50000);  /* 50ms for FW reload */

        /* Wait for FW to signal ready via semaphore */
        for (k = 0; k < 2000; k++) {
            UINT32 Sem = AqReadReg(Adapter, AQ_SEM_RESET1_ADR);
            if (Sem != 0) {
                break;
            }
            NdisStallExecution(100);
        }
    }

    /* Verify chip is alive after reset */
    if (AqReadReg(Adapter, AQ_GLB_MIF_ID_ADR) == 0xFFFFFFFF) {
        return NDIS_STATUS_HARD_ERRORS;
    }

    return NDIS_STATUS_SUCCESS;
}


/* ===================================================================
 * AqFwDownloadDwords - Read DWORDS from FW DRAM region
 *
 * The AQC10x FW exposes internal DRAM via a semaphore-gated
 * read interface. This is used to pull eFuse data (MAC addr, etc).
 *
 * Ported from hw_atl_utils_fw_downld_dwords()
 * =================================================================== */

#define AQ_FW_SEM_REG              AQ_GLB_CPU_SEM_ADR(AQ_FW_SEM_RAM)
#define AQ_FW_MBOX_IF_IN_CMD_ADR   0x00000200
#define AQ_FW_MBOX_IF_IN_ADDR_ADR  0x00000208
#define AQ_FW_MBOX_IF_IN_LEN_ADR   0x0000020C
#define AQ_FW_MBOX_IF_OUT_ADR      0x00000210

static NDIS_STATUS
AqFwDownloadDwords(PAQ_ADAPTER Adapter, UINT32 FwAddr,
                   UINT32 *Data, UINT32 Count)
{
    UINT32 k, i;

    /* Acquire FW semaphore (try for ~10ms) */
    for (k = 0; k < 100; k++) {
        if (AqReadReg(Adapter, AQ_FW_SEM_REG) == 1) {
            break;
        }
        NdisStallExecution(100);
    }
    if (k == 100) {
        return NDIS_STATUS_FAILURE;
    }

    /* Read each dword from FW DRAM */
    for (i = 0; i < Count; i++) {
        AqWriteReg(Adapter, AQ_FW_MBOX_IF_IN_ADDR_ADR,
                   FwAddr + (i * sizeof(UINT32)));
        AqWriteReg(Adapter, AQ_FW_MBOX_IF_IN_CMD_ADR, 0x00008000);
        AqFlush(Adapter);

        /* Wait for read completion */
        for (k = 0; k < 100; k++) {
            if ((AqReadReg(Adapter, AQ_FW_MBOX_IF_IN_CMD_ADR) & 0x100) != 0) {
                break;
            }
            NdisStallExecution(10);
        }

        Data[i] = AqReadReg(Adapter, AQ_FW_MBOX_IF_OUT_ADR);
    }

    /* Release semaphore */
    AqWriteReg(Adapter, AQ_FW_SEM_REG, 1);

    return NDIS_STATUS_SUCCESS;
}


/* ===================================================================
 * AqReadMacAddress - Get permanent MAC from eFuse via FW
 *
 * Ported from hw_atl_utils_get_mac_permanent():
 *   - Reads eFuse offset pointer from 0x374
 *   - Downloads 2 dwords at efuse + 40*4 (MAC location)
 *   - Byte-swaps to host order
 *   - Falls back to generated MAC if eFuse is empty/invalid
 * =================================================================== */

static NDIS_STATUS
AqReadMacAddress(PAQ_ADAPTER Adapter)
{
    UINT32 MacWords[2] = { 0, 0 };
    UINT32 EfuseAddr;
    UINT32 Ucp370;
    NDIS_STATUS Status;

    /* Ensure UCP 0x370 has a seed value (used for fallback MAC) */
    Ucp370 = AqReadReg(Adapter, AQ_UCP_0x370_REG);
    if (Ucp370 == 0) {
        /* Generate pseudo-random seed.
         * In production, use KeQueryPerformanceCounter or similar. */
        LARGE_INTEGER Tsc;
        NdisGetCurrentSystemTime(&Tsc);
        Ucp370 = 0x02020202 | (0xFEFEFEFE & Tsc.LowPart);
        AqWriteReg(Adapter, AQ_UCP_0x370_REG, Ucp370);
    }

    /* Read eFuse base address pointer */
    EfuseAddr = AqReadReg(Adapter, 0x00000374);

    /* MAC is at eFuse offset 40 dwords */
    Status = AqFwDownloadDwords(Adapter,
                                EfuseAddr + (40 * sizeof(UINT32)),
                                MacWords, 2);

    if (Status == NDIS_STATUS_SUCCESS) {
        /* Byte-swap from FW order (big-endian words) */
        MacWords[0] = RtlUlongByteSwap(MacWords[0]);
        MacWords[1] = RtlUlongByteSwap(MacWords[1]);
    } else {
        MacWords[0] = 0;
        MacWords[1] = 0;
    }

    NdisMoveMemory(Adapter->PermanentMac, (PUCHAR)MacWords,
                   ETH_LENGTH_OF_ADDRESS);

    /* Validate: must be unicast and non-zero */
    if ((Adapter->PermanentMac[0] & 0x01) ||
        (Adapter->PermanentMac[0] == 0 &&
         Adapter->PermanentMac[1] == 0 &&
         Adapter->PermanentMac[2] == 0)) {

        /* Generate fallback MAC from UCP register.
         * Format: 30:0E:XX:XX:XX:XX (aQuantia OUI-ish) */
        UINT32 L = 0xE3000000 |
                   (0xFFFF & AqReadReg(Adapter, AQ_UCP_0x370_REG));
        UINT32 H = 0x8001300E;

        Adapter->PermanentMac[0] = (UINT8)((H >> 8) & 0xFF);
        Adapter->PermanentMac[1] = (UINT8)(H & 0xFF);
        Adapter->PermanentMac[2] = (UINT8)((L >> 24) & 0xFF);
        Adapter->PermanentMac[3] = (UINT8)((L >> 16) & 0xFF);
        Adapter->PermanentMac[4] = (UINT8)((L >> 8) & 0xFF);
        Adapter->PermanentMac[5] = (UINT8)(L & 0xFF);
    }

    /* Set current MAC = permanent MAC initially */
    NdisMoveMemory(Adapter->CurrentMac, Adapter->PermanentMac,
                   ETH_LENGTH_OF_ADDRESS);

    return NDIS_STATUS_SUCCESS;
}


/* ===================================================================
 * AqAllocDmaRings - Allocate descriptor rings and buffer tracking
 *
 * For each TX/RX ring:
 *   - NdisMAllocateSharedMemory for descriptor ring (contiguous DMA)
 *   - Allocate NBL/VA/PA tracking arrays
 *   - For RX rings: pre-allocate data buffers
 * =================================================================== */

static NDIS_STATUS
AqAllocOneDmaRing(PAQ_ADAPTER Adapter, PAQ_DMA_RING Ring,
                  UINT32 NumDescs, UINT32 DxSize)
{
    ULONG RingBytes = NumDescs * DxSize;

    Ring->Size = NumDescs;
    Ring->DxSize = DxSize;
    Ring->Head = 0;
    Ring->Tail = 0;
    Ring->DescSize = RingBytes;

    /* Allocate descriptor ring (contiguous DMA-able memory) */
    NdisMAllocateSharedMemory(
        Adapter->AdapterHandle,
        RingBytes,
        TRUE,              /* cached */
        &Ring->DescVa,
        &Ring->DescPa);

    if (Ring->DescVa == NULL) {
        return NDIS_STATUS_RESOURCES;
    }

    NdisZeroMemory(Ring->DescVa, RingBytes);

    /* Allocate per-descriptor tracking arrays */
    Ring->NblArray = (PNET_BUFFER_LIST *)NdisAllocateMemoryWithTagPriority(
        Adapter->AdapterHandle,
        NumDescs * sizeof(PNET_BUFFER_LIST),
        'AQNB', NormalPoolPriority);

    Ring->BufVa = (PVOID *)NdisAllocateMemoryWithTagPriority(
        Adapter->AdapterHandle,
        NumDescs * sizeof(PVOID),
        'AQBV', NormalPoolPriority);

    Ring->BufPa = (NDIS_PHYSICAL_ADDRESS *)NdisAllocateMemoryWithTagPriority(
        Adapter->AdapterHandle,
        NumDescs * sizeof(NDIS_PHYSICAL_ADDRESS),
        'AQBP', NormalPoolPriority);

    if (!Ring->NblArray || !Ring->BufVa || !Ring->BufPa) {
        return NDIS_STATUS_RESOURCES;
    }

    NdisZeroMemory(Ring->NblArray, NumDescs * sizeof(PNET_BUFFER_LIST));
    NdisZeroMemory(Ring->BufVa, NumDescs * sizeof(PVOID));
    NdisZeroMemory(Ring->BufPa, NumDescs * sizeof(NDIS_PHYSICAL_ADDRESS));

    return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS
AqAllocDmaRings(PAQ_ADAPTER Adapter)
{
    NDIS_STATUS Status;
    UINT32 i;

    /* Allocate TX rings */
    for (i = 0; i < Adapter->NumTxRings; i++) {
        Status = AqAllocOneDmaRing(Adapter, &Adapter->TxRing[i],
                                   AQ_TX_RING_SIZE_DEFAULT, AQ_TXD_SIZE);
        if (Status != NDIS_STATUS_SUCCESS) {
            return Status;
        }
    }

    /* Allocate RX rings */
    for (i = 0; i < Adapter->NumRxRings; i++) {
        Status = AqAllocOneDmaRing(Adapter, &Adapter->RxRing[i],
                                   AQ_RX_RING_SIZE_DEFAULT, AQ_RXD_SIZE);
        if (Status != NDIS_STATUS_SUCCESS) {
            return Status;
        }

        /* Pre-allocate RX data buffers and fill descriptors */
        Status = AqAllocRxBuffers(Adapter, i);
        if (Status != NDIS_STATUS_SUCCESS) {
            return Status;
        }
    }

    return NDIS_STATUS_SUCCESS;
}


/* ===================================================================
 * AqAllocRxBuffers - Pre-fill RX ring with data buffers
 *
 * Each RX descriptor gets a 2K buffer. The HW DMA engine writes
 * received packets into these buffers and sets the DD bit in the
 * writeback descriptor.
 * =================================================================== */

static NDIS_STATUS
AqAllocRxBuffers(PAQ_ADAPTER Adapter, UINT32 RingIdx)
{
    PAQ_DMA_RING Ring = &Adapter->RxRing[RingIdx];
    UINT32 i;

    for (i = 0; i < Ring->Size; i++) {
        PAQ_RXD Rxd;
        NDIS_PHYSICAL_ADDRESS BufPa;
        PVOID BufVa;

        /* Allocate contiguous DMA buffer for packet data */
        NdisMAllocateSharedMemory(
            Adapter->AdapterHandle,
            AQ_RX_BUF_SIZE,
            TRUE,
            &BufVa,
            &BufPa);

        if (BufVa == NULL) {
            return NDIS_STATUS_RESOURCES;
        }

        Ring->BufVa[i] = BufVa;
        Ring->BufPa[i] = BufPa;

        /* Write buffer physical address into RX descriptor */
        Rxd = (PAQ_RXD)((PUCHAR)Ring->DescVa + (i * AQ_RXD_SIZE));
        Rxd->BufAddr = BufPa.QuadPart;
        Rxd->HdrAddr = 0;  /* no header split */
    }

    return NDIS_STATUS_SUCCESS;
}


/* ===================================================================
 * AqFreeDmaRings - Teardown (called from fail path and HaltEx)
 * =================================================================== */

static VOID
AqFreeOneDmaRing(PAQ_ADAPTER Adapter, PAQ_DMA_RING Ring, BOOLEAN IsRx)
{
    UINT32 i;

    /* Free RX data buffers */
    if (IsRx && Ring->BufVa) {
        for (i = 0; i < Ring->Size; i++) {
            if (Ring->BufVa[i]) {
                NdisMFreeSharedMemory(
                    Adapter->AdapterHandle,
                    AQ_RX_BUF_SIZE, TRUE,
                    Ring->BufVa[i], Ring->BufPa[i]);
                Ring->BufVa[i] = NULL;
            }
        }
    }

    /* Free tracking arrays */
    if (Ring->NblArray) {
        NdisFreeMemory(Ring->NblArray, 0, 0);
        Ring->NblArray = NULL;
    }
    if (Ring->BufVa) {
        NdisFreeMemory(Ring->BufVa, 0, 0);
        Ring->BufVa = NULL;
    }
    if (Ring->BufPa) {
        NdisFreeMemory(Ring->BufPa, 0, 0);
        Ring->BufPa = NULL;
    }

    /* Free descriptor ring */
    if (Ring->DescVa) {
        NdisMFreeSharedMemory(
            Adapter->AdapterHandle,
            Ring->DescSize, TRUE,
            Ring->DescVa, Ring->DescPa);
        Ring->DescVa = NULL;
    }
}

static VOID
AqFreeDmaRings(PAQ_ADAPTER Adapter)
{
    UINT32 i;

    for (i = 0; i < Adapter->NumTxRings; i++) {
        AqFreeOneDmaRing(Adapter, &Adapter->TxRing[i], FALSE);
    }
    for (i = 0; i < Adapter->NumRxRings; i++) {
        AqFreeOneDmaRing(Adapter, &Adapter->RxRing[i], TRUE);
    }
}


/* ===================================================================
 * AqRegisterInterrupt - Register MSI-X (fallback to MSI/line)
 * =================================================================== */

static NDIS_STATUS
AqRegisterInterrupt(PAQ_ADAPTER Adapter)
{
    NDIS_MINIPORT_INTERRUPT_CHARACTERISTICS IntChars;
    NDIS_STATUS Status;

    NdisZeroMemory(&IntChars, sizeof(IntChars));

    IntChars.Header.Type =
        NDIS_OBJECT_TYPE_MINIPORT_INTERRUPT;
    IntChars.Header.Revision =
        NDIS_MINIPORT_INTERRUPT_REVISION_1;
    IntChars.Header.Size =
        sizeof(NDIS_MINIPORT_INTERRUPT_CHARACTERISTICS);

    IntChars.InterruptHandler = MiniportInterrupt;
    IntChars.InterruptDpcHandler = NULL;  /* TODO: DPC handler */
    IntChars.DisableInterruptHandler = MiniportDisableInterruptEx;
    IntChars.EnableInterruptHandler = MiniportEnableInterruptEx;

    /* Request MSI-X */
    IntChars.MsiSupported = TRUE;
    IntChars.MsiSyncWithAllMessages = TRUE;
    IntChars.MessageInterruptHandler = MiniportMessageInterrupt;
    IntChars.MessageInterruptDpcHandler = NULL;  /* TODO */
    IntChars.DisableMessageInterruptHandler = MiniportDisableMessageInterruptEx;
    IntChars.EnableMessageInterruptHandler = MiniportEnableMessageInterruptEx;

    Status = NdisMRegisterInterruptEx(
        Adapter->AdapterHandle,
        Adapter,
        &IntChars,
        &Adapter->Interrupt.InterruptHandle);

    if (Status == NDIS_STATUS_SUCCESS) {
        Adapter->Interrupt.MsiXEnabled = TRUE;
    }

    return Status;
}


/* ===================================================================
 * AqInitHardware - Program TX/RX DMA engines, set link speed,
 *                  enable interrupts
 *
 * Ported from hw_atl_b0_hw_init() + hw_atl_b0_hw_start()
 * =================================================================== */

static NDIS_STATUS
AqInitHardware(PAQ_ADAPTER Adapter)
{
    UINT32 i;

    /* ---- TX Path ---- */
    for (i = 0; i < Adapter->NumTxRings; i++) {
        PAQ_DMA_RING Ring = &Adapter->TxRing[i];

        /* Program descriptor ring base address (64-bit) */
        AqWriteReg(Adapter, AQ_TX_DMA_DESC_BASE_LSW(i),
                   Ring->DescPa.LowPart);
        AqWriteReg(Adapter, AQ_TX_DMA_DESC_BASE_MSW(i),
                   Ring->DescPa.HighPart);

        /* Set ring length (in 8-descriptor units) and enable */
        AqWriteRegField(Adapter, AQ_TX_DMA_DESC_CTL(i),
                        AQ_TX_DESC_LEN_MSK, AQ_TX_DESC_LEN_SHIFT,
                        Ring->Size / AQ_TXD_MULTIPLE);

        AqWriteRegField(Adapter, AQ_TX_DMA_DESC_CTL(i),
                        AQ_TX_DESC_EN_MSK, AQ_TX_DESC_EN_SHIFT, 1);
    }

    /* ---- RX Path ---- */
    for (i = 0; i < Adapter->NumRxRings; i++) {
        PAQ_DMA_RING Ring = &Adapter->RxRing[i];

        /* Program descriptor ring base address (64-bit) */
        AqWriteReg(Adapter, AQ_RX_DMA_DESC_BASE_LSW(i),
                   Ring->DescPa.LowPart);
        AqWriteReg(Adapter, AQ_RX_DMA_DESC_BASE_MSW(i),
                   Ring->DescPa.HighPart);

        /* Set ring length and enable */
        AqWriteRegField(Adapter, AQ_RX_DMA_DESC_CTL(i),
                        AQ_RX_DESC_LEN_MSK, AQ_RX_DESC_LEN_SHIFT,
                        Ring->Size / AQ_RXD_MULTIPLE);

        AqWriteRegField(Adapter, AQ_RX_DMA_DESC_CTL(i),
                        AQ_RX_DESC_EN_MSK, AQ_RX_DESC_EN_SHIFT, 1);

        /* Advance tail pointer so HW knows all descriptors are available */
        AqWriteReg(Adapter, AQ_RX_DMA_DESC_TAIL(i), Ring->Size - 1);
    }

    /* ---- TX DMA total request limit (from B0 init) ---- */
    AqWriteReg(Adapter, AQ_TX_DMA_TOTAL_REQ_LIMIT, 24);

    /* ---- Set link speed via MPI control register ---- */
    {
        UINT32 MpiCtl = AqReadReg(Adapter, AQ_MPI_CONTROL_ADR);
        MpiCtl &= ~(AQ_MPI_SPEED_MSK << AQ_MPI_SPEED_SHIFT);
        MpiCtl |= (Adapter->LinkSpeedMask & AQ_MPI_SPEED_MSK)
                   << AQ_MPI_SPEED_SHIFT;
        AqWriteReg(Adapter, AQ_MPI_CONTROL_ADR, MpiCtl);
    }

    /* ---- Enable interrupts ---- */
    /* Global interrupt config for MSI-X: auto-mask, per-vector */
    AqWriteReg(Adapter, 0x00002300, 0x00000000);  /* clear ITR reset */

    /* Set interrupt auto-mask for all vectors */
    AqWriteReg(Adapter, AQ_ITR_IAMR_ADR, AQ_INT_MASK_ALL);

    /* Unmask all active interrupts */
    AqWriteReg(Adapter, AQ_ITR_IMCR_ADR, 0x00000000);
    AqWriteReg(Adapter, AQ_ITR_IMSR_ADR, AQ_INT_MASK_ALL);

    AqFlush(Adapter);

    /* ---- Enable TX/RX packet buffer engines (hw_start) ---- */
    /* TPB TX buffer enable: register 0x7900, bit 0 */
    AqWriteRegField(Adapter, 0x00007900, 0x00000001, 0, 1);
    /* RPB RX buffer enable: register 0x5700, bit 0 */
    AqWriteRegField(Adapter, 0x00005700, 0x00000001, 0, 1);

    AqFlush(Adapter);

    return NDIS_STATUS_SUCCESS;
}
