AQtion NDIS Miniport Driver for Windows on ARM64
This repository contains an NDIS miniport driver for the Marvell AQtion chipset (used in devices like the ORICO REA-10 10GbE Thunderbolt adapter) targeting Windows on ARM64 (WoA). The driver enables 10GbE networking on ARM64 systems with Thunderbolt/USB4 support, addressing the lack of native ARM64 drivers for AQtion-based adapters.

Note: This is a proof-of-concept driver, ported from the Linux atlantic driver. It requires further debugging, testing, and Microsoft HLK certification for production use. Use at your own risk on development systems.

Features

Supports Marvell AQtion chipsets (e.g., AQC107, PCI VID 0x1D6A, DID 0x00B1).
Implements NDIS 6.30 miniport for Windows 10+ on ARM64.
Handles PCIe over Thunderbolt/USB4 for hot-plug compatibility.
TX/RX ring buffers with DMA (WDM-based for ARM64 coherence).
Basic interrupt handling (MSI-X) and packet processing.
Ported hardware logic from Linux atlantic driver.
