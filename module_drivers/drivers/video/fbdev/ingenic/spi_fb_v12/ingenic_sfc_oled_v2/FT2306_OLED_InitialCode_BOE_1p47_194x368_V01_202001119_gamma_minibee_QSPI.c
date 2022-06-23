

//#define Power_IC_RT4730
#define Power_IC_SGM38042
//#define Power_IC_SGM38046


int swire_power_on(int gpio_swire, int cycles);


static void write_8spiregister(char *fmt, ...)
//static void write_8spiregister1(char *fmt, va_list args)
{
#define	ARGSMAX (32)
	unsigned char argArray[ARGSMAX];
	int nArgValue = 0;
	int nArgCount = 0;

	va_list args;
	va_start(args, fmt);

	/* ------------- args start ------------- */
	//printk("===============>>%s()\n", __func__);

	/* ------------- args start ------------- */
	
	while(nArgCount<ARGSMAX) {
		nArgValue=va_arg(args,int);
		//printk("   arg%d: %x\n",nArgCount,nArgValue);
		if (nArgValue == -1)
			break;

		argArray[nArgCount] = nArgValue;
		++nArgCount;
	}

	/* ------------- args start ------------- */
	va_end(args);

	/* call sfc write command */
	if (nArgCount>0) {
		//ingenic_sfc_write_8spiregister(g_flash, nArgCount, argArray);
		//ingenic_sfc_write_8spiregister(g_flash, (*argArray<<8) , argArray+1, nArgCount-1);
		ingenic_sfc_write_8spiregister(g_flash, (*argArray<<8) |(*(argArray+1)), argArray+2, nArgCount-2);
		//mdelay(2000);
	}

	return;
}


/* static void write_8spiregister(char *fmt, ...) */
/* { */
/* 	va_list args; */

/* 	/\* ------------- args start ------------- *\/ */
/* 	va_start(args, fmt); */
/* 	printk("===============>>%s()\n", __func__); */
/* 	write_8spiregister1(fmt, args); */
/* 	/\* ------------- args start ------------- *\/ */
/* 	va_end(args); */

/* 	return; */
/* } */

/* clear max 32 args */
#define clear_va_args				\
	write_8spiregister(fmt,				\
			   -1,-1,-1,-1,-1,-1,-1,-1,	\
			   -1,-1,-1,-1,-1,-1,-1,-1,	\
			   -1,-1,-1,-1,-1,-1,-1,-1,	\
			   -1,-1,-1,-1,-1,-1,-1,-1)


#define delayms_pc mdelay

