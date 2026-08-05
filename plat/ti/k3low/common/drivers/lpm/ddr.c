/*
 * Copyright (c) 2024-2026, Texas Instruments Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <board_def.h>
#include <ddr.h>
#include <lib/mmio.h>

/* DDR Subsystem configuration base address and field values */
#define DDRSS0_SSCFG_BASE			(0xF300000UL)

/* DDRSS Subsystem Configuration Registers (R/W registers) */
#define DDRSS_SSCFG_SS_CTL_REG			(0x004U)
#define DDRSS_SSCFG_V2A_CTL_MATCH_PRI_REGS_OFFS	(0x020U)
#define NUM_DDRSS_SSCFG_V2A_CTL_MATCH_PRI_REGS	(8U)
#define DDRSS_SSCFG_V2A_OLD_CMD_PRI_RAISE_REG	(0x05CU)
#define DDRSS_SSCFG_V2A_BUS_TIMEOUT_REG		(0x09CU)
#define DDRSS_SSCFG_V2A_INT_EN_SET_REG		(0x0A8U)
#define DDRSS_SSCFG_PERF_CNT_SEL_REG		(0x100U)
#define DDRSS_SSCFG_PHY_TEST_CTL_REGS_OFFS	(0x184U)
#define NUM_DDRSS_SSCFG_PHY_TEST_CTL_REGS	(10U)
#define DDRSS_SSCFG_PHY_TEST_CTL_12_REG		(0x1B0U)

/* DDR Control system base address and field values */
#define DDRSS0_CTRL_BASE			(0xF308000UL)
/* Register block CTL (CTL_0-CTL_422) offset, total and field values */
#define CTLCFG_DENALI_CTL_(x)			((x) << 2U)
#define NUM_DDR_CTL_REG				423U
/* Register block PI (PI_0-PI_344) offset, total and field values */
#define DDRSS_PI_REGISTER_BLOCK_OFFS		0x2000U
#define CTLCFG_DENALI_PI_(x) \
	(((x) << 2U) + DDRSS_PI_REGISTER_BLOCK_OFFS)
#define NUM_DDR_PI_REG				345U
/* Register block Data_Slice_0 (or PHY Register block offset PHY_0-PHY_125) */
#define DDRSS_DATA_SLICE_0_REGISTER_BLOCK_OFFS	0x4000U
#define CTLCFG_DENALI_PHY_(x) \
	(((x) << 2U) + DDRSS_DATA_SLICE_0_REGISTER_BLOCK_OFFS)
#define NUM_DDR_DATA_0_REG			126U
/* Register block Data_Slice_1 (PHY_256-PHY_381) offset and total */
#define DDRSS_DATA_SLICE_1_REGISTER_BLOCK_OFFS	0x4400U
#define NUM_DDR_DATA_1_REG			126U
/* Register block Address_Slice_0 (PHY_512-PHY_554) offset and total */
#define DDRSS_ADDRESS_SLICE_0_REGISTER_BLOCK_OFFS	0x4800U
#define NUM_DDR_ADDR_0_REG			43U
/* Register block Address_Slice_1 (PHY_768-PHY_810) offset and total */
#define DDRSS_ADDRESS_SLICE_1_REGISTER_BLOCK_OFFS	0x4c00U
#define NUM_DDR_ADDR_1_REG			43U
/* Register block Address_Slice_2 (PHY_1024-PHY_1066) offset and total */
#define DDRSS_ADDRESS_SLICE_2_REGISTER_BLOCK_OFFS	0x5000U
#define NUM_DDR_ADDR_2_REG			43U
/* Register block core (PHY_1280-PHY_1405) offset and total */
#define DDRSS_PHY_CORE_REGISTER_BLOCK_OFFS	0x5400U
#define NUM_DDR_PHY_REG				126U
#define DDRSS_PHY_CORE_REGISTER_1281_POS	0x1U
#define DDRSS_PHY_CORE_REGISTER_1281_MULTICAST_EN	BIT(8)
#define DDRSS_PHY_CORE_REGISTER_1281_FREQ_SEL_INDEX	BIT(16)
#define NUM_ALL_PHY_REG \
	(NUM_DDR_DATA_0_REG + NUM_DDR_DATA_1_REG + \
	 NUM_DDR_ADDR_0_REG + NUM_DDR_ADDR_1_REG + \
	 NUM_DDR_ADDR_2_REG + NUM_DDR_PHY_REG)
#define NUM_ALL_DDR_REG \
	(NUM_DDR_CTL_REG + NUM_DDR_PI_REG + (NUM_ALL_PHY_REG << 1U))
