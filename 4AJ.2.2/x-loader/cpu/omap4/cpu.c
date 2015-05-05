/*
 * (C) Copyright 2004-2009 Texas Insturments
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <gj@denx.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * CPU specific code
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/mem.h>
#include <asm/arch/bits.h>
#include <asm/arch/cpu.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/rom_hal_api.h>
#include <asm/arch/clocks.h>
#include <asm/arch/rom_public_api_func.h>


/* See also ARM Ref. Man. */
#define C1_MMU		(1<<0)		/* mmu off/on */
#define C1_ALIGN	(1<<1)		/* alignment faults off/on */
#define C1_DC		(1<<2)		/* dcache off/on */
#define C1_WB		(1<<3)		/* merging write buffer on/off */
#define C1_BIG_ENDIAN	(1<<7)		/* big endian off/on */
#define C1_SYS_PROT	(1<<8)		/* system protection */
#define C1_ROM_PROT	(1<<9)		/* ROM protection */
#define C1_IC		(1<<12)		/* icache off/on */
#define C1_HIGH_VECTORS	(1<<13) /* location of vectors: low/high addresses */
#define RESERVED_1	(0xf << 3)	/* must be 111b for R/W */

#define PL310_POR	5

int cpu_init (void)
{
	unsigned int es_revision;

	es_revision = omap_revision();

	/* OMAP 4430 ES1.0 is the only device rev that does not support
	 * modifying PL310.POR. Thus this is will be applied for any 4430 rev
	 * (except 1.0) and any 4460 */
	if (es_revision > OMAP4430_ES1_0 && get_device_type() != GP_DEVICE) {
		/* Set PL310 Prefetch Offset Register w/PPA svc*/
		omap_smc_ppa(PPA_SERVICE_PL310_POR, 0, 0x7, 1, PL310_POR);
		/* Enable L2 data prefetch */
		omap_smc_rom(ROM_SERVICE_PL310_AUXCR,
			__raw_readl(OMAP44XX_PL310_AUX_CTRL) | 0x10000000);
	/* This ROM svc is availble only for OMAP4430 ES2.2 or any 4460 */
	} else if (es_revision > OMAP4430_ES2_1) {
		/* Set PL310 Prefetch Offset Register using ROM svc */
		omap_smc_rom(ROM_SERVICE_PL310_POR, PL310_POR);
		/* Enable L2 data prefetch */
		omap_smc_rom(ROM_SERVICE_PL310_AUXCR,
			__raw_readl(OMAP44XX_PL310_AUX_CTRL) | 0x10000000);
	}
	/*
	 * Errata: i684
	 * For all ESx.y trimmed and untrimmed units
	 * Override efuse with LPDDR P:16/N:16 and
	 * smart IO P:0/N:0 as per recomendation
	 * OMAP4470 CPU types not impacted.
	 */
	if (es_revision < OMAP4470_ES1_0)
		__raw_writel(0x00084000, SYSCTRL_PADCONF_CORE_EFUSE_2);

	/* For ES2.2
	 * 1. If unit does not have SLDO trim, set override
	 * and force max multiplication factor to ensure
	 * proper SLDO voltage at low OPP's
	 * 2. Trim VDAC value for TV out as recomended to avoid
	 * potential instabilities at low OPP's. VDAC is absent
	 * on OMAP4470 CPUs.
	 */

	/*if MPU_VOLTAGE_CTRL is 0x0 unit is not trimmed*/
	if ((es_revision >= OMAP4460_ES1_0) &&
		(((__raw_readl(IVA_LDOSRAM_VOLTAGE_CTRL) &
					   ~(0x3e0)) == 0x0)) ||
		((es_revision >= OMAP4430_ES2_2) &&
			 (es_revision < OMAP4460_ES1_0) &&
			 (!(__raw_readl(IVA_LDOSRAM_VOLTAGE_CTRL))))) {
		/* Set M factor to max (2.7) */
		__raw_writel(0x0401040f, IVA_LDOSRAM_VOLTAGE_CTRL);
		__raw_writel(0x0401040f, MPU_LDOSRAM_VOLTAGE_CTRL);
		__raw_writel(0x0401040f, CORE_LDOSRAM_VOLTAGE_CTRL);
		if (es_revision < OMAP4470_ES1_0)
			__raw_writel(0x000001c0, SYSCTRL_PADCONF_CORE_EFUSE_1);
	}

	return 0;
}

