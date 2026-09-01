/*
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include <common/debug.h>
#include <lib/mmio.h>

#include <board_def.h>
#include <lpm_ddr.h>
#include <lpm_timeout.h>

/* DDR Subsystem configuration base address and field values */
#define DDRSS0_SSCFG_BASE			(0xF300000UL)

/* DDRSS Subsystem Configuration Registers (R/W registers) */
#define DDRSS_SSCFG_SS_CTL_REG				(0x004U)
#define DDRSS_SSCFG_V2A_CTL_MATCH_PRI_REGS_OFFS	(0x020U)
#define NUM_DDRSS_SSCFG_V2A_CTL_MATCH_PRI_REGS		(8U)
#define DDRSS_SSCFG_V2A_OLD_CMD_PRI_RAISE_REG		(0x05CU)
#define DDRSS_SSCFG_V2A_BUS_TIMEOUT_REG		(0x09CU)
#define DDRSS_SSCFG_V2A_INT_EN_SET_REG			(0x0A8U)
#define DDRSS_SSCFG_PERF_CNT_SEL_REG			(0x100U)
#define DDRSS_SSCFG_PHY_TEST_CTL_REGS_OFFS		(0x184U)
#define NUM_DDRSS_SSCFG_PHY_TEST_CTL_REGS		(10U)
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
#define DDR_REG_STRIDE				4U
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
#define LPDDR4_DRAM_CLASS_REG_VALUE		0xBU
#define DDR4_DRAM_CLASS_REG_VALUE		0xAU
#define CTL_BUSY_BIT				BIT(0)
#define INT_STATUS_DFS_OFFSET			16U
#define CTL_INT_INIT_STATUS			0x02000000U
#define PI_INT_INIT_STATUS			0x1U
/* DFS (Dynamic Frequency Scaling) interrupt status bits in CTL_342 register */
#define DFS_INT_HW_IGNORED			BIT(0)	/* HW DFS request ignored */
#define DFS_INT_HW_TIMEOUT			BIT(1)	/* HW DFS timeout error */
#define DFS_INT_SW_IGNORED			BIT(3)	/* SW DFS request ignored */
#define DFS_INT_SW_TIMEOUT			BIT(4)	/* SW DFS timeout error */
#define DFS_INT_ERROR_MASK			(DFS_INT_HW_IGNORED | \
						 DFS_INT_HW_TIMEOUT | \
						 DFS_INT_SW_IGNORED | \
						 DFS_INT_SW_TIMEOUT)
#define DDR_MEM_ACTIVE_FREQ_SHIFT		8U
#define DDR_MEM_ACTIVE_FREQ_MASK		GENMASK(4, 0)
#define DDR_MEM_CLASS_SHIFT			8U
#define DDR_MEM_CLASS_MASK			GENMASK(11, 8)
#define DDR_MEM_INIT_START_BIT			BIT(0)

/* LP_MODE register status values */
#define LP_STATUS_LPDDR4_LONG_SELF_REFRESH	0x4EU
#define LP_STATUS_DDR4_LONG_SELF_REFRESH	0x49U

/* CTL register indices */
#define REG_CTL_0			0U /* CTL_0: DRAM class and START bit */
#define REG_PHY_INDEP_TRAIN		20U
#define REG_PHY_INDEP_INIT		21U
#define REG_PWRUP_SREFRESH		106U
#define REG_LP_CMD			158U
#define REG_LP_AUTO			167U
#define REG_TREF_F1			178U
#define REG_MR_FSP_VALID_F0		276U
#define REG_MR_FSP_VALID_F1F2		277U
#define REG_BUSY			330U
#define REG_INT_STATUS_LP		337U
#define REG_DFS_STATUS			342U
#define REG_CTL_INT_STATUS		342U
#define REG_INT_ACK_LP			345U
#define REG_DFS_ACK			350U
#define REG_INT_MASK_LP			353U

/* PI register indices */
#define REG_PI_0			0U /* PI_0: DRAM class and START bit */
#define REG_INIT_LVL_EN			4U
#define REG_INIT_WORK_FREQ		11U
#define REG_INIT_DONE			83U
#define REG_PWRUP_SREFRESH_PI		134U
#define REG_DLL_RST			138U
#define REG_ACTIVE_FREQ			153U

/* PHY register indices */
#define REG_DFI_INPUT0			1306U
#define REG_FREQ_SEL			1281U

/* CTL PHY independent training register (CTL_20) bit field */
#define DENALI_CTL_PHY_INDEP_TRAIN_MODE_SHIFT	24U
#define DENALI_CTL_PHY_INDEP_TRAIN_MODE_WIDTH	1U
#define DENALI_CTL_PHY_INDEP_TRAIN_MODE_VAL	0x1U

/* CTL PHY independent initialization register (CTL_21) bit field */
#define DENALI_CTL_PHY_INDEP_INIT_MODE_SHIFT	8U
#define DENALI_CTL_PHY_INDEP_INIT_MODE_WIDTH	1U
#define DENALI_CTL_PHY_INDEP_INIT_MODE_VAL	0x1U

/* CTL self refresh control register (CTL_106) bit field */
#define DENALI_CTL_PWRUP_SREFRESH_EXIT_SHIFT	0U
#define DENALI_CTL_PWRUP_SREFRESH_EXIT_WIDTH	1U
#define DENALI_CTL_PWRUP_SREFRESH_EXIT_VAL	0x0U

/* CTL LP Command register (CTL_158) bit field */
#define DENALI_CTL_LP_CMD_SHIFT			8U
#define DENALI_CTL_LP_CMD_WIDTH			7U