#define LP_MODE_LONG_SELF_REFRESH		0x31U
#define LP_MODE_LONG_SELF_REFRESH_PHY_CTRL	0x51U
#define LP_MODE_LONG_SELF_REFRESH_EXIT		0x2U
#define LPDDR4_DRAM_CLASS_REG_VALUE		0xBU
#define DDR4_DRAM_CLASS_REG_VALUE		0xAU
#define CTL_BUSY_BIT				BIT(0)
#define INT_STATUS_DFS_OFFSET			16U
/* DFS (Dynamic Frequency Scaling) interrupt status bits in CTL_342 register */
#define DFS_INT_HW_IGNORED			BIT(0)	/* HW DFS request ignored */
#define DFS_INT_HW_TIMEOUT			BIT(1)	/* HW DFS timeout error */
#define DFS_INT_HW_DONE				BIT(2)	/* HW DFS completed */
#define DFS_INT_SW_IGNORED			BIT(3)	/* SW DFS request ignored */
#define DFS_INT_SW_TIMEOUT			BIT(4)	/* SW DFS timeout error */
#define DFS_INT_SW_DONE				BIT(5)	/* SW DFS completed */
#define DFS_INT_ERROR_MASK			(DFS_INT_HW_IGNORED | \
						 DFS_INT_HW_TIMEOUT | \
						 DFS_INT_SW_IGNORED | \
						 DFS_INT_SW_TIMEOUT)
#define DDR_MEM_ACTIVE_FREQ_SHIFT		8U
#define DDR_MEM_ACTIVE_FREQ_MASK		0x1FU
#define DDR_MEM_CLASS_SHIFT			8U
#define DDR_MEM_CLASS_MASK			0xF00U

/* WKUP CTRL MMR Base and register configuration values */
#define WKUP_CTRL_MMR_SEC_4_BASE		(0x43040000UL)
#define CHNG_DDR4_FSP_REQ			(0x0U)
#define CHNG_DDR4_FSP_REQ_REQ			BIT(8)
#define CHNG_DDR4_FSP_REQ_REQ_TYPE		(0x0U)
#define CHNG_DDR4_FSP_ACK			(0x4U)
#define CHNG_DDR4_FSP_ACK_ACK			BIT(7)
#define CHNG_DDR4_FSP_ACK_ERROR			BIT(0)
#define DDR4_FSP_CLKCHNG_REQ			(0x80U)
#define DDR4_FSP_CLKCHNG_REQ_REQ		BIT(7)
#define DDR4_FSP_CLKCHNG_REQ_REQ_TYPE_MASK	(3U)
#define DDR4_FSP_CLKCHNG_ACK			(0x84U)
#define DDR4_FSP_CLKCHNG_ACK_ACK		BIT(0)

/* MAIN PLL MMR Base */
#define MAIN_PLL_MMR_BASE			(0x04060000UL)

#define TIMEOUT_VALUE				10000000U

/*
 * Structure to hold DDR subsystem configuration registers.
 *
 * Defines all DDRSS configuration registers that need to be saved and
 * restored during suspend/resume operations.
 */
struct emif_handle_s {
	uintptr_t	ss_cfg_base_addr;
	uintptr_t	ctl_cfg_base_addr;
};

struct ddrss_sscfg_regs {
	uint32_t	ss_ctl_reg;
	uint32_t	v2a_ctl_match_pri_regs[NUM_DDRSS_SSCFG_V2A_CTL_MATCH_PRI_REGS];
	uint32_t	v2a_old_cmd_pri_raise_reg;
	uint32_t	v2a_bus_timeout_reg;
	uint32_t	v2a_int_en_set_reg;
	uint32_t	perf_cnt_sel_reg;
	uint32_t	phy_test_ctl_regs[NUM_DDRSS_SSCFG_PHY_TEST_CTL_REGS];
	uint32_t	phy_test_ctl_12_reg;
};

__wkupsramdata struct emif_handle_s Emifhandle;
__wkupsramdata uint32_t ddrss_save_restore[NUM_ALL_DDR_REG];
__wkupsramdata bool ddrss_is_fsp_supported;
__wkupsramdata uint32_t dram_class;
__wkupsramdata struct ddrss_sscfg_regs ddrss_sscfg_regs;

/* Poll until PI and CTL initialisation complete. */
__wkupsramfunc static void poll_for_init_completion(struct emif_handle_s *h)
{
	/* Poll for PI Init completion */
	while (((mmio_read_32(h->ctl_cfg_base_addr +
			      CTLCFG_DENALI_PI_(83))) & 0x1U) != 0x1U) {
	}
	/* Poll for CTL Init completion */
	while (((mmio_read_32(h->ctl_cfg_base_addr +
			      CTLCFG_DENALI_CTL_(342))) &
		0x02000000U) != 0x02000000U) {
	}
}

/*
 * Write a bit field into an MMR register.
 *
 * mmr_address: address of the register.
 * field_value: value to place in the field.
 * width:       width of the field in bits.
 * leftshift:   least-significant bit position of the field.
 */
