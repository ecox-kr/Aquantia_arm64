/*
 * Aq_utils.h - AQtion NDIS Miniport Driver Definitions
 *
 * Ported from Linux atlantic driver (GPL-2.0)
 * Copyright (C) 2014-2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * Windows ARM64 NDIS port - POC
 */

#ifndef AQ_UTILS_H
#define AQ_UTILS_H

#include <ndis.h>

#pragma warning(disable:4200)  // zero-length arrays
#pragma warning(disable:4201)  // nameless struct/union

/* ===================================================================
 * PCI Identification
 * =================================================================== */

#define AQ_PCI_VENDOR_ID            0x1D6A   /* Marvell/aQuantia */
#define AQ_PCI_DEVICE_ID_AQC107     0x00B1   /* AQC107 10GbE */
#define AQ_PCI_DEVICE_ID_AQC108     0x11B1   /* AQC108 5GbE */
#define AQ_PCI_DEVICE_ID_AQC109     0x12B1   /* AQC109 2.5GbE */
#define AQ_PCI_DEVICE_ID_AQC100     0x00C0   /* AQC100 10GbE (Antigua) */
#define AQ_PCI_DEVICE_ID_AQC107S    0x07B1   /* AQC107S */
#define AQ_PCI_DEVICE_ID_AQC108S    0x87B1   /* AQC108S */
#define AQ_PCI_DEVICE_ID_AQC109S    0x88B1   /* AQC109S */
#define AQ_PCI_DEVICE_ID_AQC111     0x08B1   /* AQC111 5GbE */
#define AQ_PCI_DEVICE_ID_AQC112     0x09B1   /* AQC112 2.5GbE */

#define AQ_PCI_SUBSYS_ORICO_REA10   0x0001   /* ORICO REA-10 TB3 adapter */

/* ===================================================================
 * Chip Revision & Feature Flags
 * =================================================================== */

#define AQ_CHIP_REVISION_B0         0xA0
#define AQ_CHIP_REVISION_B1         0xA1
#define AQ_CHIP_ATLANTIC            0x00800000
#define AQ_CHIP_ANTIGUA             0x08000000

#define AQ_FW_VER_EXPECTED          0x01050006

/* ===================================================================
 * Ring / DMA Configuration Constants
 * =================================================================== */

#define AQ_TX_RINGS_MAX             4
#define AQ_RX_RINGS_MAX             4
#define AQ_RINGS_MAX                32

#define AQ_TX_RING_SIZE_DEFAULT     1024
#define AQ_RX_RING_SIZE_DEFAULT     1024
#define AQ_TX_RING_SIZE_MIN         32
#define AQ_TX_RING_SIZE_MAX         8184
#define AQ_RX_RING_SIZE_MIN         32
#define AQ_RX_RING_SIZE_MAX         8184

#define AQ_TXD_SIZE                 16   /* bytes per TX descriptor */
#define AQ_RXD_SIZE                 16   /* bytes per RX descriptor */
#define AQ_TXD_ALIGN                8
#define AQ_RXD_ALIGN                8
#define AQ_TXD_MULTIPLE             8
#define AQ_RXD_MULTIPLE             8

#define AQ_MTU_DEFAULT              1514
#define AQ_MTU_JUMBO                16352
#define AQ_RX_BUF_SIZE              2048
#define AQ_MAX_MULTICAST            32

#define AQ_TXBUF_MAX                160
#define AQ_RXBUF_MAX                320

/* RSS */
#define AQ_RSS_MAX                  8
#define AQ_RSS_KEY_SIZE             40   /* 320 bits */
#define AQ_RSS_IND_TABLE_SIZE       64

/* ===================================================================
 * Link Speed Masks
 * =================================================================== */

#define AQ_LINK_10M                 0x0001
#define AQ_LINK_100M                0x0002
#define AQ_LINK_1G                  0x0004
#define AQ_LINK_2G5                 0x0008
#define AQ_LINK_5G                  0x0010
#define AQ_LINK_10G                 0x0020
#define AQ_LINK_ALL                 0x003F