/* CTL LP auto entry/exit control register (CTL-167) */
#define DENALI_LP_AUTO_ENTRY_EN_SHIFT		16U
#define DENALI_LP_AUTO_ENTRY_EN_WIDTH		4U
#define DENALI_LP_AUTO_ENTRY_DISABLE_VAL	0U
#define DENALI_LP_AUTO_EXIT_EN_SHIFT		24U
#define DENALI_LP_AUTO_EXIT_EN_WIDTH		4U
#define DENALI_LP_AUTO_EXIT_DISABLE_VAL		0U
#define DENALI_LP_MODE_SHIFT			8U
#define DENALI_LP_MODE_WIDTH			7U
#define DENALI_LP_MODE_MASK				(uint32_t)MASK(DENALI_LP_MODE)

/* CTL DRAM tREG value register (CTL_178) bit field */
#define DENALI_CTL_TREF_F1_SHIFT			0U
#define DENALI_CTL_TREF_F1_WIDTH			2U

/* CTL Memory Mode register (CTL_276 & CTL_277) bit field */
#define DENALI_CTL_MR_FSP_DATA_VALID_F0_SHIFT	24U
#define DENALI_CTL_MR_FSP_DATA_VALID_F1_SHIFT	0U
#define DENALI_CTL_MR_FSP_DATA_VALID_F2_SHIFT	8U
#define DENALI_CTL_MR_FSP_DATA_VALID_WIDTH	1U
#define DENALI_CTL_MR_FSP_DATA_VALID_VAL	1U

/* CTL Interrupt Status in Low Power register (CTL_337) bit field */
#define DENALI_CTL_INT_STATUS_LP_SHIFT		16U
#define DENALI_CTL_INT_STATUS_LP_BIT		1U

/* CTL interrupt status register (CTL_345) bit field */
#define DENALI_CTL_INT_ACK_LP_SHIFT		16U
#define DENALI_CTL_INT_ACK_LP_WIDTH		16U
#define DENALI_CTL_INT_ACK_LP_VAL		0x1U

/* CTL interrupt signal mask register (CTL_353) bit field */
#define DENALI_CTL_INT_MASK_LP_SHIFT		16U
#define DENALI_CTL_INT_MASK_LP_WIDTH		16U
#define DENALI_CTL_INT_MASK_LP_VAL		0x0U

/* PI initial level enable register (PI_4) bit field */
#define DENALI_PI_INIT_LVL_EN_SHIFT		0U
#define DENALI_PI_INIT_LVL_EN_WIDTH		1U
#define DENALI_PI_INIT_LVL_EN_VAL		0x0U

/* PI initial work frequency register (PI_11) bit field */
#define DENALI_PI_INIT_WORK_FREQ_SHIFT		0U
#define DENALI_PI_INIT_WORK_FREQ_WIDTH		5U

/* PI self refresh control register (PI_134) bit field */
#define DENALI_PI_PWRUP_SREFRESH_EXIT_SHIFT	8U
#define DENALI_PI_PWRUP_SREFRESH_EXIT_WIDTH	1U
#define DENALI_PI_PWRUP_SREFRESH_EXIT_VAL	0x1U

/* PI DLL reset register (PI_138) bit field */
#define DENALI_PI_DLL_RST_SHIFT			0U
#define DENALI_PI_DLL_RST_WIDTH			1U
#define DENALI_PI_DLL_RST_VAL			0x1U
#define DENALI_PI_DRAM_INIT_EN_SHIFT		8U
#define DENALI_PI_DRAM_INIT_EN_WIDTH		1U
#define DENALI_PI_DRAM_INIT_EN_VAL		0x1U

/* PHY set DFI input0 register (PHY_1306) bit field */
#define DENALI_PHY_DFI_INPUT0_SHIFT		0U
#define DENALI_PHY_DFI_INPUT0_WIDTH		1U
#define DENALI_PHY_DFI_INPUT0_VAL		0x1U

/* WKUP CTRL MMR Base and register configuration values */
#define WKUP_CTRL_MMR_SEC_4_BASE		(0x43040000UL)
#define CHNG_DDR4_FSP_REQ			(0x0U)
#define CHNG_DDR4_FSP_REQ_REQ			BIT(8)
#define CHNG_DDR4_FSP_REQ_REQ_TYPE		(0x0U)
#define CHNG_DDR4_FSP_ACK			(0x4U)
#define CHNG_DDR4_FSP_ACK_ACK			BIT(7)
#define CHNG_DDR4_FSP_ACK_IN_PROG		0x0U
#define CHNG_DDR4_FSP_ACK_ERROR			BIT(0)
#define DDR4_FSP_CLKCHNG_REQ			(0x80U)
#define DDR4_FSP_CLKCHNG_REQ_REQ		BIT(7)
#define DDR4_FSP_CLKCHNG_REQ_REQ_TYPE_MASK	GENMASK(1, 0)
#define DDR4_FSP_CLKCHNG_ACK			(0x84U)
#define DDR4_FSP_CLKCHNG_ACK_ACK		BIT(0)
#define DDR4_FSP_CLKCHNG_ACK_CLEAR		(0x0U)
#define CHNG_DDR4_FSP_REQ_FSP0			(0x0U)
#define CHNG_DDR4_FSP_REQ_FSP1			(0x1U)
#define CHNG_DDR4_FSP_REQ_FSP2			(0x2U)

/* WKUP_CTRL_MMR_CFG4_DDR32SS_PMCTRL Register */
#define DDR32SS_PMCTRL				(0x1000U)
#define DDR32SS_PMCTRL_DATA_RETENTION_SHIFT	0U
#define DDR32SS_PMCTRL_DATA_RETENTION_WIDTH	4U
#define DDR32SS_PMCTRL_DATA_RETENTION_DEACTIVATED	0x0U
#define DDR32SS_PMCTRL_DATA_RETENTION_ACTIVATED		0x6U
#define DDR32SS_PMCTRL_LATCH_LOAD_SHIFT		31U
#define DDR32SS_PMCTRL_LATCH_LOAD_WIDTH		1U
#define DDR32SS_PMCTRL_LATCH_CLOSED		0x0U
#define DDR32SS_PMCTRL_LATCH_OPEN		0x1U