__wkupsramfunc static void write_mmr_field(uintptr_t mmr_address,
					   uint32_t field_value,
					   uint32_t width, uint32_t leftshift)
{
	uint32_t val;
	uint32_t mask;

	val = mmio_read_32(mmr_address);
	mask = (((1U << width) - 1U) << leftshift);
	mask = ~mask;
	val &= mask;
	val |= (field_value << leftshift);
	mmio_write_32(mmr_address, val);
}

/* Enable DDR data retention mode via WKUP_CTRL_MMR DDR32SS_PMCTRL. */
__wkupsramfunc static void enable_ddr_data_retention(void)
{
	uint32_t val;

	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x6U, 4U, 0U);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x1U, 1U, 31U);
	val = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL);
	while (val != ((1U << 31U) | 0x6U)) {
		val = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL);
	}
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x0U, 1U, 31U);
}

/*
 * Execute DDR Frequency Set Point (FSP) change sequence.
 *
 * Performs the hardware handshake to switch the DDR controller operating
 * frequency by coordinating between the DDR controller, PLL, and WKUP
 * control registers.
 *
 * fsp_point: target FSP to switch to (0, 1, or 2).
 *
 * Return 0 on success; negative values indicate specific failure points:
 *   -1: timeout waiting for controller busy to clear
 *   -2: timeout waiting for FSP clock change request
 *   -3: invalid FSP request type
 *   -4: timeout waiting for clock change request to clear
 *   -5: timeout waiting for DDR FSP acknowledgment
 *   -6: DDR FSP acknowledgment error bit set
 *   -7: timeout waiting for DFS interrupt status
 *   -8: DFS operation error (HW/SW ignored or timeout)
 */
__wkupsramfunc static int32_t execute_ddr_fsp_seq(uint8_t fsp_point)
{
	uint32_t req;
	uint32_t req_type;
	uint32_t timeout;
	uint32_t int_status;

	/* Wait for controller busy signal to be de-asserted */
	timeout = TIMEOUT_VALUE;
	while (((mmio_read_32(DDRSS0_CTRL_BASE +
			      CTLCFG_DENALI_CTL_(330)) &
		 CTL_BUSY_BIT) == CTL_BUSY_BIT) && (timeout > 0U)) {
		timeout--;
	}
	if (timeout == 0U) {
		return -1;
	}

	/* Set valid data for FSP points to initiate DFS request */
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(276),
			1U, 1U, 24U);
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(277),
			1U, 1U, 8U);
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(277),
			1U, 1U, 0U);

	/* Set the request type in FSP request register */
	mmio_write_32(WKUP_CTRL_MMR_SEC_4_BASE + CHNG_DDR4_FSP_REQ,
		      fsp_point);
	dsb();
	/* Initiate the request in FSP request register */
	mmio_write_32(WKUP_CTRL_MMR_SEC_4_BASE + CHNG_DDR4_FSP_REQ,
		      fsp_point | CHNG_DDR4_FSP_REQ_REQ);
	dsb();

	/* Wait for the request to be asserted in clock change request register */
	timeout = TIMEOUT_VALUE;
	req = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
			    DDR4_FSP_CLKCHNG_REQ) &
	       DDR4_FSP_CLKCHNG_REQ_REQ);
	while ((req != DDR4_FSP_CLKCHNG_REQ_REQ) && (timeout > 0U)) {
		req = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
				    DDR4_FSP_CLKCHNG_REQ) &
		       DDR4_FSP_CLKCHNG_REQ_REQ);
		timeout--;
	}
	if (timeout == 0U) {
		return -2;
	}

	/* Change the PLL frequency as per the requested FSP point */
	req_type = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
				 DDR4_FSP_CLKCHNG_REQ) &
		    DDR4_FSP_CLKCHNG_REQ_REQ_TYPE_MASK);
	if (req_type == 0U) {
		write_mmr_field((MAIN_PLL_MMR_BASE + (0U * 0x1000U) +
				 ((2U * 0x4U) + 0x80U)),
				0x4FU, 7U, 0U);
	} else if (req_type == 1U) {
		write_mmr_field((MAIN_PLL_MMR_BASE + (0U * 0x1000U) +
				 ((2U * 0x4U) + 0x80U)),
				0x9U, 7U, 0U);
	} else if (req_type == 2U) {
		write_mmr_field((MAIN_PLL_MMR_BASE + (0U * 0x1000U) +
				 ((2U * 0x4U) + 0x80U)),
				0x4U, 7U, 0U);
	} else {
		return -3;
	}
	dsb();

	/* Set the FSP ACK bit */
	mmio_write_32(WKUP_CTRL_MMR_SEC_4_BASE + DDR4_FSP_CLKCHNG_ACK,
		      DDR4_FSP_CLKCHNG_ACK_ACK);
	dsb();

	/* Wait for request to go away */
	timeout = TIMEOUT_VALUE;
	req = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
			    DDR4_FSP_CLKCHNG_REQ) &
	       DDR4_FSP_CLKCHNG_REQ_REQ);
	while ((req == DDR4_FSP_CLKCHNG_REQ_REQ) && (timeout > 0U)) {
		req = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
				    DDR4_FSP_CLKCHNG_REQ) &
		       DDR4_FSP_CLKCHNG_REQ_REQ);
		timeout--;
	}
	if (timeout == 0U) {
		return -4;
	}

	/* Clear the ACK bit */
	mmio_write_32(WKUP_CTRL_MMR_SEC_4_BASE + DDR4_FSP_CLKCHNG_ACK, 0x0U);
	dsb();

	/* Wait for DDR to acknowledge the software-initiated FSP request */
	timeout = TIMEOUT_VALUE;
	req = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
			    CHNG_DDR4_FSP_ACK) &
	       CHNG_DDR4_FSP_ACK_ACK);
	while ((req == 0x0U) && (timeout > 0U)) {
		req = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
				    CHNG_DDR4_FSP_ACK) &
		       CHNG_DDR4_FSP_ACK_ACK);
		timeout--;
	}
	if (timeout == 0U) {
		return -5;
	}

	/* Read the error bit */
	if ((mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
			  CHNG_DDR4_FSP_ACK) &
	     CHNG_DDR4_FSP_ACK_ERROR) != 0U) {
		return -6;
	}

	/* De-assert the software-initiated FSP request */
	req = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE + CHNG_DDR4_FSP_REQ);
	req &= ~CHNG_DDR4_FSP_REQ_REQ;
	mmio_write_32(WKUP_CTRL_MMR_SEC_4_BASE + CHNG_DDR4_FSP_REQ, req);
	dsb();

	/* Check the status of interrupts related to frequency scaling */
	timeout = TIMEOUT_VALUE;
	int_status = (mmio_read_32(DDRSS0_CTRL_BASE +
				   CTLCFG_DENALI_CTL_(342)) >>
		      INT_STATUS_DFS_OFFSET);
	while ((int_status == 0U) && (timeout > 0U)) {
		int_status = (mmio_read_32(DDRSS0_CTRL_BASE +
					   CTLCFG_DENALI_CTL_(342)) >>
			      INT_STATUS_DFS_OFFSET);
		timeout--;
	}
	if (timeout == 0U) {
		return -7;
	}

	/* Acknowledge all DFS interrupts */
	mmio_write_32(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(350), int_status);

	/* Check if any error occurred */
	if ((int_status & DFS_INT_ERROR_MASK) != 0U) {
		return -8;
	}

	return 0;
}