/* ===================================================================
 * Interrupt Registers
 * =================================================================== */

#define AQ_ITR_ISR_ADR              0x00002000   /* Interrupt Status */
#define AQ_ITR_ISCR_ADR             0x00002050   /* Interrupt Status Clear */
#define AQ_ITR_IMSR_ADR             0x00002060   /* Interrupt Mask Set */
#define AQ_ITR_IMCR_ADR             0x00002070   /* Interrupt Mask Clear */
#define AQ_ITR_IAMR_ADR             0x00002090   /* Interrupt Auto-Mask */

#define AQ_ITR_RESET_ADR            0x00002300
#define AQ_ITR_RESET_MSK            0x80000000

/* Interrupt moderation per-queue */
#define AQ_RX_INTR_MOD_CTL_ADR(q)  (0x00005A40 + (q) * 0x4)
#define AQ_TX_INTR_MOD_CTL_ADR(q)  (0x00008980 + (q) * 0x4)

#define AQ_INTR_MODER_MAX           0x1FF
#define AQ_INTR_MODER_MIN           0xFF

#define AQ_INT_MASK_ALL             0xFFFFFFFF

/* ===================================================================
 * Global / Misc Registers
 * =================================================================== */

#define AQ_GLB_MIF_ID_ADR           0x0000001C   /* MIF identification */
#define AQ_GLB_CPU_SEM_ADR(s)       (0x000003A0 + (s) * 0x4)  /* semaphore */
#define AQ_FW_SEM_RAM               0x2

#define AQ_GLB_SOFT_RESET_ADR       0x00000000
#define AQ_GLB_SOFT_RESET_MSK       0x00008000

/* MPI (firmware mailbox) */
#define AQ_MPI_CONTROL_ADR          0x00000368
#define AQ_MPI_STATE_ADR            0x0000036C
#define AQ_MPI_SPEED_MSK            0xFFFF
#define AQ_MPI_SPEED_SHIFT          16

#define AQ_FW_MBOX_ADDR_ADR         0x00000360
#define AQ_UCP_0x370_REG            0x00000370

/* PCI */
#define AQ_PCI_REG_RES_DSBL_ADR     0x00001000

/* ===================================================================
 * RX DMA Descriptor Ring Registers
 * Base: 0x5B00, stride: 0x20, range: [0..31]
 * =================================================================== */

#define AQ_RX_DMA_DESC_BASE_LSW(d)  (0x00005B00 + (d) * 0x20)
#define AQ_RX_DMA_DESC_BASE_MSW(d)  (0x00005B04 + (d) * 0x20)
#define AQ_RX_DMA_DESC_CTL(d)       (0x00005B08 + (d) * 0x20)
#define AQ_RX_DMA_DESC_HEAD(d)      (0x00005B0C + (d) * 0x20)
#define AQ_RX_DMA_DESC_TAIL(d)      (0x00005B10 + (d) * 0x20)
#define AQ_RX_DMA_DESC_STAT(d)      (0x00005B14 + (d) * 0x20)

/* RX descriptor control bits */
#define AQ_RX_DESC_EN_MSK           0x80000000   /* bit 31 = enable */
#define AQ_RX_DESC_EN_SHIFT         31
#define AQ_RX_DESC_LEN_MSK          0x00001FF8   /* bits 12:3 = ring length */
#define AQ_RX_DESC_LEN_SHIFT        3
#define AQ_RX_DESC_RESET_MSK        0x02000000   /* bit 25 = reset */
#define AQ_RX_DESC_RESET_SHIFT      25

/* RX DMA cache init */
#define AQ_RX_DMA_DESC_CACHE_INIT_ADR       0x00005A00
#define AQ_RX_DMA_DESC_CACHE_INIT_DONE_ADR  0x00005A10