/* MAIN PLL MMR Base */
#define MAIN_PLL_MMR_BASE			(0x04060000UL)

/* MAIN PLL MMR Registers used in FSP sequence */
#define MAIN_PLL0_HSDIV2_CTRL		(MAIN_PLL_MMR_BASE + (0x88UL))
#define MAIN_PLL0_HSDIV2_CTRL_HSDIV_SHIFT	0U
#define MAIN_PLL0_HSDIV2_CTRL_HSDIV_WIDTH	7U
#define MAIN_PLL0_HSDIV2_CTRL_FSP0_DIV_VAL	0x4FU	/* FSP0 divider value */
#define MAIN_PLL0_HSDIV2_CTRL_FSP1_DIV_VAL	0x09U	/* FSP1 divider value */
#define MAIN_PLL0_HSDIV2_CTRL_FSP2_DIV_VAL	0x04U	/* FSP2 divider value */

#define TIMEOUT_VALUE				10000U

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

static struct emif_handle_s emif_handle;
static uint32_t ddrss_save_restore[NUM_ALL_DDR_REG];
static bool ddrss_is_fsp_supported;
static uint32_t dram_class;
static struct ddrss_sscfg_regs ddrss_sscfg_regs;

/* Poll until PI and CTL initialisation complete. */
static int32_t poll_for_init_completion(struct emif_handle_s *h)
{
	uint32_t timeout;

	/* Poll for PI Init completion */
	timeout = TIMEOUT_VALUE;
	while (((mmio_read_32(h->ctl_cfg_base_addr +
			      CTLCFG_DENALI_PI_(REG_INIT_DONE))) &
		PI_INT_INIT_STATUS) != PI_INT_INIT_STATUS) {
		if (timeout == 0U) {
			return -ETIMEDOUT;
		}
		timeout--;
	}

	/* Poll for CTL Init completion */
	timeout = TIMEOUT_VALUE;
	while (((mmio_read_32(h->ctl_cfg_base_addr +
			      CTLCFG_DENALI_CTL_(REG_CTL_INT_STATUS))) &
		CTL_INT_INIT_STATUS) != CTL_INT_INIT_STATUS) {
		if (timeout == 0U) {
			return -ETIMEDOUT;
		}
		timeout--;
	}

	return 0;
}

/*
 * Write a bit field into an MMR register.
 *
 * mmr_address: address of the register.
 * field_value: value to place in the field.
 * width:       width of the field in bits.
 * leftshift:   least-significant bit position of the field.
 */
static void write_mmr_field(uintptr_t mmr_address,
			    uint32_t field_value,
			    uint32_t width, uint32_t leftshift)
{
	uint32_t val;
	uint32_t mask;

	assert(width > 0U);
	assert(width < 32U);
	assert((width + leftshift) <= 32U);

	val = mmio_read_32(mmr_address);
	mask = ~(((1U << width) - 1U) << leftshift);
	val &= mask;
	val |= (field_value << leftshift);
	mmio_write_32(mmr_address, val);
}

/* Enable DDR data retention mode via WKUP_CTRL_MMR DDR32SS_PMCTRL. */
static int32_t enable_ddr_data_retention(void)
{
	uint32_t val;
	uint32_t timeout;

	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_DATA_RETENTION_ACTIVATED,
			DDR32SS_PMCTRL_DATA_RETENTION_WIDTH,
			DDR32SS_PMCTRL_DATA_RETENTION_SHIFT);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_LATCH_OPEN,
			DDR32SS_PMCTRL_LATCH_LOAD_WIDTH,
			DDR32SS_PMCTRL_LATCH_LOAD_SHIFT);

	timeout = TIMEOUT_VALUE;
	do {
		val = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL);
		if (timeout == 0U) {
			return -ETIMEDOUT;
		}
		timeout--;
	} while (val != ((DDR32SS_PMCTRL_LATCH_OPEN <<
			  DDR32SS_PMCTRL_LATCH_LOAD_SHIFT) |
			 DDR32SS_PMCTRL_DATA_RETENTION_ACTIVATED));

	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_LATCH_CLOSED,
			DDR32SS_PMCTRL_LATCH_LOAD_WIDTH,
			DDR32SS_PMCTRL_LATCH_LOAD_SHIFT);
	return 0;
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
 * Return 0 on success; negative error code otherwise.
 */