/* Save all read/write subsystem configuration registers. */
__wkupsramfunc static void save_ss_config_registers(struct emif_handle_s *h)
{
	uintptr_t base = h->ss_cfg_base_addr;
	uint32_t i;

	/* Save Subsystem Control Register */
	ddrss_sscfg_regs.ss_ctl_reg = mmio_read_32(base +
						    DDRSS_SSCFG_SS_CTL_REG);

	/* Save VBUSM2AXI Control, Range Match and Priority Map Registers */
	for (i = 0U; i < NUM_DDRSS_SSCFG_V2A_CTL_MATCH_PRI_REGS; i++) {
		ddrss_sscfg_regs.v2a_ctl_match_pri_regs[i] =
			mmio_read_32(base +
				     DDRSS_SSCFG_V2A_CTL_MATCH_PRI_REGS_OFFS +
				     (i * 4U));
	}

	/* Save VBUSM2AXI Oldest Command Priority Raise Register */
	ddrss_sscfg_regs.v2a_old_cmd_pri_raise_reg =
		mmio_read_32(base + DDRSS_SSCFG_V2A_OLD_CMD_PRI_RAISE_REG);

	/* Save VBUSM2AXI Bus Timeout Register */
	ddrss_sscfg_regs.v2a_bus_timeout_reg =
		mmio_read_32(base + DDRSS_SSCFG_V2A_BUS_TIMEOUT_REG);

	/* Save VBUSM2AXI Interrupt Enable Register */
	ddrss_sscfg_regs.v2a_int_en_set_reg =
		mmio_read_32(base + DDRSS_SSCFG_V2A_INT_EN_SET_REG);

	/* Save Performance Counter Select Register */
	ddrss_sscfg_regs.perf_cnt_sel_reg =
		mmio_read_32(base + DDRSS_SSCFG_PERF_CNT_SEL_REG);

	/* Save PHY Test Control Registers 1-10 */
	for (i = 0U; i < NUM_DDRSS_SSCFG_PHY_TEST_CTL_REGS; i++) {
		ddrss_sscfg_regs.phy_test_ctl_regs[i] =
			mmio_read_32(base +
				     DDRSS_SSCFG_PHY_TEST_CTL_REGS_OFFS +
				     (i * 4U));
	}

	/*
	 * Save PHY Test Control Register 12 (0x1B0).
	 * Note: register 11 at 0x1AC is reserved.
	 */
	ddrss_sscfg_regs.phy_test_ctl_12_reg =
		mmio_read_32(base + DDRSS_SSCFG_PHY_TEST_CTL_12_REG);
}