/* RX packet buffer */
#define AQ_RPB_DMA_SYS_LBK_ADR     0x00005000
#define AQ_RPB_DMA_DROP_PKT_ADR     0x00006818

/* ===================================================================
 * TX DMA Descriptor Ring Registers
 * Base: 0x7C00, stride: 0x40, range: [0..31]
 * =================================================================== */

#define AQ_TX_DMA_DESC_BASE_LSW(d)  (0x00007C00 + (d) * 0x40)
#define AQ_TX_DMA_DESC_BASE_MSW(d)  (0x00007C04 + (d) * 0x40)
#define AQ_TX_DMA_DESC_CTL(d)       (0x00007C08 + (d) * 0x40)
#define AQ_TX_DMA_DESC_HEAD(d)      (0x00007C0C + (d) * 0x40)
#define AQ_TX_DMA_DESC_TAIL(d)      (0x00007C10 + (d) * 0x40)

/* TX descriptor control bits */
#define AQ_TX_DESC_EN_MSK           0x80000000
#define AQ_TX_DESC_EN_SHIFT         31
#define AQ_TX_DESC_LEN_MSK          0x00001FF8
#define AQ_TX_DESC_LEN_SHIFT        3
#define AQ_TX_DESC_WRB_THRESH_MSK   0x00007F00
#define AQ_TX_DESC_WRB_THRESH_SHIFT 8

#define AQ_TX_DMA_TOTAL_REQ_LIMIT   0x00007B20

/* ===================================================================
 * DMA Statistics Registers
 * =================================================================== */

#define AQ_STATS_RX_DMA_PKT_LSW    0x00006800
#define AQ_STATS_RX_DMA_PKT_MSW    0x00006804
#define AQ_STATS_RX_DMA_OCT_LSW    0x00006808
#define AQ_STATS_RX_DMA_OCT_MSW    0x0000680C
#define AQ_STATS_TX_DMA_PKT_LSW    0x00008800
#define AQ_STATS_TX_DMA_PKT_MSW    0x00008804
#define AQ_STATS_TX_DMA_OCT_LSW    0x00008808
#define AQ_STATS_TX_DMA_OCT_MSW    0x0000880C

/* MAC MSM counters */
#define AQ_MAC_RX_UCST_FRM_ADR     0x000000E0
#define AQ_MAC_RX_MCST_FRM_ADR     0x000000E8
#define AQ_MAC_RX_BCST_FRM_ADR     0x000000F0
#define AQ_MAC_TX_UCST_FRM_ADR     0x00000108
#define AQ_MAC_TX_MCST_FRM_ADR     0x00000110
#define AQ_MAC_RX_ERRS_ADR         0x00000120

/* Temperature */
#define AQ_TS_CONTROL_ADR           0x00003100
#define AQ_TS_DATA_ADR              0x00003120

/* ===================================================================
 * TX Descriptor Bit Definitions
 * =================================================================== */

/* TX data descriptor (TXD) ctl field */
#define AQ_TXD_CTL_TYPE_TXD         0x00000001
#define AQ_TXD_CTL_TYPE_TXC         0x00000002
#define AQ_TXD_CTL_BLEN_MSK         0x000FFFF0   /* buffer length */
#define AQ_TXD_CTL_BLEN_SHIFT       4
#define AQ_TXD_CTL_DD               0x00100000   /* descriptor done */
#define AQ_TXD_CTL_EOP              0x00200000   /* end of packet */

/* TX command bits in ctl field */
#define AQ_TXD_CTL_CMD_VLAN         (1 << 22)
#define AQ_TXD_CTL_CMD_FCS          (1 << 23)
#define AQ_TXD_CTL_CMD_IPCSO        (1 << 24)   /* IP checksum offload */
#define AQ_TXD_CTL_CMD_TUCSO        (1 << 25)   /* TCP/UDP csum offload */
#define AQ_TXD_CTL_CMD_LSO          (1 << 26)   /* large send offload */
#define AQ_TXD_CTL_CMD_WB           (1 << 27)   /* writeback */
#define AQ_TXD_CTL_CMD_VXLAN        (1 << 28)