static int FT2306_OLED_InitialCode_BOE_1p47_194x368_V01_202001119_gamma_minibee_QSPI(void)
{
	char fmt[2];
	fmt[0] = 'f';
	fmt[1] = 0;
	printk("===============>>%s:%s:%d\n",__FILE__,__func__,__LINE__);

//printk("%s() %d ...\n", __func__, __LINE__); mdelay(3000) ;

//FPGA_INI();
//delayms_pc(10);
//ini_ssd2828();
//delayms_pc(1000);

//delayms_pc(100);
//clear_va_args;write_8spiregister(fmt,0x01,0x00,0x00);  // soft reset
//clear_va_args;write_8spiregister(fmt,0xff,0x91,0x40);        // change read io pin

//CMD2ENABLE
clear_va_args;write_8spiregister(fmt,0xFF,0x00,0x23,0x06,0x01);
delayms_pc(100);
clear_va_args;write_8spiregister(fmt,0xFF,0x80,0x23,0x06);
delayms_pc(100);
//=========OSCfrequency=============//
clear_va_args;write_8spiregister(fmt,0xC1,0x80,0x55); //OSC 8M 20200917
//=========PanelSize=============//
clear_va_args;write_8spiregister(fmt,0xB3,0xA1,0x00,0xC2,0x01,0x70,0x00,0xC2,0x01,0x70); //194x368
//=========RTN=============//
clear_va_args;write_8spiregister(fmt,0xC0,0x80,0x01,0xA9,0x00,0x0C,0x00,0x0C);
clear_va_args;write_8spiregister(fmt,0xC0,0x96,0x01,0xA9,0x00,0x0C,0x00,0x0C);
clear_va_args;write_8spiregister(fmt,0xC0,0x86,0x01,0xA9,0x00,0x0C,0x00,0x0C);
clear_va_args;write_8spiregister(fmt,0xC0,0x90,0x01,0xA9,0x00,0x0C,0x00,0x0C);
//=========LPF=============
clear_va_args;write_8spiregister(fmt,0xC0,0xE0,0x11);
clear_va_args;write_8spiregister(fmt,0xC0,0xE2,0x01,0x00,0x01,0x03); //AOD 15Hz
clear_va_args;write_8spiregister(fmt,0xC0,0xE9,0x01,0x01);
//=========Panelresol=============//
clear_va_args;write_8spiregister(fmt,0xB2,0x82,0x65); //x=194
//=========MUXswitch=============//
clear_va_args;write_8spiregister(fmt,0xC2,0xC0,0x32,0xFF,0x54,0xFF,0x10,0xFF,0x23,0xFF,0x45,0xFF,0x01,0xFF);
//modify 20200812
clear_va_args;write_8spiregister(fmt,0xC2,0xD0,0x32,0xFF,0x54,0xFF,0x10,0xFF,0x23,0xFF,0x45,0xFF,0x01,0xFF);
//=========TCONMSTATE=============//
clear_va_args;write_8spiregister(fmt,0xC0,0xA0,0x2C,0x22);
clear_va_args;write_8spiregister(fmt,0xC0,0xB0,0x2C,0x22);
clear_va_args;write_8spiregister(fmt,0xC1,0xD0,0x04,0x1C,0x03,0x17);
clear_va_args;write_8spiregister(fmt,0xC1,0xC9,0x00);
clear_va_args;write_8spiregister(fmt,0xC2,0xE1,0x00); //MUX precharge off
//=========VST=============//
clear_va_args;write_8spiregister(fmt,0xC2,0x80,0x82,0x00);
clear_va_args;write_8spiregister(fmt,0xC2,0x82,0xFF,0x00,0x2A,0x00,0x84,0x14);
clear_va_args;write_8spiregister(fmt,0xC2,0x88,0x32,0x00,0x32,0x00,0x00);
//=========CKV=============//
clear_va_args;write_8spiregister(fmt,0xC2,0x90,0x88,0x0F,0x01);
clear_va_args;write_8spiregister(fmt,0xC2,0x93,0x11);
clear_va_args;write_8spiregister(fmt,0xC2,0x94,0x04);
clear_va_args;write_8spiregister(fmt,0xC2,0x95,0x06,0x00,0x11);
clear_va_args;write_8spiregister(fmt,0xC2,0x98,0x87,0x0F,0x01);
clear_va_args;write_8spiregister(fmt,0xC2,0x9B,0x11);
clear_va_args;write_8spiregister(fmt,0xC2,0x9C,0x00);
clear_va_args;write_8spiregister(fmt,0xC2,0x9D,0x2A,0xCC,0x00);
//=========STE=============//
clear_va_args;write_8spiregister(fmt,0xC2,0x20,0x10,0x00);
clear_va_args;write_8spiregister(fmt,0xC2,0x22,0xD0);
clear_va_args;write_8spiregister(fmt,0xC2,0x23,0x84,0x04,0x04);
clear_va_args;write_8spiregister(fmt,0xC2,0x26,0xC3,0xEE,0x3C,0xD3,0x59,0x00);
//=========ENMODE=============//
clear_va_args;write_8spiregister(fmt,0xC3,0x80,0xF2,0x00,0xF2,0xF2,0x00,0x00,0x00,0xC2,0xF1,0x00);
clear_va_args;write_8spiregister(fmt,0xC3,0x90,0xEE,0xF0,0xFE,0xFE,0x00,0x00,0x00,0xFE,0xFF,0x00);
clear_va_args;write_8spiregister(fmt,0xC3,0xA0,0xEF,0x00,0xAA,0xAA,0x00,0x00,0x00,0xEE,0x55,0x00); //20200904
//=========GIP mapping=============//
clear_va_args;write_8spiregister(fmt,0xC3,0xB0,0x78,0x94,0x54,0x50,0x00,0x05,0x00,0x00,0x55,0x00,0x00,0x54);
//modify 20200812
clear_va_args;write_8spiregister(fmt,0xC3,0xC0,0x65,0x48,0x54,0x92,0x00,0x15,0x00,0x00,0x55,0x00,0x00,0x54);

//=========Normal mode Power setting=============//
clear_va_args;write_8spiregister(fmt,0xEA,0x10,0x54); //AVDD=6V ,20200817
clear_va_args;write_8spiregister(fmt,0xC5,0x82,0x1C,0x1C); //VGH=6V, VGL=-6V ,20200817
clear_va_args;write_8spiregister(fmt,0xD8,0x05,0x1C,0x1C); //VGHO=6V, VGLO=-6V
clear_va_args;write_8spiregister(fmt,0xD8,0x07,0x08,0x17); //NORM VREFN=-2.5V, 20201119
//BOE Gamma tool Normal Vmin=VGSP ,20200907
//clear_va_args;write_8spiregister(fmt,0xD8,0x00,0x00,0xA7,0x70,0x00,0x00);
//Focal Gamma tool Normal Vmin=Adjustable value ,20200907
clear_va_args;write_8spiregister(fmt,0xD8,0x00,0x00,0xA7,0x70,0x00,0x67); //VGMP=4.3V, VGSP=1.6V, 20200817
#ifdef Power_IC_RT4730
//=========Power IC RT4730 Setting=============//
clear_va_args;write_8spiregister(fmt,0xF3,0x81,0x14); //HBM Power IC ELVDD=3.8, 2020729
clear_va_args;write_8spiregister(fmt,0xEA,0x13,0x3D); //HBM Power IC ELVSS=-3.7V, 2020729
clear_va_args;write_8spiregister(fmt,0xC5,0xA1,0x0F); //Normal ELVDD 3.3V,
clear_va_args;write_8spiregister(fmt,0xEA,0x14,0x41); //Normal ELVSS -3.3V 20200819
#elif defined(Power_IC_SGM38042)
//=========Power IC SGM38042 Setting=============//

clear_va_args;write_8spiregister(fmt,0xF3,0x81,0x45); //HBM Power IC ELVDD=3.8, 2020819
clear_va_args;write_8spiregister(fmt,0xEA,0x13,0x1C); //HBM Power IC ELVSS=-3.7V, 20200819
clear_va_args;write_8spiregister(fmt,0xC5,0xA1,0x4A); //Normal ELVDD 3.3V,
clear_va_args;write_8spiregister(fmt,0xEA,0x14,0x20); //Normal ELVSS -3.3V 20200819

#elif defined(Power_IC_SGM38046)
//=========Power IC SGM38046 Setting 20210315=============//
clear_va_args;write_8spiregister(fmt,0xF3,0x81,0x14); //HBM Power IC ELVDD=3.8, 20210315
clear_va_args;write_8spiregister(fmt,0xEA,0x13,0x3D); //HBM Power IC ELVSS=-3.7V, 20210315
clear_va_args;write_8spiregister(fmt,0xC5,0xA1,0x00); //Normal ELVDD 3.3V, 20210315
clear_va_args;write_8spiregister(fmt,0xEA,0x14,0x00); //Normal ELVSS -3.3V 20210315
#else
#error "not defined Power_IC"
#endif
//=========Source Precharge=============//
clear_va_args;write_8spiregister(fmt,0xC4,0x83,0x43,0xFF); //SD PCH BL=VGSP
clear_va_args;write_8spiregister(fmt,0xC4,0x85,0x22); //SD_AOD_ENTER/EXIT VGMP
clear_va_args;write_8spiregister(fmt,0xF5,0xAA,0x16);
clear_va_args;write_8spiregister(fmt,0xF5,0xA2,0x00,0x00,0x00,0x00); //SD EN PCH&DISC, 2020803
clear_va_args;write_8spiregister(fmt,0xC4,0x80,0x32);


//=========other setting=============//
clear_va_args;write_8spiregister(fmt,0xC5,0xE0,0x3E); // STB VDD, 2020729
#ifdef Power_IC_RT4730
clear_va_args;write_8spiregister(fmt,0xc0,0xC3,0x0C); //ESTV PWR Blank Num 20200904
#elif defined(Power_IC_SGM38042)
clear_va_args;write_8spiregister(fmt,0xc0,0xC3,0x06); // Power IC SGM38046 20210315
#elif defined(Power_IC_SGM38046)
clear_va_args;write_8spiregister(fmt,0xc0,0xC3,0x06); // Power IC SGM38046 20210315
#endif
clear_va_args;write_8spiregister(fmt,0xC0,0xC5,0x01);
clear_va_args;write_8spiregister(fmt,0xC0,0xD0,0xC4); // mirror X modify20200812
clear_va_args;write_8spiregister(fmt,0xC4,0x82,0xAC); //SD bias short
clear_va_args;write_8spiregister(fmt,0xC5,0xC6,0xAA); // STB VGHO
clear_va_args;write_8spiregister(fmt,0xC5,0xB0,0x8F); //AVDD clamp en
clear_va_args;write_8spiregister(fmt,0xC5,0xB2,0x00); //NORM,AOD VGH=AVDD
clear_va_args;write_8spiregister(fmt,0xC5,0xB3,0x03); //VGL/VGH, clamp en, HBM VGH=AVDD
clear_va_args;write_8spiregister(fmt,0xC5,0xB4,0x01); //VCL+VCL ,20200922
clear_va_args;write_8spiregister(fmt,0xF5,0xD8,0x00,0x00,0x0D,0x11); //VREFP off, VREFN on
clear_va_args;write_8spiregister(fmt,0xF6,0x85,0x04); //esd detec TE1	2020729
clear_va_args;write_8spiregister(fmt,0xF5,0x90,0x00,0x00,0x00,0x00); //close VGHO/VGLO EN
clear_va_args;write_8spiregister(fmt,0xCB,0x00,0x01); //SWIRE Follow 29 @modify20200817
clear_va_args;write_8spiregister(fmt,0xf3,0x82,0x02); //DCX/SDO output data

//=========AOD/HBM setting=============//
clear_va_args;write_8spiregister(fmt,0xC5,0x84,0x12,0x1C); //AOD Internal ELVDD=3.3V, ELVSS=-3.3V, 2020729
clear_va_args;write_8spiregister(fmt,0xC5,0x86,0x1C,0x1C,0x1C,0x1C); //AOD&HBM VGHO=6V, VGLO=-6V
clear_va_args;write_8spiregister(fmt,0xC5,0xB1,0x91);//VCL=-2VCI , 2020917
clear_va_args;write_8spiregister(fmt,0xC5,0x8A,0x08,0x08,0x17,0x21); //AOD_VREFN=-2.5V,HBM_VREFN=-3.5V,2020729

//BOE Gamma tool HBM Vmin=VGSP ,20200907
//clear_va_args;write_8spiregister(fmt,0xC5,0x90,0x00,0xA7,0xCF,0x70,0x50,0x00,0x00,0x00,0x00);

//Focal Gamma tool HBM Vmin=Adjustable value
clear_va_args;write_8spiregister(fmt,0xC5,0x90,0x00,0xA7,0xCF,0x70,0x50,0x00,0x00,0x00,0x87); //AOD&HBM vmax/vmin 20200817
clear_va_args;write_8spiregister(fmt,0xC5,0xD2,0x04); //VGSP/VGMP en in skip frame, 2020729
clear_va_args;write_8spiregister(fmt,0xC5,0xC7,0x53,0xA5,0x90,0xC1,0xC0,0x0C,0x00); // AOD/HBM switch mode settings 1, 20200917
clear_va_args;write_8spiregister(fmt,0xC5,0xD5,0x00,0x00,0x01,0x34,0x3C,0x00,0x1C,0x0C,0x3A,0x48,0x41); // AOD/HBM switch mode settings 2, 2020729
clear_va_args;write_8spiregister(fmt,0xC5,0xE8,0x00,0x82,0x82,0x80,0x80,0x08,0x88,0x88); // AOD/HBM switch mode settings 3, 2020729

//========= power saving ============//
clear_va_args;write_8spiregister(fmt,0xC5,0xE1,0x09,0x09,0x09); //GAP/AP //20200821
clear_va_args;write_8spiregister(fmt,0xC5,0xE4,0x62,0x62,0x63); //SAP, 2020729
clear_va_args;write_8spiregister(fmt,0xC2,0x70,0x06,0x00,0x60,0x02,0x11,0x20); //CKV/CKH precharge,20200819
clear_va_args;write_8spiregister(fmt,0xC4,0x83,0x33);//SD PCH off
clear_va_args;write_8spiregister(fmt,0xC4,0x95,0x90); //GM OP num, 2020729
clear_va_args;write_8spiregister(fmt,0xF3,0x80,0x31); //20200904
clear_va_args;write_8spiregister(fmt,0xF3,0x95,0xF0); //20200819
//Normal Gamma Setting @modify 20200811
clear_va_args;write_8spiregister(fmt,0xE2,0x00,0x01,0x00,0xBB,0x22,0x49,0xD4,0x33,0x2E,0x71,0x33,0x96,0xB0,0x33,
0xC9,0xE1);
clear_va_args;write_8spiregister(fmt,0xE2,0x0F,0x34,0xF8,0x0D,0x44,0x20,0x32,0x44,0x58,0x79,0x44,0x98,0xB8,0x45,
0xF2,0x2A,0x55);
clear_va_args;write_8spiregister(fmt,0xE2,0x1F,0x5D,0x97,0x56,0xCB,0x00,0x66,0x33,0x66,0x66,0x97,0xCC,0x66,0xE9,
0xF6,0x70,0x02);
clear_va_args;write_8spiregister(fmt,0xE5,0x00,0x01,0x00,0xD5,0x22,0x5C,0xDE,0x33,0x1A,0x64,0x33,0x8D,0xA2,0x33,
0xBA,0xCD);
clear_va_args;write_8spiregister(fmt,0xE5,0x0F,0x33,0xE4,0xF7,0x44,0x08,0x19,0x44,0x3C,0x5A,0x44,0x77,0x94,0x44,
0xC6,0xF8,0x55);
clear_va_args;write_8spiregister(fmt,0xE5,0x1F,0x24,0x58,0x55,0x86,0xB2,0x56,0xE0,0x0F,0x66,0x36,0x63,0x66,0x7D,
0x88,0x60,0x90);
clear_va_args;write_8spiregister(fmt,0xE8,0x00,0x01,0x00,0xCE,0x23,0x6D,0x0B,0x33,0x73,0xC0,0x34,0xE7,0x01,0x44,
0x1E,0x38);
clear_va_args;write_8spiregister(fmt,0xE8,0x0F,0x44,0x51,0x69,0x44,0x7F,0x94,0x44,0xBF,0xE6,0x55,0x0B,0x31,0x55,
0x75,0xB9,0x56);
clear_va_args;write_8spiregister(fmt,0xE8,0x1F,0xF9,0x3D,0x66,0x7B,0xBA,0x67,0xF7,0x39,0x77,0x79,0xBC,0x77,0xDF,0xEF,0x70,0xFF);
//AOD Gamma @modify 20200818
//BOE Gamma tool IDLE Vmin=VGSP
//clear_va_args;write_8spiregister(fmt,0xEA,0x15,0x00);
//Focal Gamma tool IDLE Vmin=Normal Vmin(D804h)
clear_va_args;write_8spiregister(fmt,0xEA,0x15,0x67); //IDLE Vmin , 20200907
clear_va_args;write_8spiregister(fmt,0xC6,0x00,0x5E); //IDLE 50nit
clear_va_args;write_8spiregister(fmt,0xC6,0x02,0x80); //IDLE Gamma Follow Normal

//HBM Gamma @modify 20200811
clear_va_args;write_8spiregister(fmt,0xE3,0x00,0x01,0x00,0x3B,0x12,0xBC,0x42,0x22,0xB0,0xF5,0x33,0x16,0x32,0x33,
0x4A,0x64);
clear_va_args;write_8spiregister(fmt,0xE3,0x0F,0x33,0x7A,0x8F,0x33,0xA1,0xB5,0x33,0xD7,0xF9,0x44,0x1B,0x3D,0x44,
0x7E,0xBA,0x45);
clear_va_args;write_8spiregister(fmt,0xE3,0x1F,0xF5,0x32,0x55,0x71,0xAD,0x56,0xEA,0x29,0x66,0x67,0xAB,0x66,0xCD,0xDE,0x60,0xF0);
clear_va_args;write_8spiregister(fmt,0xE6,0x00,0x01,0x00,0x4F,0x12,0xCB,0x49,0x22,0x97,0xE3,0x33,0x08,0x1E,0x33,
0x38,0x50);
clear_va_args;write_8spiregister(fmt,0xE6,0x0F,0x33,0x63,0x77,0x33,0x87,0x9A,0x33,0xB8,0xD8,0x34,0xF3,0x10,0x44,
0x49,0x82,0x44);
clear_va_args;write_8spiregister(fmt,0xE6,0x1F,0xB4,0xE5,0x55,0x19,0x50,0x55,0x88,0xBD,0x56,0xF2,0x2A,0x66,0x44,
0x53,0x60,0x61);
clear_va_args;write_8spiregister(fmt,0xE9,0x00,0x01,0x00,0x45,0x12,0xD4,0x6C,0x23,0xEE,0x3C,0x33,0x5F,0x7D,0x33,
0x97,0xB3);
clear_va_args;write_8spiregister(fmt,0xE9,0x0F,0x33,0xCA,0xE2,0x34,0xF7,0x0F,0x44,0x39,0x65,0x44,0x8D,0xB3,0x45,
0xFC,0x4A,0x55);
clear_va_args;write_8spiregister(fmt,0xE9,0x1F,0x95,0xDD,0x66,0x23,0x6C,0x67,0xB9,0x08,0x77,0x55,0xA9,0x77,0xD4,
0xEA,0x70,0xFF);

//========= SPR4 ============//
//SPR TEST (On c990=0x09 , Off c990=0x00)
clear_va_args;write_8spiregister(fmt,0xC9,0x90,0x00);
// reg_spr_mode ... etc(please check contents at this parameter)
//(spr On c9e0=0x28 , Off c9e0=0x00)
clear_va_args;write_8spiregister(fmt,0xC9,0xE0,0x00);

//L2N , 20200907
clear_va_args;write_8spiregister(fmt,0xC9,0xBA,0x00,0x01,0x03,0x07,0x00,0x0B);
clear_va_args;write_8spiregister(fmt,0xC9,0xC0,0x0F,0x13,0x17,0x02,0x1C,0x21,0x26,0x2E,0x2D,0x35,0x3F,0x47,0x57,0x2D,0x63,0x77);
clear_va_args;write_8spiregister(fmt,0xC9,0xD0,0x87,0x96,0x5D,0xA3,0xAF,0xBA,0xCE,0x51,0xE0,0xF0,0xFF,0x00);
clear_va_args;write_8spiregister(fmt,0xC9,0xE1,0x1F,0x05,0x1F,0x00,0x0D,0x00,0x00,0x00,0x00);
clear_va_args;write_8spiregister(fmt,0xC9,0xEA,0x00,0x00,0x00,0x00,0x0D,0x00);
clear_va_args;write_8spiregister(fmt,0xC9,0xF0,0x1F,0x05,0x1F);
clear_va_args;write_8spiregister(fmt,0xC9,0xF3,0x80,0x00,0x1F,0x00,0x0C,0x06,0x00,0x00,0x1F);
//========= Round Corner2 ============//
//Round Corner
clear_va_args;write_8spiregister(fmt,0xCA,0xC0,0x50,0x00,0x00,0x00,0x00,0x00,0x11,0x11,0x02,0x10,0x07,0x15);
clear_va_args;write_8spiregister(fmt,0x1F,0x00,0x32,0x22,0x11,0x11,0x11,0x11,0x01,0x10,0x10,0x01,0x00,0x00,0x00,0x00);
clear_va_args;write_8spiregister(fmt,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
clear_va_args;write_8spiregister(fmt,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
clear_va_args;write_8spiregister(fmt,0x1F,0x00,0x35,0x8A,0xBD,0xEF,0xFF,0xFF,0xFF,0xFF,0x26,0x9C,0xFF,0xFF,0xFF,0xFF);
clear_va_args;write_8spiregister(fmt,0x1F,0x00,0x38,0xCF,0xFF,0xFF,0x38,0xCF,0xFF,0x16,0xBF,0x39,0xEF,0x5B,0x6C,0x6D);
clear_va_args;write_8spiregister(fmt,0x1F,0x00,0x6D,0x5D,0x4C,0x2A,0x7F,0x4C,0x09,0x5D,0x09,0x4D,0x7F,0x1B,0x4D,0x79);
clear_va_args;write_8spiregister(fmt,0x1F,0x00,0xBC,0xDE,0xFF);

//========= ID ============//
clear_va_args;write_8spiregister(fmt,0xD1,0x00,0x22);//DBh(ID2)=0x22

//========= External DCDC Support AOD Power ============//
//Power IC RT4730 Setting
//clear_va_args;write_8spiregister(fmt,0xCB,0x07,0xA7);
//clear_va_args;write_8spiregister(fmt,0xCB,0x20,0x44);
//clear_va_args;write_8spiregister(fmt,0xC5,0xA2,0x0F);
//clear_va_args;write_8spiregister(fmt,0xC5,0xA4,0x41);
////Power IC SGM38042 Setting
//clear_va_args;write_8spiregister(fmt,0xCB,0x07,0xA7);
//clear_va_args;write_8spiregister(fmt,0xCB,0x20,0x44);
//clear_va_args;write_8spiregister(fmt,0xC5,0xA2,0x4A);
//clear_va_args;write_8spiregister(fmt,0xC5,0xA4,0x20);




//========= TE ============//
clear_va_args;write_8spiregister(fmt,0x35,0x00,0x00); //TE(Vsync) ON


//printk("swire_power_on() 1111\n"); //mdelay(2000);
//swire_power_on(0, 1); // SGM38042 swire: 65, OVSS-3.3V
//printk("swire_power_on() 222\n"); //mdelay(2000);


//printk("%s() %d ...\n", __func__, __LINE__); mdelay(3000) ;

//========= SLPOUT and DISPON ============//
clear_va_args;write_8spiregister(fmt,0x11,0x00,0x00);

delayms_pc(120);

//clear_va_args;write_8spiregister(fmt,0x29,0x00,0x00);

//printk("%s() %d ...\n", __func__, __LINE__); mdelay(3000) ;

#if 0
/* ========= Set Display Area  ========== */
printk("%s() Set Display Area ...\n", __func__);
clear_va_args;write_8spiregister(fmt,0x2A,0x00,0x00,0x00,0x00,0xc2); /* CASET (2AH): Column Address Set */
clear_va_args;write_8spiregister(fmt,0x2B,0x00,0x00,0x00,0x01,0x70); /* PASET (2BH): Page Address Set */
clear_va_args;write_8spiregister(fmt,0x30,0x00,0x00,0x00,0x01,0x70); /* PTLAR (30H): Partial Area */
clear_va_args;write_8spiregister(fmt,0x12,0x00,0x00); /* Partial Display Mode On */

#endif


//spi_writepattern(0x00,0xFF,0x00);
printk("%s() end...\n", __func__);
//while (1) ;

//clear_va_args;write_8spiregister(fmt,0x2c,0x00,0xff,0x00,0x00); while (1) ;

while (0) {
	clear_va_args;write_8spiregister(fmt,0x11,0x00,0x00);
	printk("%s() 0x11 ...\n", __func__);delayms_pc(2000); 
	clear_va_args;write_8spiregister(fmt,0x29,0x00);
	printk("%s() 0x29 ...\n", __func__);mdelay(2000);
	clear_va_args;write_8spiregister(fmt,0x28,0x00);
	printk("%s() 0x28 ...\n", __func__); mdelay(2000);
	clear_va_args;write_8spiregister(fmt,0x10,0x00,0x00);
	printk("%s() 0x11 ...\n", __func__);delayms_pc(2000); 
}

	/* clear_va_args;write_8spiregister(fmt,0x01,0x00,0x00); */
	/* mdelay(2000);printk("%s() 0x01 reset ...\n", __func__); */
while (0) {
	clear_va_args;write_8spiregister(fmt,0x01,0x00,0x00);
	mdelay(2000);printk("%s() 0x29 ...\n", __func__);
	clear_va_args;write_8spiregister(fmt,0x11,0x00,0x00);
	mdelay(2000);printk("%s() 0x29 ...\n", __func__);
	clear_va_args;write_8spiregister(fmt,0x29,0x00,0x00);
	mdelay(2000);printk("%s() 0x29 ...\n", __func__);
}



return 0;
}


static int FT2306_OLED_ON(void)
{
	char fmt[2];
	fmt[0] = 'f';
	fmt[1] = 0;


clear_va_args;write_8spiregister(fmt,0x29,0x00,0x00);

	return 0;
}
/* read 0x0a */

static int FT2306_OLED_BRI(int value)
{
	char fmt[2];
	fmt[0] = 'f';
	fmt[1] = 0;


	clear_va_args;write_8spiregister(fmt,0x51,0x00,value);
}


#undef delayms_pc