/* Restore subsystem configuration registers previously saved by save_ss_config_registers(). */
__wkupsramfunc static void restore_ss_config_registers(struct emif_handle_s *h)
{
	uintptr_t base = h->ss_cfg_base_addr;
	uint32_t i;

	/* Restore Subsystem Control Register */
	mmio_write_32(base + DDRSS_SSCFG_SS_CTL_REG,
		      ddrss_sscfg_regs.ss_ctl_reg);

	/* Restore VBUSM2AXI Control, Range Match and Priority Map Registers */
	for (i = 0U; i < NUM_DDRSS_SSCFG_V2A_CTL_MATCH_PRI_REGS; i++) {
		mmio_write_32(base +
			      DDRSS_SSCFG_V2A_CTL_MATCH_PRI_REGS_OFFS +
			      (i * 4U),
			      ddrss_sscfg_regs.v2a_ctl_match_pri_regs[i]);
	}

	/* Restore VBUSM2AXI Oldest Command Priority Raise Register */
	mmio_write_32(base + DDRSS_SSCFG_V2A_OLD_CMD_PRI_RAISE_REG,
		      ddrss_sscfg_regs.v2a_old_cmd_pri_raise_reg);

	/* Restore VBUSM2AXI Bus Timeout Register */
	mmio_write_32(base + DDRSS_SSCFG_V2A_BUS_TIMEOUT_REG,
		      ddrss_sscfg_regs.v2a_bus_timeout_reg);

	/* Restore VBUSM2AXI Interrupt Enable Register */
	mmio_write_32(base + DDRSS_SSCFG_V2A_INT_EN_SET_REG,
		      ddrss_sscfg_regs.v2a_int_en_set_reg);

	/* Restore Performance Counter Select Register */
	mmio_write_32(base + DDRSS_SSCFG_PERF_CNT_SEL_REG,
		      ddrss_sscfg_regs.perf_cnt_sel_reg);

	/* Restore PHY Test Control Registers 1-10 */
	for (i = 0U; i < NUM_DDRSS_SSCFG_PHY_TEST_CTL_REGS; i++) {
		mmio_write_32(base +
			      DDRSS_SSCFG_PHY_TEST_CTL_REGS_OFFS + (i * 4U),
			      ddrss_sscfg_regs.phy_test_ctl_regs[i]);
	}

	/*
	 * Restore PHY Test Control Register 12 (0x1B0).
	 * Note: register 11 at 0x1AC is reserved.
	 */
	mmio_write_32(base + DDRSS_SSCFG_PHY_TEST_CTL_12_REG,
		      ddrss_sscfg_regs.phy_test_ctl_12_reg);
}

__wkupsramfunc int32_t k3low_put_ddr_in_rtc_lpm(void)
{
	uint32_t lp_status = 0U;
	int32_t ret;

	/* disable auto entry / exit */
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(167),
			0U, 4U, 16U);
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(167),
			0U, 4U, 24U);

	ret = execute_ddr_fsp_seq(0U);
	if (ret != 0) {
		return ret;
	}

	/* Program Self Refresh mode */
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(158),
			LP_MODE_LONG_SELF_REFRESH, 7U, 8U);
	/* Poll for Self Refresh Mode change */
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(353),
			0x0U, 16U, 16U);
	lp_status = (mmio_read_32(DDRSS0_CTRL_BASE +
				  CTLCFG_DENALI_CTL_(337)) & 0x10000U);
	while (lp_status != 0x10000U) {
		lp_status = (mmio_read_32(DDRSS0_CTRL_BASE +
					  CTLCFG_DENALI_CTL_(337)) & 0x10000U);
	}
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(345),
			0x1U, 16U, 16U);
	lp_status = ((mmio_read_32(DDRSS0_CTRL_BASE +
				   CTLCFG_DENALI_CTL_(167)) & 0x7F00U) >> 8U);
	while (lp_status != 0x4EU) {
		lp_status = ((mmio_read_32(DDRSS0_CTRL_BASE +
					   CTLCFG_DENALI_CTL_(167)) &
			      0x7F00U) >> 8U);
	}

	/*
	 * Enable DDR data retention: write b0110 to
	 * WKUP_CTRL_MMR.DDR32SS_PMCTRL.data_retention.
	 */
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x6U, 4U, 0U);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x1U, 1U, 31U);
	lp_status = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL);
	while (lp_status != ((1U << 31U) | 0x6U)) {
		lp_status = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
					 DDR32SS_PMCTRL);
	}
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x0U, 1U, 31U);

	return 0;
}

