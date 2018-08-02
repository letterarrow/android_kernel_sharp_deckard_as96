/* include/sharp/shtps_dev_sy3x00.h
 *
 * Copyright (C) 2012 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SHTPS_DEV_SY3X00_H__
#define __SHTPS_DEV_SY3X00_H__

#define SHTPS_SY3X00_GPIO_RST	29
#define SHTPS_SY3X00_GPIO_IRQ	77
#define SHTPS_SY3X00_SPI_CLOCK	1100000

enum{
	SHTPS_VEILVIEW_PATTERN_RGB_CHIDORI_1H = 0,
	SHTPS_VEILVIEW_PATTERN_RGB_CHIDORI_2H,
	SHTPS_VEILVIEW_PATTERN_MONOCHROME_1H,
	SHTPS_VEILVIEW_PATTERN_MONOCHROME_2H,
};

#if defined( CONFIG_SHTPS_SY3000_TM2713_001 )
	#include <sharp/shtps_sy3000_tm2713-001.h>
	#undef	SHTPS_SY3X00_SPI_CLOCK
	#define SHTPS_SY3X00_SPI_CLOCK	400000
#endif	/* CONFIG_SHTPS_SY3000_TM2713_001 */

#if defined( CONFIG_SHTPS_SY3000_TM2726_001 )
	#include <sharp/shtps_sy3000_tm2726-001.h>
	#undef	SHTPS_SY3X00_SPI_CLOCK
	#define SHTPS_SY3X00_SPI_CLOCK	400000
#endif	/* CONFIG_SHTPS_SY3000_TM2726_001 */

#if defined( CONFIG_SHTPS_SY3000_TM2741_001 )
	#include <sharp/shtps_sy3000_tm2741-001.h>
	#undef	SHTPS_SY3X00_SPI_CLOCK
	#define SHTPS_SY3X00_SPI_CLOCK	400000
#endif	/* CONFIG_SHTPS_SY3000_TM2741_001 */

#if defined( CONFIG_SHTPS_SY3000_TM2742_001 )
	#include <sharp/shtps_sy3000_tm2742-001.h>
	#undef	SHTPS_SY3X00_SPI_CLOCK
	#define SHTPS_SY3X00_SPI_CLOCK	400000
#endif	/* CONFIG_SHTPS_SY3000_TM2742_001 */

#if defined( CONFIG_SHTPS_SY3000_TM2776_001 )
	#include <sharp/shtps_sy3000_tm2776-001.h>
	#undef	SHTPS_SY3X00_SPI_CLOCK
	#define SHTPS_SY3X00_SPI_CLOCK	400000
#endif	/* CONFIG_SHTPS_SY3000_TM2776_001 */

#endif /* __SHTPS_DEV_SY3X00_H__ */
