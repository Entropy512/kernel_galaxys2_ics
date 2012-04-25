/*
 * midas-thermistor.c - thermistor of MIDAS Project
 *
 * Copyright (C) 2011 Samsung Electrnoics
 * SangYoung Son <hello.son@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mach/midas-thermistor.h>
#ifdef CONFIG_SEC_THERMISTOR
#include <mach/sec_thermistor.h>
#endif

#ifdef CONFIG_S3C_ADC
#if defined(CONFIG_MACH_M0)
static struct adc_table_data ap_adc_temper_table_battery[] = {
	{  204,	 800 },
	{  210,	 790 },
	{  216,	 780 },
	{  223,	 770 },
	{  230,	 760 },
	{  237,	 750 },
	{  244,	 740 },
	{  252,	 730 },
	{  260,	 720 },
	{  268,	 710 },
	{  276,	 700 },
	{  285,	 690 },
	{  294,	 680 },
	{  303,	 670 },
	{  312,	 660 },
	{  322,	 650 },
	{  332,	 640 },
	{  342,	 630 },
	{  353,	 620 },
	{  364,	 610 },
	{  375,	 600 },
	{  387,	 590 },
	{  399,	 580 },
	{  411,	 570 },
	{  423,	 560 },
	{  436,	 550 },
	{  450,	 540 },
	{  463,	 530 },
	{  477,	 520 },
	{  492,	 510 },
	{  507,	 500 },
	{  522,	 490 },
	{  537,	 480 },
	{  553,	 470 },
	{  569,	 460 },
	{  586,	 450 },
	{  603,	 440 },
	{  621,	 430 },
	{  638,	 420 },
	{  657,	 410 },
	{  675,	 400 },
	{  694,	 390 },
	{  713,	 380 },
	{  733,	 370 },
	{  753,	 360 },
	{  773,	 350 },
	{  794,	 340 },
	{  815,	 330 },
	{  836,	 320 },
	{  858,	 310 },
	{  880,	 300 },
	{  902,	 290 },
	{  924,	 280 },
	{  947,	 270 },
	{  969,	 260 },
	{  992,	 250 },
	{ 1015,	 240 },
	{ 1039,	 230 },
	{ 1062,	 220 },
	{ 1086,	 210 },
	{ 1109,	 200 },
	{ 1133,	 190 },
	{ 1156,	 180 },
	{ 1180,	 170 },
	{ 1204,	 160 },
	{ 1227,	 150 },
	{ 1250,	 140 },
	{ 1274,	 130 },
	{ 1297,	 120 },
	{ 1320,	 110 },
	{ 1343,	 100 },
	{ 1366,	  90 },
	{ 1388,	  80 },
	{ 1410,	  70 },
	{ 1432,	  60 },
	{ 1454,	  50 },
	{ 1475,	  40 },
	{ 1496,	  30 },
	{ 1516,	  20 },
	{ 1536,	  10 },
	{ 1556,	   0 },
	{ 1576,	 -10 },
	{ 1595,	 -20 },
	{ 1613,	 -30 },
	{ 1631,	 -40 },
	{ 1649,	 -50 },
	{ 1666,	 -60 },
	{ 1683,	 -70 },
	{ 1699,  -80 },
	{ 1714,  -90 },
	{ 1730, -100 },
	{ 1744, -110 },
	{ 1759, -120 },
	{ 1773, -130 },
	{ 1786, -140 },
	{ 1799, -150 },
	{ 1811, -160 },
	{ 1823, -170 },
	{ 1835, -180 },
	{ 1846, -190 },
	{ 1856, -200 },
};
#elif defined(CONFIG_MACH_C1)
static struct adc_table_data ap_adc_temper_table_battery[] = {
	{ 305,  650 },
	{ 566,  430 },
	{ 1494,   0 },
	{ 1571, -50 },
};
#elif defined(CONFIG_MACH_S2PLUS)
static struct adc_table_data ap_adc_temper_table_battery[] = {
	{ 305,  650 },
	{ 566,  430 },
	{ 1494,   0 },
	{ 1571, -50 },
};
#else	/* sample */
static struct adc_table_data ap_adc_temper_table_battery[] = {
	{ 305,  650 },
	{ 566,  430 },
	{ 1494,   0 },
	{ 1571, -50 },
};
#endif

int convert_adc(int adc_data, int channel)
{
	int adc_value;
	int low, mid, high;
	struct adc_table_data *temper_table = NULL;
	pr_debug("%s\n", __func__);

	low = mid = high = 0;
	switch (channel) {
	case 1:
		temper_table = ap_adc_temper_table_battery;
		high = ARRAY_SIZE(ap_adc_temper_table_battery) - 1;
		break;
	case 2:
		temper_table = ap_adc_temper_table_battery;
		high = ARRAY_SIZE(ap_adc_temper_table_battery) - 1;
		break;
	default:
		pr_info("%s: not exist temper table for ch(%d)\n", __func__,
							channel);
		return -EINVAL;
		break;
	}

	/* Out of table range */
	if (adc_data <= temper_table[low].adc) {
		adc_value = temper_table[low].value;
		return adc_value;
	} else if (adc_data >= temper_table[high].adc) {
		adc_value = temper_table[high].value;
		return adc_value;
	}

	while (low <= high) {
		mid = (low + high) / 2;
		if (temper_table[mid].adc > adc_data)
			high = mid - 1;
		else if (temper_table[mid].adc < adc_data)
			low = mid + 1;
		else
			break;
	}
	adc_value = temper_table[mid].value;

	/* high resolution */
	if (adc_data < temper_table[mid].adc)
		adc_value = temper_table[mid].value +
			((temper_table[mid-1].value - temper_table[mid].value) *
			(temper_table[mid].adc - adc_data) /
			(temper_table[mid].adc - temper_table[mid-1].adc));
	else
		adc_value = temper_table[mid].value -
			((temper_table[mid].value - temper_table[mid+1].value) *
			(adc_data - temper_table[mid].adc) /
			(temper_table[mid+1].adc - temper_table[mid].adc));

	pr_debug("%s: adc data(%d), adc value(%d)\n", __func__,
					adc_data, adc_value);
	return adc_value;

}
#endif