/* TX ctl2 field */
#define AQ_TXD_CTL2_LEN_MSK         0xFFFFC000   /* payload length */
#define AQ_TXD_CTL2_LEN_SHIFT       14
#define AQ_TXD_CTL2_CTX_EN          0x00002000
#define AQ_TXD_CTL2_CTX_IDX         0x00001000

/* ===================================================================
 * RX Descriptor Bit Definitions (Writeback)
 * =================================================================== */

#define AQ_RXD_DD                   0x0001
#define AQ_RXD_WB_STAT2_DD         0x0001
#define AQ_RXD_WB_STAT2_EOP        0x0002
#define AQ_RXD_WB_STAT2_MACERR     0x0004
#define AQ_RXD_WB_STAT2_IP4ERR     0x0008
#define AQ_RXD_WB_STAT2_TCPUDPERR  0x0010
#define AQ_RXD_WB_STAT2_RXSTAT     0x003C
#define AQ_RXD_WB_STAT2_RSCCNT     0xF000

#define AQ_RXD_WB_STAT_RSSTYPE_MSK  0x0000000F
#define AQ_RXD_WB_STAT_PKTTYPE_MSK  0x00000FF0
#define AQ_RXD_WB_STAT_PKTTYPE_SHIFT 4
#define AQ_RXD_WB_STAT_HDRLEN_MSK   0xFFC00000
#define AQ_RXD_WB_STAT_HDRLEN_SHIFT  22
#define AQ_RXD_WB_PKTTYPE_VLAN      (1 << 5)

/* ===================================================================
 * Hardware Descriptor Structures (16 bytes each)
 * Packed to match HW layout exactly
 * =================================================================== */

#include <pshpack1.h>

/* TX data descriptor - written by driver, read by HW */
typedef struct _AQ_TXD {
    UINT64  BufAddr;        /* physical address of packet data */
    UINT32  Ctl;            /* type, buffer len, DD, EOP, commands */
    UINT32  Ctl2;           /* payload len, context enable/index */
} AQ_TXD, *PAQ_TXD;

C_ASSERT(sizeof(AQ_TXD) == 16);

/* TX context descriptor - used for offload metadata */
typedef struct _AQ_TXC {
    UINT32  Rsvd;
    UINT32  Len;            /* tunnel len, outer len */
    UINT32  Ctl;            /* type, ctx_id, vlan, commands, l2len */
    UINT32  Len2;           /* l3len, l4len, mss */
} AQ_TXC, *PAQ_TXC;

C_ASSERT(sizeof(AQ_TXC) == 16);

/* RX read descriptor - written by driver with buffer addresses */
typedef struct _AQ_RXD {
    UINT64  BufAddr;        /* physical address of data buffer */
    UINT64  HdrAddr;        /* physical address of header buffer (split) */
} AQ_RXD, *PAQ_RXD;

C_ASSERT(sizeof(AQ_RXD) == 16);

/* RX writeback descriptor - written by HW after packet receipt */
typedef struct _AQ_RXD_WB {
    UINT32  Type;           /* RSS type, packet type, header info */
    UINT32  RssHash;        /* RSS hash value */
    UINT16  Status;         /* DD, EOP, error flags */
    UINT16  PktLen;         /* received packet length */
    UINT16  NextDescPtr;    /* next descriptor pointer (RSC) */
    UINT16  VlanTag;        /* extracted VLAN tag */
} AQ_RXD_WB, *PAQ_RXD_WB;

C_ASSERT(sizeof(AQ_RXD_WB) == 16);

#include <poppack.h>

/* ===================================================================
 * DMA Ring Descriptor Management
 * =================================================================== */