static int32_t execute_ddr_fsp_seq(uint8_t fsp_point)
{
	uint32_t req;
	uint32_t req_type;
	uint32_t timeout;
	uint32_t int_status;

	/* Wait for controller busy signal to be de-asserted */
	timeout = TIMEOUT_VALUE;
	while (((mmio_read_32(DDRSS0_CTRL_BASE +
			      CTLCFG_DENALI_CTL_(REG_BUSY)) &
		 CTL_BUSY_BIT) == CTL_BUSY_BIT) && (timeout > 0U)) {
		timeout--;
	}
	if (timeout == 0U) {
		return -ETIMEDOUT;
	}

	/* Set valid data for FSP points to initiate DFS request */
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(REG_MR_FSP_VALID_F0),
			DENALI_CTL_MR_FSP_DATA_VALID_VAL,
			DENALI_CTL_MR_FSP_DATA_VALID_WIDTH,
			DENALI_CTL_MR_FSP_DATA_VALID_F0_SHIFT);
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(REG_MR_FSP_VALID_F1F2),
			DENALI_CTL_MR_FSP_DATA_VALID_VAL,
			DENALI_CTL_MR_FSP_DATA_VALID_WIDTH,
			DENALI_CTL_MR_FSP_DATA_VALID_F2_SHIFT);
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(REG_MR_FSP_VALID_F1F2),
			DENALI_CTL_MR_FSP_DATA_VALID_VAL,
			DENALI_CTL_MR_FSP_DATA_VALID_WIDTH,
			DENALI_CTL_MR_FSP_DATA_VALID_F1_SHIFT);

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
		return -ETIMEDOUT;
	}

	/* Change the PLL frequency as per the requested FSP point */
	req_type = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
				 DDR4_FSP_CLKCHNG_REQ) &
		    DDR4_FSP_CLKCHNG_REQ_REQ_TYPE_MASK);
	if (req_type == CHNG_DDR4_FSP_REQ_FSP0) {
		write_mmr_field(MAIN_PLL0_HSDIV2_CTRL,
				MAIN_PLL0_HSDIV2_CTRL_FSP0_DIV_VAL,
				MAIN_PLL0_HSDIV2_CTRL_HSDIV_WIDTH,
				MAIN_PLL0_HSDIV2_CTRL_HSDIV_SHIFT);
	} else if (req_type == CHNG_DDR4_FSP_REQ_FSP1) {
		write_mmr_field(MAIN_PLL0_HSDIV2_CTRL,
				MAIN_PLL0_HSDIV2_CTRL_FSP1_DIV_VAL,
				MAIN_PLL0_HSDIV2_CTRL_HSDIV_WIDTH,
				MAIN_PLL0_HSDIV2_CTRL_HSDIV_SHIFT);
	} else if (req_type == CHNG_DDR4_FSP_REQ_FSP2) {
		write_mmr_field(MAIN_PLL0_HSDIV2_CTRL,
				MAIN_PLL0_HSDIV2_CTRL_FSP2_DIV_VAL,
				MAIN_PLL0_HSDIV2_CTRL_HSDIV_WIDTH,
				MAIN_PLL0_HSDIV2_CTRL_HSDIV_SHIFT);
	} else {
		return -EINVAL;
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
		return -ETIMEDOUT;
	}

	/* Clear the ACK bit */
	mmio_write_32(WKUP_CTRL_MMR_SEC_4_BASE + DDR4_FSP_CLKCHNG_ACK,
		      DDR4_FSP_CLKCHNG_ACK_CLEAR);
	dsb();

	/* Wait for DDR to acknowledge the software-initiated FSP request */
	timeout = TIMEOUT_VALUE;
	req = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
			    CHNG_DDR4_FSP_ACK) &
	       CHNG_DDR4_FSP_ACK_ACK);
	while ((req == CHNG_DDR4_FSP_ACK_IN_PROG) && (timeout > 0U)) {
		req = (mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
				    CHNG_DDR4_FSP_ACK) &
		       CHNG_DDR4_FSP_ACK_ACK);
		timeout--;
	}
	if (timeout == 0U) {
		return -ETIMEDOUT;
	}

	/* Read the error bit */
	if ((mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
			  CHNG_DDR4_FSP_ACK) &
	     CHNG_DDR4_FSP_ACK_ERROR) != 0U) {
		return -EIO;
	}

	/* De-assert the software-initiated FSP request */
	req = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE + CHNG_DDR4_FSP_REQ);
	req &= ~CHNG_DDR4_FSP_REQ_REQ;
	mmio_write_32(WKUP_CTRL_MMR_SEC_4_BASE + CHNG_DDR4_FSP_REQ, req);
	dsb();

	/* Check the status of interrupts related to frequency scaling */
	timeout = TIMEOUT_VALUE;
	int_status = (mmio_read_32(DDRSS0_CTRL_BASE +
				   CTLCFG_DENALI_CTL_(REG_DFS_STATUS)) >>
		      INT_STATUS_DFS_OFFSET);
	while ((int_status == 0U) && (timeout > 0U)) {
		int_status = (mmio_read_32(DDRSS0_CTRL_BASE +
					   CTLCFG_DENALI_CTL_(REG_DFS_STATUS)) >>
			      INT_STATUS_DFS_OFFSET);
		timeout--;
	}
	if (timeout == 0U) {
		return -ETIMEDOUT;
	}

	/* Acknowledge all DFS interrupts */
	mmio_write_32(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(REG_DFS_ACK), int_status);

	/* Check if any error occurred */
	if ((int_status & DFS_INT_ERROR_MASK) != 0U) {
		return -EIO;
	}

	return 0;
}

/* Save all read/write subsystem configuration registers. */
static void save_ss_config_registers(struct emif_handle_s *h)
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
				     (i * DDR_REG_STRIDE));
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
				     (i * DDR_REG_STRIDE));
	}

	/*
	 * Save PHY Test Control Register 12 (0x1B0).
	 * Note: register 11 at 0x1AC is reserved.
	 */
	ddrss_sscfg_regs.phy_test_ctl_12_reg =
		mmio_read_32(base + DDRSS_SSCFG_PHY_TEST_CTL_12_REG);
}

/* Restore subsystem configuration registers previously saved by save_ss_config_registers(). */
static void restore_ss_config_registers(struct emif_handle_s *h)
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
			      (i * DDR_REG_STRIDE),
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
			      DDRSS_SSCFG_PHY_TEST_CTL_REGS_OFFS + (i * DDR_REG_STRIDE),
			      ddrss_sscfg_regs.phy_test_ctl_regs[i]);
	}

	/*
	 * Restore PHY Test Control Register 12 (0x1B0).
	 * Note: register 11 at 0x1AC is reserved.
	 */
	mmio_write_32(base + DDRSS_SSCFG_PHY_TEST_CTL_12_REG,
		      ddrss_sscfg_regs.phy_test_ctl_12_reg);
}