unsigned int cortex_a9_rev(void)
{

	unsigned int i;

	/* turn off I/D-cache */
	asm ("mrc p15, 0, %0, c0, c0, 0" : "=r" (i));

	return i;
}

unsigned int omap_revision(void)
{
	/*
	 * For some of the ES2/ES1 boards ID_CODE is not reliable:
	 * Also, ES1 and ES2 have different ARM revisions
	 * So use ARM revision for identification
	 */
	unsigned int rev = cortex_a9_rev();
	unsigned int proc_version = 0;

	switch (rev) {
		case MIDR_CORTEX_A9_R0P1:
			return OMAP4430_ES1_0;
		case MIDR_CORTEX_A9_R1P2:
			rev = readl(CONTROL_ID_CODE);
			switch (rev) {
				case OMAP4_CONTROL_ID_CODE_ES2_2:
					return OMAP4430_ES2_2;
				case OMAP4_CONTROL_ID_CODE_ES2_1:
					return OMAP4430_ES2_1;
				case OMAP4_CONTROL_ID_CODE_ES2_0:
					return OMAP4430_ES2_0;
				default:
					return OMAP4430_ES2_0;
			}
		case MIDR_CORTEX_A9_R1P3:
			return OMAP4430_ES2_3;
		case MIDR_CORTEX_A9_R2P10:
			rev = readl(CONTROL_ID_CODE);
			/* For 4460/4470 skip Version Number
			 * and use Ramp System Number only.
			 * There isn't a difference between v1.0/1.1
			 * for x-loader
			 */
			proc_version = (rev >> 28) & 0xf;
			rev &= OMAP4_CONTROL_ID_CODE_RAMP_MASK;
			switch (rev) {
				case OMAP4_CONTROL_ID_CODE_4460_ES1:
					switch (proc_version) {
						case 0x0:
							return OMAP4460_ES1_0;
						case 0x2:
							return OMAP4460_ES1_1;
						default:
							return OMAP44XX_SILICON_ID_INVALID;
					}
				case OMAP4_CONTROL_ID_CODE_4470_ES1:
					return OMAP4470_ES1_0;
				default:
					return OMAP44XX_SILICON_ID_INVALID;
			}
		default:
			return OMAP44XX_SILICON_ID_INVALID;
	}
}

u32 omap4_silicon_type(void)
{
	u32 si_type = readl(STD_FUSE_PROD_ID_1);
	si_type = get_bit_field(si_type, PROD_ID_1_SILICON_TYPE_SHIFT,
		PROD_ID_1_SILICON_TYPE_MASK);
	return si_type;
}


u32 get_device_type(void)
{
	/*
	 * Retrieve the device type: GP/EMU/HS/TST stored in
	 * CONTROL_STATUS
	 */
	return (__raw_readl(CONTROL_STATUS) & DEVICE_MASK) >> 8;
}

unsigned int get_boot_mode(void)
{
	/* retrieve the boot mode stored in scratchpad */
	return (*(volatile unsigned int *)(0x4A326004)) & 0xf;
}

unsigned int get_boot_device(void)
{
	/* retrieve the boot device stored in scratchpad */
	return (*(volatile unsigned int *)(0x4A326000)) & 0xff;
}
unsigned int raw_boot(void)
{
	if (get_boot_mode() == 1)
		return 1;
	else
		return 0;
}

unsigned int fat_boot(void)
{
	if (get_boot_mode() == 2)
		return 1;
	else
		return 0;
}