typedef struct _AQ_DMA_RING {
    /* Descriptor ring DMA */
    PVOID               DescVa;         /* virtual addr of descriptor ring */
    NDIS_PHYSICAL_ADDRESS DescPa;       /* physical addr of descriptor ring */
    UINT32              DescSize;       /* total byte size of ring */
    NDIS_HANDLE         DescAllocHandle;

    /* Ring state */
    UINT32              Size;           /* number of descriptors */
    UINT32              Head;           /* HW consumer index */
    UINT32              Tail;           /* SW producer index */
    UINT32              DxSize;         /* single descriptor size */

    /* Per-descriptor tracking (driver-side) */
    PNET_BUFFER_LIST   *NblArray;       /* NBL back-pointers for completion */
    PVOID              *BufVa;          /* data buffer virtual addresses */
    NDIS_PHYSICAL_ADDRESS *BufPa;       /* data buffer physical addresses */
} AQ_DMA_RING, *PAQ_DMA_RING;

/* ===================================================================
 * MSI-X Interrupt State
 * =================================================================== */

#define AQ_MSIX_VECTORS_MAX         (AQ_TX_RINGS_MAX + AQ_RX_RINGS_MAX + 1)

typedef struct _AQ_INTERRUPT {
    NDIS_HANDLE         InterruptHandle;
    UINT32              MsiXVectors;
    BOOLEAN             MsiXEnabled;
} AQ_INTERRUPT, *PAQ_INTERRUPT;

/* ===================================================================
 * Link Status
 * =================================================================== */

typedef struct _AQ_LINK_STATUS {
    UINT32              SpeedMbps;      /* negotiated speed */
    BOOLEAN             FullDuplex;
    BOOLEAN             LinkUp;
    UINT32              LpSpeedMask;    /* link partner capabilities */
    UINT32              FlowControl;
} AQ_LINK_STATUS, *PAQ_LINK_STATUS;

/* ===================================================================
 * Firmware Mailbox
 * =================================================================== */

typedef struct _AQ_FW_MBOX {
    UINT32              MboxAddr;       /* DRAM offset of FW mailbox */
    UINT32              RpcAddr;        /* DRAM offset of RPC area */
    UINT32              SettingsAddr;   /* DRAM offset of settings */
    UINT32              RpcTid;         /* RPC transaction ID */
    UINT32              FwVersion;
} AQ_FW_MBOX, *PAQ_FW_MBOX;

/* ===================================================================
 * Per-Adapter Context (allocated in MiniportInitializeEx)
 *
 * This is the central state for one NIC instance, threaded through
 * every NDIS handler via MiniportAdapterContext.
 * =================================================================== */

typedef struct _AQ_ADAPTER {
    /* NDIS handles */
    NDIS_HANDLE         AdapterHandle;
    NDIS_HANDLE         DmaHandle;

    /* PCI BAR0 MMIO mapping */
    PVOID               IoBase;         /* mapped virtual address */
    UINT32              IoLength;       /* mapping length */
    NDIS_PHYSICAL_ADDRESS IoPhysBase;

    /* PCI config */
    UINT16              VendorId;
    UINT16              DeviceId;
    UINT16              SubsysId;
    UINT8               RevisionId;

    /* Chip info */
    UINT32              ChipFeatures;
    BOOLEAN             RblEnabled;     /* ROM bootloader present */

    /* MAC address */
    UINT8               PermanentMac[ETH_LENGTH_OF_ADDRESS];
    UINT8               CurrentMac[ETH_LENGTH_OF_ADDRESS];

    /* Configuration */
    UINT32              Mtu;
    UINT32              LinkSpeedMask;  /* advertised speeds */
    BOOLEAN             JumboEnabled;

    /* Firmware */
    AQ_FW_MBOX          FwMbox;

    /* Link */
    AQ_LINK_STATUS      LinkStatus;

    /* Interrupts */
    AQ_INTERRUPT        Interrupt;

    /* TX/RX descriptor rings */
    AQ_DMA_RING         TxRing[AQ_TX_RINGS_MAX];
    AQ_DMA_RING         RxRing[AQ_RX_RINGS_MAX];
    UINT32              NumTxRings;
    UINT32              NumRxRings;

    /* RSS */
    UINT8               RssKey[AQ_RSS_KEY_SIZE];
    UINT8               RssIndTable[AQ_RSS_IND_TABLE_SIZE];

    /* Statistics */
    UINT64              TxPackets;
    UINT64              TxBytes;
    UINT64              RxPackets;
    UINT64              RxBytes;
    UINT64              RxErrors;
    UINT64              TxErrors;

    /* Driver state flags */
    UINT32              Flags;
    NDIS_SPIN_LOCK      Lock;

} AQ_ADAPTER, *PAQ_ADAPTER;