int32_t k3low_put_ddr_in_rtc_lpm(void)
{
	uint32_t lp_status;
	uint32_t timeout;
	int32_t ret;

	/* disable auto entry / exit */
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(REG_LP_AUTO),
			DENALI_LP_AUTO_ENTRY_DISABLE_VAL,
			DENALI_LP_AUTO_ENTRY_EN_WIDTH,
			DENALI_LP_AUTO_ENTRY_EN_SHIFT);
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(REG_LP_AUTO),
			DENALI_LP_AUTO_EXIT_DISABLE_VAL,
			DENALI_LP_AUTO_EXIT_EN_WIDTH,
			DENALI_LP_AUTO_EXIT_EN_SHIFT);

	ret = execute_ddr_fsp_seq(CHNG_DDR4_FSP_REQ_FSP0);
	if (ret != 0) {
		return ret;
	}

	/* Program Self Refresh mode */
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(REG_LP_CMD),
			LP_MODE_LONG_SELF_REFRESH,
			DENALI_CTL_LP_CMD_WIDTH,
			DENALI_CTL_LP_CMD_SHIFT);
	/* Poll for Self Refresh Mode change */
	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(REG_INT_MASK_LP),
			DENALI_CTL_INT_MASK_LP_VAL,
			DENALI_CTL_INT_MASK_LP_WIDTH,
			DENALI_CTL_INT_MASK_LP_SHIFT);
	timeout = TIMEOUT_VALUE;
	do {
		lp_status = (mmio_read_32(DDRSS0_CTRL_BASE +
					  CTLCFG_DENALI_CTL_(REG_INT_STATUS_LP)) &
			     (DENALI_CTL_INT_STATUS_LP_BIT <<
			      DENALI_CTL_INT_STATUS_LP_SHIFT));
		if (timeout == 0U) {
			return -ETIMEDOUT;
		}
		timeout--;
	} while (lp_status != (DENALI_CTL_INT_STATUS_LP_BIT <<
			       DENALI_CTL_INT_STATUS_LP_SHIFT));

	write_mmr_field(DDRSS0_CTRL_BASE + CTLCFG_DENALI_CTL_(REG_INT_ACK_LP),
			DENALI_CTL_INT_ACK_LP_VAL,
			DENALI_CTL_INT_ACK_LP_WIDTH,
			DENALI_CTL_INT_ACK_LP_SHIFT);
	timeout = TIMEOUT_VALUE;
	do {
		lp_status = ((mmio_read_32(DDRSS0_CTRL_BASE +
					   CTLCFG_DENALI_CTL_(REG_LP_AUTO)) &
			      DENALI_LP_MODE_MASK) >> DENALI_LP_MODE_SHIFT);
		if (timeout == 0U) {
			return -ETIMEDOUT;
		}
		timeout--;
	} while (lp_status != LP_STATUS_LPDDR4_LONG_SELF_REFRESH);

	/*
	 * Enable DDR data retention: write b0110 to
	 * WKUP_CTRL_MMR.DDR32SS_PMCTRL.data_retention.
	 */
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_DATA_RETENTION_ACTIVATED,
			DDR32SS_PMCTRL_DATA_RETENTION_WIDTH,
			DDR32SS_PMCTRL_DATA_RETENTION_SHIFT);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_LATCH_OPEN,
			DDR32SS_PMCTRL_LATCH_LOAD_WIDTH,
			DDR32SS_PMCTRL_LATCH_LOAD_SHIFT);
	timeout = TIMEOUT_VALUE;
	do {
		lp_status = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
					 DDR32SS_PMCTRL);
		if (timeout == 0U) {
			return -ETIMEDOUT;
		}
		timeout--;
	} while (lp_status != ((DDR32SS_PMCTRL_LATCH_OPEN <<
				DDR32SS_PMCTRL_LATCH_LOAD_SHIFT) |
			       DDR32SS_PMCTRL_DATA_RETENTION_ACTIVATED));

	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_LATCH_CLOSED,
			DDR32SS_PMCTRL_LATCH_LOAD_WIDTH,
			DDR32SS_PMCTRL_LATCH_LOAD_SHIFT);

	return 0;
}

/* Configure EMIF base addresses for the single DDR subsystem instance. */
static void emif_instance_select(struct emif_handle_s *h)
{
	h->ss_cfg_base_addr = (uintptr_t)DDRSS0_SSCFG_BASE;
	h->ctl_cfg_base_addr = (uintptr_t)DDRSS0_CTRL_BASE;
}

/* Start PI and CTL initialisation (set PI_START=1 and START=1). */
static void start_pi_ctl_init(struct emif_handle_s *h)
{
	uint32_t wr_init_val;

	wr_init_val = ((dram_class << DDR_MEM_CLASS_SHIFT) | DDR_MEM_INIT_START_BIT);
	/* Set START bit in register for PI module */
	mmio_write_32(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(REG_PI_0),
		      wr_init_val);

	k3low_lpm_delay_1us();

	/* Set START bit in register for controller */
	mmio_write_32(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(REG_CTL_0),
		      wr_init_val);
}

