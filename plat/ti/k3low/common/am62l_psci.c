/*
 * Copyright (C) 2025-2026 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdbool.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <lib/el3_runtime/cpu_data.h>
#include <lib/mmio.h>
#include <lib/psci/psci.h>
#include <plat/common/platform.h>

#include <firewall.h>
#include <k3_console.h>
#include <k3_lpm_ctrl.h>
#include <ti_clk.h>
#include <ti_device_pm.h>
#include <ti_plat_scmi_def.h>
#include <ti_sci.h>
#include <ti_sci_protocol.h>

#include <k3_gicv3.h>
#include <platform_def.h>
#include <ti_devices.h>

uintptr_t am62l_sec_entrypoint;
uintptr_t am62l_sec_entrypoint_glob;
void  __aligned(16) jump_to_atf_func(void *unused);

static int am62l_pwr_domain_on(u_register_t mpidr)
{
	int32_t core, ret;
	uint8_t proc_id;

	core = plat_core_pos_by_mpidr(mpidr);
	if (core < 0) {
		ERROR("Could not get target core id: %d\n", core);
		return PSCI_E_INTERN_FAIL;
	}

	proc_id = (uint8_t)(PLAT_PROC_START_ID + (uint32_t)core);

	ret = ti_sci_proc_request(proc_id);
	if (ret != 0) {
		ERROR("Request for processor ID 0x%x failed: %d\n",
				proc_id, ret);
		return PSCI_E_INTERN_FAIL;
	}

	ret = ti_sci_proc_set_boot_cfg(proc_id, am62l_sec_entrypoint, 0, 0);
	if (ret != 0) {
		ERROR("Request to set core boot address failed: %d\n", ret);
		return PSCI_E_INTERN_FAIL;
	}

	/* sanity check these are off before starting a core */
	ret = ti_sci_proc_set_boot_ctrl(proc_id,
			0, PROC_BOOT_CTRL_FLAG_ARMV8_L2FLUSHREQ |
			PROC_BOOT_CTRL_FLAG_ARMV8_AINACTS |
			PROC_BOOT_CTRL_FLAG_ARMV8_ACINACTM);
	if (ret != 0) {
		ERROR("Request to clear boot config failed: %d\n", ret);
		return PSCI_E_INTERN_FAIL;
	}

	/* Power up device and enable clocks */
	ti_device_id_enable_clocks(AM62LX_DEV_A53_0 + core);
	ti_device_id_power_up(AM62LX_DEV_A53_0 + core);

	return PSCI_E_SUCCESS;
}

static int am62l_pwr_domain_off_early(const psci_power_state_t *target_state)
{
	int32_t core;

	/* At very least the local core should be powering down */
	assert(((target_state)->pwr_domain_state[MPIDR_AFFLVL0]) == PLAT_MAX_OFF_STATE);

	core = plat_my_core_pos();

	/*
	 * Disable clocks while cache coherency is still active.
	 * This allows atomic clock reference counting to work correctly.
	 *
	 * Clock operations use atomic instructions to safely handle concurrent
	 * access from multiple cores. These atomic instructions require cache
	 * coherency to function properly. The PSCI flow disables cache coherency
	 * between pwr_domain_off() and pwr_domain_pwr_down(), so we must complete all
	 * clock operations here in the early hook where atomic instructions still work.
	 */
	ti_device_id_disable_clocks(AM62LX_DEV_A53_0 + core);

	return PSCI_E_SUCCESS;
}

static void am62l_pwr_domain_off(const psci_power_state_t *target_state)
{
	/* At very least the local core should be powering down */
	assert(((target_state)->pwr_domain_state[MPIDR_AFFLVL0]) == PLAT_MAX_OFF_STATE);

	/* Prevent interrupts from spuriously waking up this cpu */
	k3_gic_cpuif_disable();
}

static void am62l_pwr_down_domain(const psci_power_state_t *target_state)
{
	int32_t core;

	core = plat_my_core_pos();

	VERBOSE("%s: A53 CORE %d: OFF\n", __func__, core);
	/*
	 * Clocks already disabled in pwr_domain_off_early().
	 * Only perform power domain operations here.
	 */
	ti_device_id_power_down(AM62LX_DEV_A53_0 + core);
}

void am62l_pwr_domain_on_finish(const psci_power_state_t *target_state)
{
	k3_gic_pcpu_init();
	k3_gic_cpuif_enable();
}

static void am62l_system_reset(void)
{
	mmio_write_32(WKUP_CTRL_MMR0_BASE + WKUP_CTRL_MMR0_DEVICE_RESET_OFFSET,
		      0x6);

	/* Wait for reset to complete for 500ms before printing error */
	mdelay(500);

	/* Ideally we should not reach here */
	ERROR("%s: Failed to reset device\n", __func__);
}