/* Adapter flags */
#define AQ_FLAG_STARTED             0x00000004
#define AQ_FLAG_STOPPING            0x00000008
#define AQ_FLAG_RESETTING           0x00000010
#define AQ_FLAG_CLOSING             0x00000020
#define AQ_FLAG_LINK_DOWN           0x04000000
#define AQ_FLAG_ERR_UNPLUG          0x40000000
#define AQ_FLAG_ERR_HW              0x80000000

/* ===================================================================
 * Register Access Helpers
 * =================================================================== */

static __inline UINT32
AqReadReg(PAQ_ADAPTER Adapter, UINT32 Offset)
{
    return NdisReadRegisterUlong((PULONG)((PUCHAR)Adapter->IoBase + Offset));
}

static __inline VOID
AqWriteReg(PAQ_ADAPTER Adapter, UINT32 Offset, UINT32 Value)
{
    NdisWriteRegisterUlong((PULONG)((PUCHAR)Adapter->IoBase + Offset), Value);
}

static __inline VOID
AqFlush(PAQ_ADAPTER Adapter)
{
    (VOID)AqReadReg(Adapter, 0x10);
}

/* Read-modify-write a register field */
static __inline VOID
AqWriteRegField(PAQ_ADAPTER Adapter, UINT32 Offset,
                UINT32 Mask, UINT32 Shift, UINT32 Value)
{
    UINT32 Reg = AqReadReg(Adapter, Offset);
    Reg &= ~Mask;
    Reg |= (Value << Shift) & Mask;
    AqWriteReg(Adapter, Offset, Reg);
}

/* ===================================================================
 * Forward Declarations for NDIS Miniport Handlers
 * (implemented in separate .c files)
 * =================================================================== */

MINIPORT_INITIALIZE             MiniportInitializeEx;
MINIPORT_HALT                   MiniportHaltEx;
MINIPORT_OID_REQUEST            MiniportOidRequest;
MINIPORT_CANCEL_OID_REQUEST     MiniportCancelOidRequest;
MINIPORT_SEND_NET_BUFFER_LISTS  MiniportSendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS MiniportReturnNetBufferLists;
MINIPORT_CANCEL_SEND            MiniportCancelSend;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY MiniportDevicePnPEventNotify;
MINIPORT_SHUTDOWN               MiniportShutdownEx;
MINIPORT_CHECK_FOR_HANG         MiniportCheckForHangEx;
MINIPORT_RESET                  MiniportResetEx;
MINIPORT_PAUSE                  MiniportPause;
MINIPORT_RESTART                MiniportRestart;
MINIPORT_ISR                    MiniportInterrupt;
MINIPORT_DISABLE_INTERRUPT      MiniportDisableInterruptEx;
MINIPORT_ENABLE_INTERRUPT       MiniportEnableInterruptEx;
MINIPORT_MSI_ISR                MiniportMessageInterrupt;
MINIPORT_DISABLE_MESSAGE_INTERRUPT MiniportDisableMessageInterruptEx;
MINIPORT_ENABLE_MESSAGE_INTERRUPT  MiniportEnableMessageInterruptEx;

#endif /* AQ_UTILS_H */