/* Save all DDR controller, PI and PHY registers to the WKUP SRAM buffer. */
static void save_ddr_registers(struct emif_handle_s *h)
{
	uint32_t i;
	uint32_t j;
	uint8_t current_freq_set;

	/* DDRSS Memory Base */
	uintptr_t ctl_base = h->ctl_cfg_base_addr;
	uintptr_t pi_base =
		h->ctl_cfg_base_addr + DDRSS_PI_REGISTER_BLOCK_OFFS;
	uintptr_t phy_data0_base =
		h->ctl_cfg_base_addr + DDRSS_DATA_SLICE_0_REGISTER_BLOCK_OFFS;
	uintptr_t phy_data1_base =
		h->ctl_cfg_base_addr + DDRSS_DATA_SLICE_1_REGISTER_BLOCK_OFFS;
	uintptr_t phy_addr0_base =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_0_REGISTER_BLOCK_OFFS;
	uintptr_t phy_addr1_base =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_1_REGISTER_BLOCK_OFFS;
	uintptr_t phy_addr2_base =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_2_REGISTER_BLOCK_OFFS;
	uintptr_t phy_core_base =
		h->ctl_cfg_base_addr + DDRSS_PHY_CORE_REGISTER_BLOCK_OFFS;

	ddrss_is_fsp_supported = false;

	save_ss_config_registers(h);

	/*
	 * Update PI_INIT_WORK_FREQ and INIT_FREQ based on the current
	 * operating frequency set.
	 */
	current_freq_set =
		(uint8_t)((mmio_read_32(h->ctl_cfg_base_addr +
					CTLCFG_DENALI_PI_(REG_ACTIVE_FREQ)) >>
			   DDR_MEM_ACTIVE_FREQ_SHIFT) &
			  DDR_MEM_ACTIVE_FREQ_MASK);
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(REG_INIT_WORK_FREQ),
			current_freq_set,
			DENALI_PI_INIT_WORK_FREQ_WIDTH,
			DENALI_PI_INIT_WORK_FREQ_SHIFT);
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(REG_TREF_F1),
			current_freq_set,
			DENALI_CTL_TREF_F1_WIDTH,
			DENALI_CTL_TREF_F1_SHIFT);

	/* Save the class of DRAM */
	dram_class = ((mmio_read_32(ctl_base) &
		       DDR_MEM_CLASS_MASK) >> DDR_MEM_CLASS_SHIFT);

	j = 0U;
	for (i = 0U; i < NUM_DDR_CTL_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(ctl_base + (i * DDR_REG_STRIDE));
	}
	for (i = 0U; i < NUM_DDR_PI_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(pi_base + (i * DDR_REG_STRIDE));
	}
	/* Save the current operating frequency register set (set 2) */
	for (i = 0U; i < NUM_DDR_DATA_0_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(phy_data0_base + (i * DDR_REG_STRIDE));
	}
	for (i = 0U; i < NUM_DDR_DATA_1_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(phy_data1_base + (i * DDR_REG_STRIDE));
	}
	for (i = 0U; i < NUM_DDR_ADDR_0_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(phy_addr0_base + (i * DDR_REG_STRIDE));
	}
	for (i = 0U; i < NUM_DDR_ADDR_1_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(phy_addr1_base + (i * DDR_REG_STRIDE));
	}
	for (i = 0U; i < NUM_DDR_ADDR_2_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(phy_addr2_base + (i * DDR_REG_STRIDE));
	}

	/*
	 * Multicast is disabled when multiple FSPs are configured; the
	 * MULTICAST_EN bit being clear indicates FSP support.
	 */
	if (((mmio_read_32(ctl_base +
			   CTLCFG_DENALI_PHY_(REG_FREQ_SEL))) &
	     DDRSS_PHY_CORE_REGISTER_1281_MULTICAST_EN) == 0U) {
		ddrss_is_fsp_supported = true;
	}
	for (i = 0U; i < NUM_DDR_PHY_REG; i++, j++) {
		ddrss_save_restore[j] =
			mmio_read_32(phy_core_base + (i * DDR_REG_STRIDE));
	}
	if (ddrss_is_fsp_supported) {
		/* Write phy_freq_sel_index = 1 to save the second frequency set */
		mmio_write_32(ctl_base + CTLCFG_DENALI_PHY_(REG_FREQ_SEL),
			      DDRSS_PHY_CORE_REGISTER_1281_FREQ_SEL_INDEX);

		/* Save the second register set */
		for (i = 0U; i < NUM_DDR_DATA_0_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(phy_data0_base + (i * DDR_REG_STRIDE));
		}
		for (i = 0U; i < NUM_DDR_DATA_1_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(phy_data1_base + (i * DDR_REG_STRIDE));
		}
		for (i = 0U; i < NUM_DDR_ADDR_0_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(phy_addr0_base + (i * DDR_REG_STRIDE));
		}
		for (i = 0U; i < NUM_DDR_ADDR_1_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(phy_addr1_base + (i * DDR_REG_STRIDE));
		}
		for (i = 0U; i < NUM_DDR_ADDR_2_REG; i++, j++) {
			ddrss_save_restore[j] =
				mmio_read_32(phy_addr2_base + (i * DDR_REG_STRIDE));
		}
		/* Save the DDR PHY set with correct frequency select index */
		for (i = 0U; i < NUM_DDR_PHY_REG; i++, j++) {
			if (i == DDRSS_PHY_CORE_REGISTER_1281_POS) {
				ddrss_save_restore[j] =
					DDRSS_PHY_CORE_REGISTER_1281_FREQ_SEL_INDEX |
					DDRSS_PHY_CORE_REGISTER_1281_MULTICAST_EN;
			} else {
				ddrss_save_restore[j] =
					mmio_read_32(phy_core_base + (i * DDR_REG_STRIDE));
			}
		}
	}
}

