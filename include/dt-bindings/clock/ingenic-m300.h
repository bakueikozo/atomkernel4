#ifndef _DT_BINDINGS_CLOCK_M300_H
#define _DT_BINDINGS_CLOCK_M300_H

/* Fixed Clk */
#define CLK_ID_FIEXED	0
#define	CLK_EXT		(CLK_ID_FIEXED + 0)
#define	CLK_RTC_EXT	(CLK_ID_FIEXED + 1)
#define	CLK_EXT_DIV_512	(CLK_ID_FIEXED + 2)
#define CLK_NR_FIXED	(3)

/* PLL clk */
#define CLK_ID_PLL	(CLK_ID_FIEXED + CLK_NR_FIXED)
#define CLK_PLL_APLL	(CLK_ID_PLL + 0)
#define CLK_PLL_MPLL	(CLK_ID_PLL + 1)
#define CLK_PLL_EPLL	(CLK_ID_PLL + 2)
#define CLK_NR_PLL	(3)

/* MUX clk */
#define CLK_ID_MUX		(CLK_ID_PLL + CLK_NR_PLL)
#define CLK_MUX_SCLKA		(CLK_ID_MUX + 0)
#define CLK_MUX_CPU_L2C		(CLK_ID_MUX + 1)
#define CLK_MUX_AHB0		(CLK_ID_MUX + 2)
#define CLK_MUX_AHB2		(CLK_ID_MUX + 3)
#define CLK_MUX_DDR		(CLK_ID_MUX + 4)
#define CLK_MUX_MACPHY		(CLK_ID_MUX + 5)
#define CLK_MUX_MACTXPHY	(CLK_ID_MUX + 6)
#define CLK_MUX_MACTXPHY1	(CLK_ID_MUX + 7)
#define CLK_MUX_I2S0		(CLK_ID_MUX + 8)
#define CLK_MUX_I2S1		(CLK_ID_MUX + 9)
#define CLK_MUX_I2S2            (CLK_ID_MUX + 10)
#define CLK_MUX_I2S3            (CLK_ID_MUX + 11)
#define CLK_MUX_AUDIO_RAM       (CLK_ID_MUX + 12)
#define CLK_MUX_SPDIF           (CLK_ID_MUX + 13)
#define CLK_MUX_PCM             (CLK_ID_MUX + 14)
#define CLK_MUX_DMIC            (CLK_ID_MUX + 15)
#define CLK_MUX_LCD             (CLK_ID_MUX + 16)
#define CLK_MUX_MSC0            (CLK_ID_MUX + 17)
#define CLK_MUX_MSC1		(CLK_ID_MUX + 18)
#define CLK_MUX_MSC2            (CLK_ID_MUX + 19)
#define CLK_MUX_SFC             (CLK_ID_MUX + 20)
#define CLK_MUX_SSI             (CLK_ID_MUX + 21)
#define CLK_MUX_CIM             (CLK_ID_MUX + 22)
#define CLK_MUX_PWM             (CLK_ID_MUX + 23)
#define CLK_MUX_ISP             (CLK_ID_MUX + 24)
#define CLK_MUX_RSA             (CLK_ID_MUX + 25)
#define CLK_MUX_MACPTP		(CLK_ID_MUX + 26)
#define CLK_MUX_WDT		(CLK_ID_MUX + 27)


#define CLK_NR_MUX		(28)


#define CLK_ID_DIV		(CLK_ID_MUX + CLK_NR_MUX)
#define CLK_DIV_DDR		(CLK_ID_DIV + 0)
#define CLK_DIV_MACPHY		(CLK_ID_DIV + 1)
#define CLK_DIV_MACTXPHY	(CLK_ID_DIV + 2)
#define CLK_DIV_MACTXPHY1       (CLK_ID_DIV + 3)
#define CLK_DIV_I2S0		(CLK_ID_DIV + 4)
#define CLK_DIV_I2S1            (CLK_ID_DIV + 5)
#define CLK_DIV_I2S2            (CLK_ID_DIV + 6)
#define CLK_DIV_I2S3            (CLK_ID_DIV + 7)
#define CLK_DIV_LCD		(CLK_ID_DIV + 8)
#define CLK_DIV_MSC0            (CLK_ID_DIV + 9)
#define CLK_DIV_MSC1            (CLK_ID_DIV + 10)
#define CLK_DIV_MSC2            (CLK_ID_DIV + 11)
#define CLK_DIV_SFC             (CLK_ID_DIV + 12)
#define CLK_DIV_SSI             (CLK_ID_DIV + 13)
#define CLK_DIV_CIM             (CLK_ID_DIV + 14)
#define CLK_DIV_PWM             (CLK_ID_DIV + 15)
#define CLK_DIV_ISP		(CLK_ID_DIV + 16)
#define CLK_DIV_RSA             (CLK_ID_DIV + 17)
#define CLK_DIV_MACPTP		(CLK_ID_DIV + 18)

