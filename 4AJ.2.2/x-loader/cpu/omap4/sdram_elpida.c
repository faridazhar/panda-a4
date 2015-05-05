/*
 * (C) Copyright 2004-2009
 * Texas Instruments, <www.ti.com>
 * Aneesh V	<aneesh@ti.com>
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
 * All OMAP boards of a given revision todate use the same memory
 * configuration. So keeping this here instead of keeping in board
 * directory. If a new board has different memory part/configuration
 * in future the weakly linked alias for ddr_init() can be over-ridden
 */
#include <common.h>
#include <asm/io.h>

/* Timing regs for Elpida */
const struct ddr_regs ddr_regs_elpida2G_400_mhz = {
	.tim1		= 0x10eb065a,
	.tim2		= 0x20370dd2,
	.tim3		= 0x00b1c33f,
	.phy_ctrl_1	= 0x849FF408,
	.ref_ctrl	= 0x00000618,
	.config_init	= 0x80000eb1,
	.config_final	= 0x80001ab1,
	.zq_config	= 0x500b3215,
	.mr1		= 0x83,
	.mr2		= 0x4
};

/*
 * 400 MHz + 2 CS = 1 GB
 */
const struct ddr_regs ddr_regs_elpida2G_400_mhz_2cs = {
	/* tRRD changed from 10ns to 12.5ns because of the tFAW requirement*/
	.tim1		= 0x10eb0662,
	.tim2		= 0x20370dd2,
	.tim3		= 0x00b1c33f,
	.phy_ctrl_1	= 0x049FF408,
	.ref_ctrl	= 0x00000618,
	.config_init	= 0x80800eb9,
	.config_final	= 0x80801ab9,
	.zq_config	= 0xd00b3215,
	.mr1		= 0x83,
	.mr2		= 0x4
};

const struct ddr_regs ddr_regs_elpida4G_400_mhz_1cs = {
	.tim1		= 0x10eb0662,
	.tim2		= 0x20370dd2,
	.tim3		= 0x00b1c33f,
	.phy_ctrl_1	= 0x449FF408,
	.ref_ctrl	= 0x00000618,
	.config_init	= 0x80800eb2,
	.config_final	= 0x80801ab2,
	.zq_config	= 0x500b3215,
	.mr1		= 0x83,
	.mr2		= 0x4
};

/*
 * Set Read Latency value to 0xB and DLL delay to 0x37
 * according to OMAP4470 LPDDR interface configuration
 * update for 466 MHz
 */
const struct ddr_regs ddr_regs_elpida4G_466_mhz_1cs = {
	.tim1		= 0x130F376B,
	.tim2		= 0x3041105A,
	.tim3		= 0x00F543CF,
#ifdef CORE_233MHZ
	.phy_ctrl_1	= 0x449FF37B,
#else
	.phy_ctrl_1	= 0x449FF408,
#endif
	.ref_ctrl	= 0x0000071B,
	.config_init	= 0x80800eb2,
	.config_final	= 0x80801EB2,
	.zq_config	= 0x500b3215,
	.mr1		= 0x83,
	.mr2		= 0x5
};

const struct ddr_regs ddr_regs_elpida2G_380_mhz = {
	.tim1		= 0x10cb061a,
	.tim2		= 0x20350d52,
	.tim3		= 0x00b1431f,
	.phy_ctrl_1	= 0x049FF408,
	.ref_ctrl	= 0x000005ca,
	.config_init	= 0x80800eb1,
	.config_final	= 0x80801ab1,
	.zq_config	= 0x500b3215,
	.mr1		= 0x83,
	.mr2		= 0x4
};

const struct ddr_regs ddr_regs_elpida2G_200_mhz = {
	.tim1		= 0x08648309,
	.tim2		= 0x101b06ca,
	.tim3		= 0x0048a19f,
	.phy_ctrl_1	= 0x849FF405,
	.ref_ctrl	= 0x0000030c,
	.config_init	= 0x80000eb1,
	.config_final	= 0x80000eb1,
	.zq_config	= 0x500b3215,
	.mr1		= 0x23,
	.mr2		= 0x1
};

const struct ddr_regs ddr_regs_elpida2G_200_mhz_2cs = {
	.tim1		= 0x08648309,
	.tim2		= 0x101b06ca,
	.tim3		= 0x0048a19f,
	.phy_ctrl_1	= 0x049FF405,
	.ref_ctrl	= 0x0000030c,
	.config_init	= 0x80800eb9,
	.config_final	= 0x80800eb9,
	.zq_config	= 0xD00b3215,
	.mr1		= 0x23,
	.mr2		= 0x1
};