/* Configure EMIF base addresses for the single DDR subsystem instance. */
__wkupsramfunc static void emif_instance_select(struct emif_handle_s *h)
{
	h->ss_cfg_base_addr = (uintptr_t)DDRSS0_SSCFG_BASE;
	h->ctl_cfg_base_addr = (uintptr_t)DDRSS0_CTRL_BASE;
}

/* Start PI and CTL initialisation (set PI_START=1 and START=1). */
__wkupsramfunc static void start_PI_CTL_init(struct emif_handle_s *h)
{
	uint32_t wr_init_val;
	volatile uint32_t i;

	wr_init_val = ((dram_class << DDR_MEM_CLASS_SHIFT) | 0x1U);
	/* Set START bit in register for PI module */
	mmio_write_32(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(0),
		      wr_init_val);

	for (i = 0U; i < 1000U; i++) {
	}

	/* Set START bit in register for controller */
	mmio_write_32(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(0),
		      wr_init_val);
}

/* Save all DDR controller, PI and PHY registers to the WKUP SRAM buffer. */
__wkupsramfunc static void save_ddr_registers(struct emif_handle_s *h)
{
	uint32_t i;
	uint32_t j;
	uint8_t current_freq_set;

	/* DDRSS Memory Base */
	uintptr_t DDR_CTL_REG_BASE = h->ctl_cfg_base_addr;
	uintptr_t DDR_PI_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_PI_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_DATA_SLICE_0_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_DATA_SLICE_0_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_DATA_SLICE_1_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_DATA_SLICE_1_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_ADDR_SLICE_0_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_0_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_ADDR_SLICE_1_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_1_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_ADDR_SLICE_2_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_2_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_CORE_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_PHY_CORE_REGISTER_BLOCK_OFFS;

	ddrss_is_fsp_supported = false;

	save_ss_config_registers(h);

	/*
	 * Update PI_INIT_WORK_FREQ and INIT_FREQ based on the current
	 * operating frequency set.
	 */
	current_freq_set =
		(uint8_t)((mmio_read_32(h->ctl_cfg_base_addr +
					CTLCFG_DENALI_PI_(153)) >>
			   DDR_MEM_ACTIVE_FREQ_SHIFT) &
			  DDR_MEM_ACTIVE_FREQ_MASK);
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(11),
			current_freq_set, 5U, 0U);
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(178),
			current_freq_set, 2U, 0U);

	/* Save the class of DRAM */
	dram_class = ((mmio_read_32(DDR_CTL_REG_BASE) &
		       DDR_MEM_CLASS_MASK) >> DDR_MEM_CLASS_SHIFT);

	j = 0U;
	for (i = 0U; i < NUM_DDR_CTL_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(DDR_CTL_REG_BASE + (i * 4U));
	}
	for (i = 0U; i < NUM_DDR_PI_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(DDR_PI_REG_BASE + (i * 4U));
	}
	/* Save the current operating frequency register set (set 2) */
	for (i = 0U; i < NUM_DDR_DATA_0_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(DDR_PHY_DATA_SLICE_0_REG_BASE + (i * 4U));
	}
	for (i = 0U; i < NUM_DDR_DATA_1_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(DDR_PHY_DATA_SLICE_1_REG_BASE + (i * 4U));
	}
	for (i = 0U; i < NUM_DDR_ADDR_0_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(DDR_PHY_ADDR_SLICE_0_REG_BASE + (i * 4U));
	}
	for (i = 0U; i < NUM_DDR_ADDR_1_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(DDR_PHY_ADDR_SLICE_1_REG_BASE + (i * 4U));
	}
	for (i = 0U; i < NUM_DDR_ADDR_2_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(DDR_PHY_ADDR_SLICE_2_REG_BASE + (i * 4U));
	}

	/*
	 * Multicast is disabled when multiple FSPs are configured; the
	 * MULTICAST_EN bit being clear indicates FSP support.
	 */
	if (((mmio_read_32(DDR_CTL_REG_BASE +
			   CTLCFG_DENALI_PHY_(1281))) &
	     BIT(8)) == 0U) {
		ddrss_is_fsp_supported = true;
	}
	for (i = 0U; i < NUM_DDR_PHY_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(DDR_PHY_CORE_REG_BASE + (i * 4U));
	}
	if (ddrss_is_fsp_supported) {
		/* Write phy_freq_sel_index = 1 to save the second frequency set */
		mmio_write_32(DDR_CTL_REG_BASE + CTLCFG_DENALI_PHY_(1281),
			      BIT(16));

		/* Save the second register set */
		for (i = 0U; i < NUM_DDR_DATA_0_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(DDR_PHY_DATA_SLICE_0_REG_BASE +
					     (i * 4U));
		}
		for (i = 0U; i < NUM_DDR_DATA_1_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(DDR_PHY_DATA_SLICE_1_REG_BASE +
					     (i * 4U));
		}
		for (i = 0U; i < NUM_DDR_ADDR_0_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(DDR_PHY_ADDR_SLICE_0_REG_BASE +
					     (i * 4U));
		}
		for (i = 0U; i < NUM_DDR_ADDR_1_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(DDR_PHY_ADDR_SLICE_1_REG_BASE +
					     (i * 4U));
		}
		for (i = 0U; i < NUM_DDR_ADDR_2_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(DDR_PHY_ADDR_SLICE_2_REG_BASE +
					     (i * 4U));
		}
		/* Save the DDR PHY set with correct frequency select index */
		for (i = 0U; i < NUM_DDR_PHY_REG; i++, j++) {
			if (i == DDRSS_PHY_CORE_REGISTER_1281_POS) {
				ddrss_save_restore[j] =
					DDRSS_PHY_CORE_REGISTER_1281_FREQ_SEL_INDEX |
					DDRSS_PHY_CORE_REGISTER_1281_MULTICAST_EN;
			} else {
				ddrss_save_restore[j] =
					mmio_read_32(DDR_PHY_CORE_REG_BASE +
						     (i * 4U));
			}
		}
	}
}