#ifdef CONFIG_SEC_THERMISTOR
static struct sec_therm_adc_table temper_table_ap[] = {
	{196,	700},
	{211,	690},
	{242,	685},
	{249,	680},
	{262,	670},
	{275,	660},
	{288,	650},
	{301,	640},
	{314,	630},
	{328,	620},
	{341,	610},
	{354,	600},
	{366,	590},
	{377,	580},
	{389,	570},
	{404,	560},
	{419,	550},
	{434,	540},
	{452,	530},
	{469,	520},
	{487,	510},
	{498,	500},
	{509,	490},
	{520,	480},
	{529,	460},
	{538,	470},
	{547,	450},
	{556,	440},
	{564,	430},
	{573,	420},
	{581,	410},
	{590,	400},
	{615,	390},
	{640,	380},
	{665,	370},
	{690,	360},
	{715,	350},
	{736,	340},
	{758,	330},
	{779,	320},
	{801,	310},
	{822,	300},
};

/* when the next level is same as prev, returns -1 */
static int get_midas_siop_level(int temp)
{
	static int prev_temp = 400;
	static int prev_level = 0;
	int level = -1;

	if (temp > prev_temp) {
		if (temp >= 550)
			level = 4;
		else if (temp >= 540)
			level = 3;
		else if (temp >= 490)
			level = 2;
		else if (temp >= 440)
			level = 1;
		else
			level = 0;
	} else {
		if (temp < 410)
			level = 0;
		else if (temp < 440)
			level = 1;
		else if (temp < 490)
			level = 2;
		else if (temp < 540)
			level = 3;
		else
			level = 4;

		if (level > prev_level)
			level = prev_level;
	}

	prev_temp = temp;
	if (prev_level == level)
		return -1;

	prev_level = level;

	return level;
}

static struct sec_therm_platform_data sec_therm_pdata = {
	.adc_channel	= 1,
	.adc_arr_size	= ARRAY_SIZE(temper_table_ap),
	.adc_table	= temper_table_ap,
	.polling_interval = 30 * 1000, /* msecs */
	.get_siop_level = get_midas_siop_level,
};

struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};
#endif

#ifdef CONFIG_STMPE811_ADC
/* temperature table for ADC ch7 */
static struct adc_table_data temper_table_battery[] = {
	{	1856, -20	},
	{	1846, -19	},
	{	1835, -18	},
	{	1823, -17	},
	{	1811, -16	},
	{	1799, -15	},
	{	1786, -14	},
	{	1773, -13	},
	{	1759, -12	},
	{	1744, -11	},
	{	1730, -10	},
	{	1714, -9	},
	{	1699, -8	},
	{	1683, -7	},
	{	1666, -6	},
	{	1649, -5	},
	{	1631, -4	},
	{	1613, -3	},
	{	1595, -2	},
	{	1576, -1	},
	{	1556, 0		},
	{	1536, 1		},
	{	1516, 2		},
	{	1496, 3		},
	{	1475, 4		},
	{	1454, 5		},
	{	1432, 6		},
	{	1410, 7		},
	{	1388, 8		},
	{	1366, 9		},
	{	1343, 10	},
	{	1320, 11	},
	{	1297, 12	},
	{	1274, 13	},
	{	1250, 14	},
	{	1227, 15	},
	{	1204, 16	},
	{	1180, 17	},
	{	1156, 18	},
	{	1133, 19	},
	{	1109, 20	},
	{	1086, 21	},
	{	1062, 22	},
	{	1039, 23	},
	{	1015, 24	},
	{	992,  25	},
	{	969,  26	},
	{	947,  27	},
	{	924,  28	},
	{	902,  29	},
	{	880,  30	},
	{	858,  31	},
	{	836,  32	},
	{	815,  33	},
	{	794,  34	},
	{	773,  35	},
	{	753,  36	},
	{	733,  37	},
	{	713,  38	},
	{	694,  39	},
	{	675,  40	},
	{	657,  41	},
	{	638,  42	},
	{	621,  43	},
	{	603,  44	},
	{	586,  45	},
	{	569,  46	},
	{	553,  47	},
	{	537,  48	},
	{	522,  49	},
	{	507,  50	},
	{	492,  51	},
	{	477,  52	},
	{	463,  53	},
	{	450,  54	},
	{	436,  55	},
	{	423,  56	},
	{	411,  57	},
	{	399,  58	},
	{	387,  59	},
	{	375,  60	},
	{	364,  61	},
	{	353,  62	},
	{	342,  63	},
	{	332,  64	},
	{	322,  65	},
	{	312,  66	},
	{	303,  67	},
	{	294,  68	},
	{	285,  69	},
	{	276,  70	},
	{	268,  71	},
	{	260,  72	},
	{	252,  73	},
	{	244,  74	},
	{	237,  75	},
	{	230,  76	},
	{	223,  77	},
	{	216,  78	},
	{	210,  79	},
	{	204,  80	},
};

struct stmpe811_platform_data stmpe811_pdata = {
	.adc_table_ch4 = temper_table_battery,
	.table_size_ch4 = ARRAY_SIZE(temper_table_battery),
	.adc_table_ch7 = temper_table_battery,
	.table_size_ch7 = ARRAY_SIZE(temper_table_battery),

	.irq_gpio = GPIO_ADC_INT,
};
#endif