#define CLK_DIV_CPU		(CLK_ID_DIV + 19)
#define CLK_DIV_L2C             (CLK_ID_DIV + 20)
#define CLK_DIV_AHB0            (CLK_ID_DIV + 21)
#define CLK_DIV_AHB2            (CLK_ID_DIV + 22)
#define CLK_DIV_APB             (CLK_ID_DIV + 23)
#define CLK_DIV_CPU_L2C_X1      (CLK_ID_DIV + 24)
#define CLK_DIV_CPU_L2C_X2      (CLK_ID_DIV + 25)

#define CLK_NR_DIV		(26)


/* Gate Clocks */
#define CLK_ID_GATE		(CLK_ID_DIV + CLK_NR_DIV)
#define CLK_GATE_DDR		(CLK_ID_GATE + 0)
#define CLK_GATE_IPU		(CLK_ID_GATE + 1)
#define CLK_GATE_AHB0		(CLK_ID_GATE + 2)
#define CLK_GATE_APB0		(CLK_ID_GATE + 3)
#define CLK_GATE_RTC		(CLK_ID_GATE + 4)
#define CLK_GATE_SSI1		(CLK_ID_GATE + 5)
#define CLK_GATE_RSA		(CLK_ID_GATE + 6)
#define CLK_GATE_AES		(CLK_ID_GATE + 7)
#define CLK_GATE_LCD		(CLK_ID_GATE + 8)
#define CLK_GATE_CIM		(CLK_ID_GATE + 9)
#define CLK_GATE_PDMA		(CLK_ID_GATE + 10)
#define CLK_GATE_OST		(CLK_ID_GATE + 11)
#define CLK_GATE_SSI0		(CLK_ID_GATE + 12)
#define CLK_GATE_TCU		(CLK_ID_GATE + 13)
#define CLK_GATE_DTRNG		(CLK_ID_GATE + 14)
#define CLK_GATE_UART2		(CLK_ID_GATE + 15)
#define CLK_GATE_UART1		(CLK_ID_GATE + 16)
#define CLK_GATE_UART0		(CLK_ID_GATE + 17)
#define CLK_GATE_SADC		(CLK_ID_GATE + 18)
#define CLK_GATE_HELIX		(CLK_ID_GATE + 19)
#define CLK_GATE_AUDIO		(CLK_ID_GATE + 20)
#define CLK_GATE_SMB3		(CLK_ID_GATE + 21)
#define CLK_GATE_SMB2		(CLK_ID_GATE + 22)
#define CLK_GATE_SMB1		(CLK_ID_GATE + 23)
#define CLK_GATE_SMB0		(CLK_ID_GATE + 24)
#define CLK_GATE_SCC		(CLK_ID_GATE + 25)
#define CLK_GATE_MSC1		(CLK_ID_GATE + 26)
#define CLK_GATE_MSC0		(CLK_ID_GATE + 27)
#define CLK_GATE_OTG		(CLK_ID_GATE + 28)
#define CLK_GATE_SFC		(CLK_ID_GATE + 29)
#define CLK_GATE_EFUSE		(CLK_ID_GATE + 30)
#define CLK_GATE_NEMC		(CLK_ID_GATE + 31)
#define CLK_GATE_ARB		(CLK_ID_GATE + 32)
#define CLK_GATE_MIPI_DSI	(CLK_ID_GATE + 33)
#define CLK_GATE_MIPI_CSI	(CLK_ID_GATE + 34)
#define CLK_GATE_INTC		(CLK_ID_GATE + 35)
#define CLK_GATE_MSC2		(CLK_ID_GATE + 36)
#define CLK_GATE_GMAC1		(CLK_ID_GATE + 37)
#define CLK_GATE_GMAC0		(CLK_ID_GATE + 38)
#define CLK_GATE_UART9		(CLK_ID_GATE + 39)
#define CLK_GATE_UART8		(CLK_ID_GATE + 40)
#define CLK_GATE_UART7		(CLK_ID_GATE + 41)
#define CLK_GATE_UATR6		(CLK_ID_GATE + 42)
#define CLK_GATE_UART5		(CLK_ID_GATE + 43)
#define CLK_GATE_UART4		(CLK_ID_GATE + 44)
#define CLK_GATE_UART3		(CLK_ID_GATE + 45)
#define CLK_GATE_SPDIF		(CLK_ID_GATE + 46)
#define CLK_GATE_DMIC		(CLK_ID_GATE + 47)
#define CLK_GATE_PCM		(CLK_ID_GATE + 48)
#define CLK_GATE_I2S3		(CLK_ID_GATE + 49)
#define CLK_GATE_I2S2		(CLK_ID_GATE + 50)
#define CLK_GATE_I2S1		(CLK_ID_GATE + 51)
#define CLK_GATE_I2S0		(CLK_ID_GATE + 52)
#define CLK_GATE_ROT		(CLK_ID_GATE + 53)
#define CLK_GATE_HASH		(CLK_ID_GATE + 54)
#define CLK_GATE_PWM		(CLK_ID_GATE + 55)
#define CLK_GATE_FELIX		(CLK_ID_GATE + 56)
#define CLK_GATE_ISP1		(CLK_ID_GATE + 57)
#define CLK_GATE_ISP0		(CLK_ID_GATE + 58)
#define CLK_GATE_SMB5		(CLK_ID_GATE + 59)
#define CLK_GATE_SMB4		(CLK_ID_GATE + 60)
#define CLK_GATE_USBPHY		(CLK_ID_GATE + 61)
#define CLK_CE_I2S0		(CLK_ID_GATE + 62)
#define CLK_CE_I2S1		(CLK_ID_GATE + 63)
#define CLK_CE_I2S2		(CLK_ID_GATE + 64)
#define CLK_CE_I2S3		(CLK_ID_GATE + 65)
#define CLK_NR_GATE		(66)