/* Restore DDR controller, PI and PHY registers from the WKUP SRAM buffer. */
__wkupsramfunc static void restore_ddr_registers(struct emif_handle_s *h)
{
	uint32_t i;
	uint32_t j;

	/* DDRSS Memory Base */
	uintptr_t DDR_CTL_REG_BASE = h->ctl_cfg_base_addr;
	uintptr_t DDR_PI_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_PI_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_DATA_SLICE_0_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_DATA_SLICE_0_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_DATA_SLICE_1_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_DATA_SLICE_1_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_ADDR_SLICE_0_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_0_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_ADDR_SLICE_1_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_1_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_ADDR_SLICE_2_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_2_REGISTER_BLOCK_OFFS;
	uintptr_t DDR_PHY_CORE_REG_BASE =
		h->ctl_cfg_base_addr + DDRSS_PHY_CORE_REGISTER_BLOCK_OFFS;

	mmio_write_32(DDR_CTL_REG_BASE + CTLCFG_DENALI_CTL_(0),
		      dram_class << DDR_MEM_CLASS_SHIFT);
	/* Skip the first CTL register write (already written above) */
	j = 1U;
	for (i = 1U; i < NUM_DDR_CTL_REG; i++, j++) {
		mmio_write_32(DDR_CTL_REG_BASE + (i * 4U),
			      ddrss_save_restore[j]);
	}
	mmio_write_32(DDR_CTL_REG_BASE + CTLCFG_DENALI_PI_(0),
		      dram_class << DDR_MEM_CLASS_SHIFT);
	/* Skip the first PI register write */
	j++;
	for (i = 1U; i < NUM_DDR_PI_REG; i++, j++) {
		mmio_write_32(DDR_PI_REG_BASE + (i * 4U),
			      ddrss_save_restore[j]);
	}

	/* Restore the second frequency set conditionally */
	if (ddrss_is_fsp_supported) {
		/* Advance j to the second set saved after the first PHY set */
		j = j + NUM_ALL_PHY_REG;

		for (i = 0U; i < NUM_DDR_DATA_0_REG; i++, j++) {
			mmio_write_32(DDR_PHY_DATA_SLICE_0_REG_BASE + (i * 4U),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_DATA_1_REG; i++, j++) {
			mmio_write_32(DDR_PHY_DATA_SLICE_1_REG_BASE + (i * 4U),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_ADDR_0_REG; i++, j++) {
			mmio_write_32(DDR_PHY_ADDR_SLICE_0_REG_BASE + (i * 4U),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_ADDR_1_REG; i++, j++) {
			mmio_write_32(DDR_PHY_ADDR_SLICE_1_REG_BASE + (i * 4U),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_ADDR_2_REG; i++, j++) {
			mmio_write_32(DDR_PHY_ADDR_SLICE_2_REG_BASE + (i * 4U),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_PHY_REG; i++, j++) {
			mmio_write_32(DDR_PHY_CORE_REG_BASE + (i * 4U),
				      ddrss_save_restore[j]);
		}

		/* Disable multicast before restoring the first set */
		mmio_write_32(DDR_CTL_REG_BASE + CTLCFG_DENALI_PHY_(1281),
			      0U);

		/* Adjust j back to point to the first register set */
		j = j - (NUM_ALL_PHY_REG << 1U);
	}

	/* Restore the first frequency register set */
	for (i = 0U; i < NUM_DDR_DATA_0_REG; i++, j++) {
		mmio_write_32(DDR_PHY_DATA_SLICE_0_REG_BASE + (i * 4U),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_DATA_1_REG; i++, j++) {
		mmio_write_32(DDR_PHY_DATA_SLICE_1_REG_BASE + (i * 4U),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_ADDR_0_REG; i++, j++) {
		mmio_write_32(DDR_PHY_ADDR_SLICE_0_REG_BASE + (i * 4U),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_ADDR_1_REG; i++, j++) {
		mmio_write_32(DDR_PHY_ADDR_SLICE_1_REG_BASE + (i * 4U),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_ADDR_2_REG; i++, j++) {
		mmio_write_32(DDR_PHY_ADDR_SLICE_2_REG_BASE + (i * 4U),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_PHY_REG; i++, j++) {
		mmio_write_32(DDR_PHY_CORE_REG_BASE + (i * 4U),
			      ddrss_save_restore[j]);
	}
}

/* Restore DDR registers, release data retention, and restart the controller. */
__wkupsramfunc static void ddr_deep_sleep_resume_sequence(struct emif_handle_s *h)
{
	uint32_t lp_status;

	restore_ss_config_registers(h);

	/* Write back the copied registers */
	restore_ddr_registers(h);

	/* Configure PHY and PI settings for resume sequence */
	/* PHY_1306: Set DFI input 0 - configures DFI interface input settings */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PHY_(1306),
			0x1U, 1U, 0U);
	/* PI_4: Disable PI_INIT_LVL_EN - disable initialization leveling */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(4),
			0x0U, 1U, 0U);
	/* CTL_20: Enable PHY_INDEP_TRAIN_MODE - enable independent PHY training mode */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(20),
			0x1U, 1U, 24U);
	/* CTL_21: Enable PHY_INDEP_INIT_MODE - enable independent PHY initialization mode */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(21),
			0x1U, 1U, 8U);
	/* PI_138: Enable PI_DLL_RST - enable DLL reset */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(138),
			0x1U, 1U, 0U);
	/* CTL_106: Disable PWRUP_SREFRESH_EXIT - disable power-up self-refresh exit */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(106),
			0x0U, 1U, 0U);
	/* PI_134: Enable PI_PWRUP_SREFRESH_EXIT - enable PI power-up self-refresh exit */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(134),
			0x1U, 1U, 8U);
	/* PI_138: Enable PI_DRAM_INIT_EN - enable DRAM initialization */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(138),
			0x1U, 1U, 8U);

	/* De-assert data retention pin and wake control bits */
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x0U, 1U, 31U);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x0U, 4U, 0U);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x1U, 1U, 31U);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x0U, 4U, 0U);
	lp_status = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL);
	while (lp_status != (1U << 31U)) {
		lp_status = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
					 DDR32SS_PMCTRL);
	}
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			0x0U, 1U, 31U);

	/* Start Initialization [PI_START=1 and START=1] */
	start_PI_CTL_init(h);

	/* Wait for INIT_DONE interrupt */
	poll_for_init_completion(h);
}