/* Restore DDR controller, PI and PHY registers from the WKUP SRAM buffer. */
static void restore_ddr_registers(struct emif_handle_s *h)
{
	uint32_t i;
	uint32_t j;

	/* DDRSS Memory Base */
	uintptr_t ctl_base = h->ctl_cfg_base_addr;
	uintptr_t pi_base =
		h->ctl_cfg_base_addr + DDRSS_PI_REGISTER_BLOCK_OFFS;
	uintptr_t phy_data0_base =
		h->ctl_cfg_base_addr + DDRSS_DATA_SLICE_0_REGISTER_BLOCK_OFFS;
	uintptr_t phy_data1_base =
		h->ctl_cfg_base_addr + DDRSS_DATA_SLICE_1_REGISTER_BLOCK_OFFS;
	uintptr_t phy_addr0_base =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_0_REGISTER_BLOCK_OFFS;
	uintptr_t phy_addr1_base =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_1_REGISTER_BLOCK_OFFS;
	uintptr_t phy_addr2_base =
		h->ctl_cfg_base_addr + DDRSS_ADDRESS_SLICE_2_REGISTER_BLOCK_OFFS;
	uintptr_t phy_core_base =
		h->ctl_cfg_base_addr + DDRSS_PHY_CORE_REGISTER_BLOCK_OFFS;

	mmio_write_32(ctl_base + CTLCFG_DENALI_CTL_(REG_CTL_0),
		      dram_class << DDR_MEM_CLASS_SHIFT);
	/* Skip the first CTL register write (already written above) */
	j = 1U;
	for (i = 1U; i < NUM_DDR_CTL_REG; i++, j++) {
		mmio_write_32(ctl_base + (i * DDR_REG_STRIDE),
			      ddrss_save_restore[j]);
	}
	mmio_write_32(ctl_base + CTLCFG_DENALI_PI_(REG_PI_0),
		      dram_class << DDR_MEM_CLASS_SHIFT);
	/* Skip the first PI register write */
	j++;
	for (i = 1U; i < NUM_DDR_PI_REG; i++, j++) {
		mmio_write_32(pi_base + (i * DDR_REG_STRIDE),
			      ddrss_save_restore[j]);
	}

	/* Restore the second frequency set conditionally */
	if (ddrss_is_fsp_supported) {
		/* Advance j to the second set saved after the first PHY set */
		j = j + NUM_ALL_PHY_REG;

		for (i = 0U; i < NUM_DDR_DATA_0_REG; i++, j++) {
			mmio_write_32(phy_data0_base + (i * DDR_REG_STRIDE),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_DATA_1_REG; i++, j++) {
			mmio_write_32(phy_data1_base + (i * DDR_REG_STRIDE),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_ADDR_0_REG; i++, j++) {
			mmio_write_32(phy_addr0_base + (i * DDR_REG_STRIDE),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_ADDR_1_REG; i++, j++) {
			mmio_write_32(phy_addr1_base + (i * DDR_REG_STRIDE),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_ADDR_2_REG; i++, j++) {
			mmio_write_32(phy_addr2_base + (i * DDR_REG_STRIDE),
				      ddrss_save_restore[j]);
		}
		for (i = 0U; i < NUM_DDR_PHY_REG; i++, j++) {
			mmio_write_32(phy_core_base + (i * DDR_REG_STRIDE),
				      ddrss_save_restore[j]);
		}

		/* Disable multicast before restoring the first set */
		mmio_write_32(ctl_base + CTLCFG_DENALI_PHY_(REG_FREQ_SEL),
			      0U);

		/* Adjust j back to point to the first register set */
		assert(j >= (NUM_ALL_PHY_REG << 1U));
		j = j - (NUM_ALL_PHY_REG << 1U);
	}

	/* Restore the first frequency register set */
	for (i = 0U; i < NUM_DDR_DATA_0_REG; i++, j++) {
		mmio_write_32(phy_data0_base + (i * DDR_REG_STRIDE),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_DATA_1_REG; i++, j++) {
		mmio_write_32(phy_data1_base + (i * DDR_REG_STRIDE),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_ADDR_0_REG; i++, j++) {
		mmio_write_32(phy_addr0_base + (i * DDR_REG_STRIDE),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_ADDR_1_REG; i++, j++) {
		mmio_write_32(phy_addr1_base + (i * DDR_REG_STRIDE),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_ADDR_2_REG; i++, j++) {
		mmio_write_32(phy_addr2_base + (i * DDR_REG_STRIDE),
			      ddrss_save_restore[j]);
	}
	for (i = 0U; i < NUM_DDR_PHY_REG; i++, j++) {
		mmio_write_32(phy_core_base + (i * DDR_REG_STRIDE),
			      ddrss_save_restore[j]);
	}
}

/* Restore DDR registers, release data retention, and restart the controller. */
static int32_t ddr_deep_sleep_resume_sequence(struct emif_handle_s *h)
{
	uint32_t lp_status;
	uint32_t timeout;
	int32_t ret;

	restore_ss_config_registers(h);

	/* Write back the copied registers */
	restore_ddr_registers(h);

	/* Configure PHY and PI settings for resume sequence */
	/* PHY_1306: Set DFI input 0 - configures DFI interface input settings */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PHY_(REG_DFI_INPUT0),
			DENALI_PHY_DFI_INPUT0_VAL,
			DENALI_PHY_DFI_INPUT0_WIDTH,
			DENALI_PHY_DFI_INPUT0_SHIFT);
	/* PI_4: Disable PI_INIT_LVL_EN - disable initialization leveling */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(REG_INIT_LVL_EN),
			DENALI_PI_INIT_LVL_EN_VAL,
			DENALI_PI_INIT_LVL_EN_WIDTH,
			DENALI_PI_INIT_LVL_EN_SHIFT);
	/* CTL_20: Enable PHY_INDEP_TRAIN_MODE - enable independent PHY training mode */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(REG_PHY_INDEP_TRAIN),
			DENALI_CTL_PHY_INDEP_TRAIN_MODE_VAL,
			DENALI_CTL_PHY_INDEP_TRAIN_MODE_WIDTH,
			DENALI_CTL_PHY_INDEP_TRAIN_MODE_SHIFT);
	/* CTL_21: Enable PHY_INDEP_INIT_MODE - enable independent PHY initialization mode */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(REG_PHY_INDEP_INIT),
			DENALI_CTL_PHY_INDEP_INIT_MODE_VAL,
			DENALI_CTL_PHY_INDEP_INIT_MODE_WIDTH,
			DENALI_CTL_PHY_INDEP_INIT_MODE_SHIFT);
	/* PI_138: Enable PI_DLL_RST - enable DLL reset */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(REG_DLL_RST),
			DENALI_PI_DLL_RST_VAL,
			DENALI_PI_DLL_RST_WIDTH,
			DENALI_PI_DLL_RST_SHIFT);
	/* CTL_106: Disable PWRUP_SREFRESH_EXIT - disable power-up self-refresh exit */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(REG_PWRUP_SREFRESH),
			DENALI_CTL_PWRUP_SREFRESH_EXIT_VAL,
			DENALI_CTL_PWRUP_SREFRESH_EXIT_WIDTH,
			DENALI_CTL_PWRUP_SREFRESH_EXIT_SHIFT);
	/* PI_134: Enable PI_PWRUP_SREFRESH_EXIT - enable PI power-up self-refresh exit */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(REG_PWRUP_SREFRESH_PI),
			DENALI_PI_PWRUP_SREFRESH_EXIT_VAL,
			DENALI_PI_PWRUP_SREFRESH_EXIT_WIDTH,
			DENALI_PI_PWRUP_SREFRESH_EXIT_SHIFT);
	/* PI_138: Enable PI_DRAM_INIT_EN - enable DRAM initialization */
	write_mmr_field(h->ctl_cfg_base_addr + CTLCFG_DENALI_PI_(REG_DLL_RST),
			DENALI_PI_DRAM_INIT_EN_VAL,
			DENALI_PI_DRAM_INIT_EN_WIDTH,
			DENALI_PI_DRAM_INIT_EN_SHIFT);

	/* De-assert data retention pin and wake control bits */
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_LATCH_CLOSED,
			DDR32SS_PMCTRL_LATCH_LOAD_WIDTH,
			DDR32SS_PMCTRL_LATCH_LOAD_SHIFT);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_DATA_RETENTION_DEACTIVATED,
			DDR32SS_PMCTRL_DATA_RETENTION_WIDTH,
			DDR32SS_PMCTRL_DATA_RETENTION_SHIFT);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_LATCH_OPEN,
			DDR32SS_PMCTRL_LATCH_LOAD_WIDTH,
			DDR32SS_PMCTRL_LATCH_LOAD_SHIFT);
	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_DATA_RETENTION_DEACTIVATED,
			DDR32SS_PMCTRL_DATA_RETENTION_WIDTH,
			DDR32SS_PMCTRL_DATA_RETENTION_SHIFT);
	timeout = TIMEOUT_VALUE;
	do {
		lp_status = mmio_read_32(WKUP_CTRL_MMR_SEC_4_BASE +
					 DDR32SS_PMCTRL);
		if (timeout == 0U) {
			return -ETIMEDOUT;
		}
		timeout--;
	} while (lp_status != ((DDR32SS_PMCTRL_LATCH_OPEN <<
				DDR32SS_PMCTRL_LATCH_LOAD_SHIFT) |
			       DDR32SS_PMCTRL_DATA_RETENTION_DEACTIVATED));

	write_mmr_field((WKUP_CTRL_MMR_SEC_4_BASE + DDR32SS_PMCTRL),
			DDR32SS_PMCTRL_LATCH_CLOSED,
			DDR32SS_PMCTRL_LATCH_LOAD_WIDTH,
			DDR32SS_PMCTRL_LATCH_LOAD_SHIFT);

	/* Start Initialization [PI_START=1 and START=1] */
	start_pi_ctl_init(h);

	/* Wait for INIT_DONE interrupt */
	ret = poll_for_init_completion(h);
	return ret;
}