#define POWER_ID		(CLK_ID_GATE + CLK_NR_GATE)
#define POWER_ISP0		(POWER_ID + 0)
#define POWER_ISP1		(POWER_ID + 1)
#define POWER_FELIX		(POWER_ID + 2)
#define POWER_HELIX		(POWER_ID + 3)

#define PD_MEM_NEMC    		(POWER_ID + 4)
#define PD_MEM_USB     		(POWER_ID + 5)
#define PD_MEM_SFC     		(POWER_ID + 6)
#define PD_MEM_PDMA_SEC		(POWER_ID + 7)
#define PD_MEM_PDMA    		(POWER_ID + 8)
#define PD_MEM_MSC2    		(POWER_ID + 9)
#define PD_MEM_MSC1    		(POWER_ID + 10)
#define PD_MEM_MSC0    		(POWER_ID + 11)
#define PD_MEM_GMAC1   		(POWER_ID + 12)
#define PD_MEM_GMAC0   		(POWER_ID + 13)
#define PD_MEM_DMIC    		(POWER_ID + 14)
#define PD_MEM_AUDIO   		(POWER_ID + 15)
#define PD_MEM_AES     		(POWER_ID + 16)
#define PD_MEM_DSI     		(POWER_ID + 17)
#define PD_MEM_SSI1    		(POWER_ID + 18)
#define PD_MEM_SSI0    		(POWER_ID + 19)
#define PD_MEM_UART9   		(POWER_ID + 20)
#define PD_MEM_UART8   		(POWER_ID + 21)
#define PD_MEM_UART7   		(POWER_ID + 22)
#define PD_MEM_UART6   		(POWER_ID + 23)
#define PD_MEM_UART5   		(POWER_ID + 24)
#define PD_MEM_UART4   		(POWER_ID + 25)
#define PD_MEM_UART3   		(POWER_ID + 26)
#define PD_MEM_UART2   		(POWER_ID + 27)
#define PD_MEM_UART1   		(POWER_ID + 28)
#define PD_MEM_UART0   		(POWER_ID + 29)
#define PD_MEM_ROT     		(POWER_ID + 30)
#define PD_MEM_CIM     		(POWER_ID + 31)
#define PD_MEM_DPU     		(POWER_ID + 32)
#define PD_MEM_DDR_CH6 		(POWER_ID + 33)
#define PD_MEM_DDR_CH5 		(POWER_ID + 34)
#define PD_MEM_DDR_CH3 		(POWER_ID + 35)
#define PD_MEM_DDR_CH1 		(POWER_ID + 36)
#define PD_MEM_DDR_CH0 		(POWER_ID + 37)
#define PD_MEM_DDR_TOP 		(POWER_ID + 38)
#define CLK_NR_POWER		(39)

#define CLK_ID_OTHER		(POWER_ID + CLK_NR_POWER)

#define NR_CLKS		(CLK_NR_FIXED + CLK_NR_PLL + CLK_NR_MUX + CLK_NR_DIV + CLK_NR_GATE + CLK_NR_POWER)
#endif