static void omap_vc_init(u8 hscll, u8 hsclh, u8 scll, u8 sclh)
{
	u32 val;

	val = 0x00 << PRM_VC_CFG_I2C_MODE_HSMCODE_SHIFT;
	if (hscll || hsclh)
		val |= PRM_VC_CFG_I2C_MODE_HSMODEEN_BIT;

	__raw_writel(val, PRM_VC_CFG_I2C_MODE);

	hscll &= PRM_VC_CFG_I2C_CLK_HSCLL_MASK;
	hsclh &= PRM_VC_CFG_I2C_CLK_HSCLH_MASK;
	scll &= PRM_VC_CFG_I2C_CLK_SCLL_MASK;
	sclh &= PRM_VC_CFG_I2C_CLK_SCLH_MASK;

	val = hscll << PRM_VC_CFG_I2C_CLK_HSCLL_SHIFT |
	    hsclh << PRM_VC_CFG_I2C_CLK_HSCLH_SHIFT |
	    scll << PRM_VC_CFG_I2C_CLK_SCLL_SHIFT |
	    sclh << PRM_VC_CFG_I2C_CLK_SCLH_SHIFT;
	__raw_writel(val, PRM_VC_CFG_I2C_CLK);
}

/**
 * omap_vc_bypass_send_value() - Send a data using VC Bypass command
 * @sa:		7 bit I2C slave address of the PMIC
 * @reg_addr:	I2C register address(8 bit) address in PMIC
 * @reg_data:	what 8 bit data to write
 */
static int omap_vc_bypass_send_value(u8 sa, u8 reg_addr, u8 reg_data)
{
	/*
	 * Unfortunately we need to loop here instead of a defined time
	 * use arbitary large value
	 */
	u32 timeout = 0xFFFF;
	u32 reg_val;

	sa &= PRM_VC_VAL_BYPASS_SLAVEADDR_MASK;
	reg_addr &= PRM_VC_VAL_BYPASS_REGADDR_MASK;
	reg_data &= PRM_VC_VAL_BYPASS_DATA_MASK;

	/* program VC to send data */
	reg_val = sa << PRM_VC_VAL_BYPASS_SLAVEADDR_SHIFT |
	    reg_addr << PRM_VC_VAL_BYPASS_REGADDR_SHIFT |
	    reg_data << PRM_VC_VAL_BYPASS_DATA_SHIFT;
	__raw_writel(reg_val, PRM_VC_VAL_BYPASS);

	/* Signal VC to send data */
	__raw_writel(reg_val | PRM_VC_VAL_BYPASS_VALID_BIT, PRM_VC_VAL_BYPASS);

	/* Wait on VC to complete transmission */
	do {
		reg_val = __raw_readl(PRM_VC_VAL_BYPASS) &
		    PRM_VC_VAL_BYPASS_VALID_BIT;
		if (!reg_val)
			break;

		spin_delay(100);
	} while (--timeout);

	/* Cleanup PRM int status reg to leave no traces of interrupts */
	reg_val = __raw_readl(PRM_IRQSTATUS_MPU);
	__raw_writel(reg_val, PRM_IRQSTATUS_MPU);

	/* In case we can do something about it in future.. */
	if (!timeout)
		return -1;

	/* All good.. */
	return 0;
}

static void do_scale_tps62361(u32 reg, u32 val)
{
	u32 l = 0;

	/*
	 * Select SET1 in TPS62361:
	 * VSEL1 is grounded on board. So the following selects
	 * VSEL1 = 0 and VSEL0 = 1
	 */

	/* set GPIO-7 direction as output */
	l = __raw_readl(0x4A310134);
	l &= ~(1 << TPS62361_VSEL0_GPIO);
	__raw_writel(l, 0x4A310134);

	omap_vc_bypass_send_value(TPS62361_I2C_SLAVE_ADDR, reg, val);

	/* set GPIO-7 data-out */
	l = 1 << TPS62361_VSEL0_GPIO;
	__raw_writel(l, 0x4A310194);

}