/* Put DDR into self-refresh mode and enable data retention. */
__wkupsramfunc static void enter_lpm_self_refresh(struct emif_handle_s *h)
{
	uint32_t lp_status = 0U;
	uint32_t lp_status_expected = 0U;

	/* Program Self Refresh mode: self-refresh long with memory clock gating */
	mmio_write_32(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(158),
		      (LP_MODE_LONG_SELF_REFRESH << 8U));

	if (dram_class == LPDDR4_DRAM_CLASS_REG_VALUE) {
		lp_status_expected = 0x4EU;
	} else if (dram_class == DDR4_DRAM_CLASS_REG_VALUE) {
		lp_status_expected = 0x49U;
	} else {
		/* Invalid */
		lp_status_expected = 0xFFU;
	}

	/* Poll for Self Refresh Mode change */
	while (lp_status != lp_status_expected) {
		lp_status = ((mmio_read_32(h->ctl_cfg_base_addr +
					   CTLCFG_DENALI_CTL_(167)) &
			      0x7F00U) >> 8U);
	}
}

__wkupsramfunc int32_t k3low_ddr_deep_sleep_suspend_sequence(void)
{
	/*
	 * Save DDR register context in WKUP SRAM and put the DDR in
	 * self refresh.
	 */
	emif_instance_select(&Emifhandle);
	save_ddr_registers(&Emifhandle);
	enter_lpm_self_refresh(&Emifhandle);
	/*
	 * Enable DDR data retention: write b0110 to
	 * WKUP_CTRL_MMR.DDR32SS_PMCTRL.data_retention.
	 */
	enable_ddr_data_retention();

	return 0;
}

__wkupsramfunc int32_t k3low_ddr_deep_sleep_resume_sequence(void)
{
	/*
	 * Restore DDR controller context, take DDR out of self refresh,
	 * and remove data retention.
	 */
	emif_instance_select(&Emifhandle);
	ddr_deep_sleep_resume_sequence(&Emifhandle);

	return 0;
}