static void am62l_pwr_domain_suspend(const psci_power_state_t *target_state)
{
	const uint32_t mode = 0U;
	const uint64_t context_save_addr = TIFS_LPM_SAVE_CTX;
	uint32_t core;
	uint32_t proc_id;
	int32_t ret;

	core = plat_my_core_pos();
	assert(core < 2U);

	/* Prevent interrupts from spuriously waking up this cpu */
	k3_gic_cpuif_disable();
	k3_gic_save_context();
	ti_clks_suspend();

	INFO("Started Suspend Sequence in ATF\n");

	/* Isolate the I/Os to allow I/O Daisy chain wakeup */
	ret = k3low_lpm_set_io_isolation(true);
	if (ret != 0) {
		ERROR("%s: IO isolation failed (%d)\n", __func__, ret);
	}

	k3low_lpm_config_magic_words(mode);

	ret = ti_sci_prepare_sleep(mode, context_save_addr, 0U);
	if (ret != 0) {
		ERROR("%s: prepare_sleep failed (%d)\n", __func__, ret);
	}
	INFO("sent prepare message\n");

	k3low_config_wake_sources(true);

	proc_id = PLAT_PROC_START_ID + core;
	ret = ti_sci_enter_sleep(proc_id, mode, am62l_sec_entrypoint);
	if (ret != 0) {
		ERROR("%s: enter_sleep failed (%d)\n", __func__, ret);
	}
	INFO("sent enter sleep message\n");

	k3low_suspend_to_ram(mode);
}

static void am62l_pwr_domain_suspend_finish(const psci_power_state_t *target_state)
{
	int32_t ret;

	/* Update firewall configurations before releasing IO isolation */
	update_fwl_configs();

	/* Remove the I/O isolation */
	ret = k3low_lpm_set_io_isolation(false);
	if (ret != 0) {
		ERROR("%s: IO isolation release failed (%d)\n", __func__, ret);
	}

	/* Initialize the console to provide early debug support */
	k3_console_setup();
	k3low_config_wake_sources(false);
	k3_gic_restore_context();
	k3_gic_cpuif_enable();
	ti_init_scmi_server();

	/* Re-copy LPM stub so the next suspend has a valid entry point */
	ret = k3low_lpm_stub_copy_to_sram();
	if (ret != 0) {
		ERROR("%s: LPM stub copy failed (%d)\n", __func__, ret);
	}

	ret = ti_clks_resume();
	if (ret != 0) {
		ERROR("%s: ti_clks_resume failed (%d)\n", __func__, ret);
	}
}

static void am62l_get_sys_suspend_power_state(psci_power_state_t *req_state)
{
	unsigned int i;

	/* CPU & cluster off, system in retention */
	for (i = MPIDR_AFFLVL0; i <= PLAT_MAX_PWR_LVL; i++) {
		req_state->pwr_domain_state[i] = PLAT_MAX_OFF_STATE;
	}
}

static plat_psci_ops_t am62l_plat_psci_ops = {
	.pwr_domain_on = am62l_pwr_domain_on,
	.pwr_domain_off_early = am62l_pwr_domain_off_early,
	.pwr_domain_off = am62l_pwr_domain_off,
	.pwr_domain_pwr_down = am62l_pwr_down_domain,
	.pwr_domain_on_finish = am62l_pwr_domain_on_finish,
	.system_reset = am62l_system_reset,
	.pwr_domain_suspend = am62l_pwr_domain_suspend,
	.pwr_domain_suspend_finish = am62l_pwr_domain_suspend_finish,
	.get_sys_suspend_power_state = am62l_get_sys_suspend_power_state,
};

void  __aligned(16) jump_to_atf_func(void *unused)
{
	/*
	 * MISRA Deviation observed:
	 * Rule 11.1 (MISRA C:2012) Prohibits conversion performed between a
	 * pointer to a function and another incompatible type.
	 * This conversion is required for handling secure boot entry points.
	 * The conversion is safe as the address is verified before execution.
	 */
	void (*bl31_loc_warm_entry)(void) = (void *)am62l_sec_entrypoint_glob;

	bl31_loc_warm_entry();
}

int plat_setup_psci_ops(uintptr_t sec_entrypoint,
			const plat_psci_ops_t **psci_ops)
{
	am62l_sec_entrypoint_glob = sec_entrypoint;
	/* Note that boot vector reg in sec mmr requires 16B aligned start address */
	am62l_sec_entrypoint = (uint64_t)(void *)&jump_to_atf_func;
	VERBOSE("am62l_sec_entrypoint = 0x%lx\n", am62l_sec_entrypoint);

	*psci_ops = &am62l_plat_psci_ops;

	return 0;
}