/* Put DDR into self-refresh mode and enable data retention. */
static int32_t enter_lpm_self_refresh(struct emif_handle_s *h)
{
	uint32_t lp_status;
	uint32_t lp_status_expected;
	uint32_t reg_val;
	uint32_t timeout;

	/* Program Self Refresh mode: self-refresh long with memory clock gating */
	mmio_write_32(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(REG_LP_CMD),
			(LP_MODE_LONG_SELF_REFRESH << DENALI_CTL_LP_CMD_SHIFT));

	if (dram_class == LPDDR4_DRAM_CLASS_REG_VALUE) {
		lp_status_expected = LP_STATUS_LPDDR4_LONG_SELF_REFRESH;
	} else if (dram_class == DDR4_DRAM_CLASS_REG_VALUE) {
		lp_status_expected = LP_STATUS_DDR4_LONG_SELF_REFRESH;
	} else {
		return -ENOTSUP;
	}

	/* Poll for Self Refresh Mode change */
	timeout = TIMEOUT_VALUE;
	do {
		reg_val = mmio_read_32(h->ctl_cfg_base_addr + CTLCFG_DENALI_CTL_(REG_LP_AUTO));
		lp_status = (reg_val & DENALI_LP_MODE_MASK) >> DENALI_LP_MODE_SHIFT;

		if (timeout == 0U) {
			return -ETIMEDOUT;
		}
		timeout--;
	} while (lp_status != lp_status_expected);

	return 0;
}

int32_t k3low_ddr_deep_sleep_suspend_sequence(void)
{
	int32_t ret;

	/*
	 * Save DDR register context in WKUP SRAM and put the DDR in
	 * self refresh.
	 */
	emif_instance_select(&emif_handle);
	save_ddr_registers(&emif_handle);

	ret = enter_lpm_self_refresh(&emif_handle);
	if (ret != 0) {
		return ret;
	}

	/*
	 * Enable DDR data retention: write b0110 to
	 * WKUP_CTRL_MMR.DDR32SS_PMCTRL.data_retention.
	 */
	return enable_ddr_data_retention();
}

int32_t k3low_ddr_deep_sleep_resume_sequence(void)
{
	/*
	 * Restore DDR controller context, take DDR out of self refresh,
	 * and remove data retention.
	 */
	emif_instance_select(&emif_handle);
	return ddr_deep_sleep_resume_sequence(&emif_handle);
}