/**
 * scale_vcore_omap4430() - Scale for OMAP4430
 * @rev:	OMAP chip revision
 *
 * PMIC assumed to be used is TWL6030
 */
static void scale_vcore_omap4430(unsigned int rev)
{
	u8 vsel;

	/* vdd_core - VCORE 3 - OPP100 - ES2+: 1.127 */
	vsel = (rev == OMAP4430_ES1_0) ? 0x31 : 0x22;
	omap_vc_bypass_send_value(TWL6030_SRI2C_SLAVE_ADDR,
				  TWL6030_SRI2C_REG_ADDR_VCORE3, vsel);

	/* vdd_mpu - VCORE 1 - OPP100 - ES2+: 1.2027 */
	vsel = (rev == OMAP4430_ES1_0) ? 0x3B : 0x28;
	omap_vc_bypass_send_value(TWL6030_SRI2C_SLAVE_ADDR,
				  TWL6030_SRI2C_REG_ADDR_VCORE1, vsel);

	/* vdd_iva - VCORE 2 - OPP50 - ES2+: 0.950V */
	vsel = (rev == OMAP4430_ES1_0) ? 0x31 : 0x14;
	omap_vc_bypass_send_value(TWL6030_SRI2C_SLAVE_ADDR,
				  TWL6030_SRI2C_REG_ADDR_VCORE2, vsel);
}

/**
 * scale_vcore_omap4460() - Scale for OMAP4460
 * @rev:	OMAP chip revision
 *
 * PMIC assumed to be used is TWL6030 + TPS62361
 */
static void scale_vcore_omap4460(unsigned int rev)
{
	u32 volt;

	/* vdd_core - TWL6030 VCORE 1 - OPP100 - 1.127V */
	omap_vc_bypass_send_value(TWL6030_SRI2C_SLAVE_ADDR,
				  TWL6030_SRI2C_REG_ADDR_VCORE1, 0x22);

	/* vdd_mpu - TPS62361 - OPP100 - 1.210V (roundup from 1.2V) */
	volt = 1210;
	volt -= TPS62361_BASE_VOLT_MV;
	volt /= 10;
	do_scale_tps62361(TPS62361_REG_ADDR_SET1, volt);

	/* vdd_iva - TWL6030 VCORE 2 - OPP50  - 0.950V */
	omap_vc_bypass_send_value(TWL6030_SRI2C_SLAVE_ADDR,
				  TWL6030_SRI2C_REG_ADDR_VCORE2, 0x14);
}

/**
 * scale_vcore_omap4470() - Scale for OMAP4470
 * @rev:	OMAP chip revision
 *
 * PMIC assumed to be used is TWL6032
 */
static void scale_vcore_omap4470(unsigned int rev)
{
#ifdef CORE_233MHZ
        /* vdd_core - SMPS 2 - OPP100-OV - 1.25V */
        omap_vc_bypass_send_value(TWL6032_SRI2C_SLAVE_ADDR,
                                  TWL6032_SRI2C_REG_ADDR_SMPS2, 0x2C);
#else
	/* vdd_core - SMPS 2 - OPP100 - 1.1268V */
	omap_vc_bypass_send_value(TWL6032_SRI2C_SLAVE_ADDR,
				  TWL6032_SRI2C_REG_ADDR_SMPS2, 0x22);
#endif

	/* vdd_mpu - SMPS 1 - OPP100 - 1.2027V */
	omap_vc_bypass_send_value(TWL6032_SRI2C_SLAVE_ADDR,
				  TWL6032_SRI2C_REG_ADDR_SMPS1, 0x28);

	/* vdd_iva - SMPS 5 - OPP50 - 0.950V */
	omap_vc_bypass_send_value(TWL6032_SRI2C_SLAVE_ADDR,
				  TWL6032_SRI2C_REG_ADDR_SMPS5, 0x14);
}