/* ddr_init() - initializes ddr */
void __ddr_init(void)
{
	u32 rev;
	const struct ddr_regs *ddr_regs = 0;
	rev = omap_revision();
	if (rev == OMAP4430_ES1_0)
		ddr_regs = &ddr_regs_elpida2G_380_mhz;
	else if (rev == OMAP4430_ES2_0)
		ddr_regs = &ddr_regs_elpida2G_200_mhz_2cs;
	else if (rev >= OMAP4430_ES2_1 && rev < OMAP4470_ES1_0)
		ddr_regs = &ddr_regs_elpida2G_400_mhz_2cs;
	else if (rev >= OMAP4470_ES1_0)
#ifdef CORE_233MHZ
		ddr_regs = &ddr_regs_elpida4G_466_mhz_1cs;
#else
		ddr_regs = &ddr_regs_elpida4G_400_mhz_1cs;
#endif
	/*
	 * DMM DMM_LISA_MAP_i registers fields description
	 * [31:24] SYS_ADDR
	 * [22:20] SYS_SIZE
	 * [19:18] SDRC_INTLDMM
	 * [17:16] SDRC_ADDRSPC
	 * [9:8] SDRC_MAP
	 * [7:0] SDRC_ADDR
	 */

	/* TI Errata i614 - DMM Hang Issue During Unmapped Accesses
	 * CRITICALITY: Medium
	 * REVISIONS IMPACTED: OMAP4430 all
	 * DESCRIPTION
	 * DMM replies with an OCP error response in case of unmapped access.
	 * DMM can generate unmapped access at the end of a mapped section.
	 * A hang occurs if an unmapped access is issued after a mapped access.
	 *
	 * WORKAROUND
	 * Define LISA section to have response error when unmapped addresses.
	 * Define LISA section in such a way that all transactions reach the
	 * EMIF, which will return an error response.
	 *
	 * For W/A configure DMM Section 0 size equal to 2048 MB and map it on
	 * SDRC_ADDRSPC=0, and move original SDRAM configuration
	 * to section 1 with higher priority.
	 *
	 * This W/A has been applied for all OMAP4 CPUs - It doesn't any harm
	 * on 4460, and even desirable than hitting an un-mapped area in DMM.
	 * This makes the code simpler and easier to understand and
	 * the settings will be more uniform.
	 **/
	/* TRAP for catching accesses to the umapped memory */
	__raw_writel(0x80720100, DMM_BASE + DMM_LISA_MAP_0);

	__raw_writel(0x00000000, DMM_BASE + DMM_LISA_MAP_2);
	/* TRAP for catching accesses to the memory actually used by TILER */
	__raw_writel(0xFF020100, DMM_BASE + DMM_LISA_MAP_1);

	if (rev == OMAP4430_ES1_0)
		/* original DMM configuration
		 * - 512 MB, 128 byte interleaved, EMIF1&2, SDRC_ADDRSPC=0 */
		__raw_writel(0x80540300, DMM_BASE + DMM_LISA_MAP_3);
	else if (rev < OMAP4460_ES1_0)
		/* original DMM configuration
		 * - 1024 MB, 128 byte interleaved, EMIF1&2, SDRC_ADDRSPC=0 */
		__raw_writel(0x80640300, DMM_BASE + DMM_LISA_MAP_3);
	else {
		/* OMAP4460 and higher: original DMM configuration
		 * - 1024 MB, 128 byte interleaved, EMIF1&2, SDRC_ADDRSPC=0 */
		__raw_writel(0x80640300, DMM_BASE + DMM_LISA_MAP_3);

		__raw_writel(0x80720100, MA_BASE + DMM_LISA_MAP_0);
		__raw_writel(0xFF020100, MA_BASE + DMM_LISA_MAP_1);
		__raw_writel(0x00000000, MA_BASE + DMM_LISA_MAP_2);
		__raw_writel(0x80640300, MA_BASE + DMM_LISA_MAP_3);
	}

	/* same memory part on both EMIFs */
	do_ddr_init(ddr_regs, ddr_regs);

	/* Pull Dn enabled for "Weak driver control" on LPDDR
	 * Interface.
	 */
	if (rev >= OMAP4460_ES1_0) {
		__raw_writel(0x9c9c9c9c, CONTROL_LPDDR2IO1_0);
		__raw_writel(0x9c9c9c9c, CONTROL_LPDDR2IO1_1);
		__raw_writel(0x9c989c00, CONTROL_LPDDR2IO1_2);
		__raw_writel(0xa0888c03, CONTROL_LPDDR2IO1_3);
		__raw_writel(0x9c9c9c9c, CONTROL_LPDDR2IO2_0);
		__raw_writel(0x9c9c9c9c, CONTROL_LPDDR2IO2_1);
		__raw_writel(0x9c989c00, CONTROL_LPDDR2IO2_2);
		__raw_writel(0xa0888c03, CONTROL_LPDDR2IO2_3);
	}

#ifdef CORE_233MHZ
	/*
	 * Slew Rate should be set to “FASTEST” and
	 * Impedance Control to “Drv12”:
	 * CONTROL_LPDDR2IOx_2[LPDDR2IO1_GR10_SR] = 0
	 * CONTROL_LPDDR2IOx_2[LPDDR2IO1_GR10_I] = 7
	 * where x=[1-2]
	 */
	if (rev >= OMAP4470_ES1_0) {
		__raw_writel(0x9c3c9c00, CONTROL_LPDDR2IO1_2);
		__raw_writel(0x9c3c9c00, CONTROL_LPDDR2IO2_2);
	}
#endif

}

void ddr_init(void)
	__attribute__((weak, alias("__ddr_init")));