static void scale_vcores(void)
{
	unsigned int rev = omap_revision();

	/*
	 * Dont use HSMODE, scll=0x60, sclh=0x26
	 * Note on HSMODE = 0:
	 * This allows us to allow a voltage domain to scale while we do i2c
	 * operation for the next domain - Please verify to ensure
	 * adequate delays are present in the case of slower ramp time for PMIC.
	 * This settings allow upto ~100Usec latency covered while i2c operation
	 * is in progress with the above configuration.
	 * Note #2: this latency will also depend on i2c_clk configuration as
	 * well.
	 */
	omap_vc_init(0x00, 0x00, 0x60, 0x26);

	if (rev >= OMAP4470_ES1_0 && rev <= OMAP4470_MAX_REVISION)
		scale_vcore_omap4470(rev);
	else if (rev >= OMAP4460_ES1_0 && rev <= OMAP4460_MAX_REVISION)
		scale_vcore_omap4460(rev);
	else			/* Default OMAP4430 */
		scale_vcore_omap4430(rev);
}

static void errata_i727_workaround(void)
{
	unsigned long reg;

	/* Errata i727
	 *
	 * For all OMAP4 revisions
	 * When a warm reset is applied on the system, the OMAP processor
	 * restarts with another OPP and so frequency is not the same. Due to
	 * this frequency change, the refresh rate programmed is not the accurate
	 * value and could result in an unexpected behavior on the memory side.
	 *
	 * The workaround is to force self-refresh immediately when coming back
	 * from the warm reset.
	 */

	/* set EMIF1 ddr self refresh mode and clear self refresh timer */
	reg = __raw_readl(EMIF1_BASE + EMIF_PWR_MGMT_CTRL);
	reg &= ~EMIF_PWR_MGMT_CTRL_LP_MODE_MASK;
	reg |= EMIF_PWR_MGMT_CTRL_LP_MODE_REFRESH;
	reg &= ~EMIF_PWR_MGMT_CTRL_SR_TIM_MASK;
	__raw_writel(reg, EMIF1_BASE + EMIF_PWR_MGMT_CTRL);

	/* set EMIF2 ddr self refresh mode and clear self refresh timer */
	reg = __raw_readl(EMIF2_BASE + EMIF_PWR_MGMT_CTRL);
	reg &= ~EMIF_PWR_MGMT_CTRL_LP_MODE_MASK;
	reg |= EMIF_PWR_MGMT_CTRL_LP_MODE_REFRESH;
	reg &= ~EMIF_PWR_MGMT_CTRL_SR_TIM_MASK;
	__raw_writel(reg, EMIF2_BASE + EMIF_PWR_MGMT_CTRL);

	/* dummy read commands to make changes above take effect */
	__raw_readl(EMIF1_BASE + EMIF_PWR_MGMT_CTRL);
	__raw_readl(EMIF2_BASE + EMIF_PWR_MGMT_CTRL);

	/* i735 Errata which requires SR_TIM to be programmed with a
	 * value greater than 6 is not implemented here because
	 * ddr_init will run shortly after this function and
	 * reprogram the DDR power management registers including
	 * SR_TIM.
	 */

}

/**********************************************************
 * Routine: s_init
 * Description: Does early system init of muxing and clocks.
 * - Called path is with SRAM stack.
 **********************************************************/
void s_init(void)
{
	errata_i727_workaround();

	set_muxconf_regs();
	spin_delay(100);

	/* WKUP clocks */
	sr32(CM_WKUP_GPIO1_CLKCTRL, 0, 32, 0x1);
	wait_on_value(BIT17|BIT16, 0, CM_WKUP_GPIO1_CLKCTRL, LDELAY);

	/* Set VCORE1 = 1.3 V, VCORE2 = VCORE3 = 1.21V */
	scale_vcores();

	ddr_init();
	prcm_init();
}

/******************************************************
 * Routine: wait_for_command_complete
 * Description: Wait for posting to finish on watchdog
 ******************************************************/
void wait_for_command_complete(unsigned int wd_base)
{
	int pending = 1;
	do {
		pending = __raw_readl(wd_base + WWPS);
	} while (pending);
}

int nand_init(void)
{
	return 1;
}
