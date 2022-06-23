 /*
 * Copyright (C) 2014 Ingenic Semiconductor Co., Ltd.
 *	http://www.ingenic.com
 * Author: cli <chen.li@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/gpio.h>
#include "../as-v1/asoc-aic.h"
#include <linux/delay.h>


static int x1600_i2s_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;
	int id = 0;
	int sysclk = 12288000;
	int div = 12288000 / params_rate(params) / 64;
	int aic_mode = 0, codec_mode = 0;

#ifdef CONFIG_SND_ASOC_INGENIC_MASTER_MODE
	aic_mode |= SND_SOC_DAIFMT_CBS_CFM;
	codec_mode |= SND_SOC_DAIFMT_CBS_CFS;
#endif
#ifdef CONFIG_SND_ASOC_INGENIC_SLAVE_MODE
	aic_mode |= SND_SOC_DAIFMT_CBM_CFM;
	codec_mode |= SND_SOC_DAIFMT_CBM_CFM;
#endif
#ifdef CONFIG_SND_ASOC_INGENIC_I2S_MODE
	aic_mode |= SND_SOC_DAIFMT_I2S;
	codec_mode |= SND_SOC_DAIFMT_I2S;
#endif
#ifdef CONFIG_SND_ASOC_INGENIC_LEFT_J_MODE
	aic_mode |= SND_SOC_DAIFMT_MSB;
	codec_mode |= SND_SOC_DAIFMT_LEFT_J;
#endif

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		id |= INGENIC_I2S_PLAYBACK;
	} else {
		id |= INGENIC_I2S_CAPTURE;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, id, sysclk, SND_SOC_CLOCK_OUT);
	if (ret)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, id, sysclk, SND_SOC_CLOCK_IN);
	if (ret)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, aic_mode);
	if (ret)
		return ret;

	ret = snd_soc_dai_set_fmt(codec_dai, codec_mode);
	if (ret)
		return ret;


	ret = snd_soc_dai_set_clkdiv(cpu_dai, id, div);
	if (ret)
		return ret;

	return 0;
}

static int x1600_i2s_hw_free(struct snd_pcm_substream *substream)
{
	/*notify release pll*/
	return 0;
};

static struct snd_soc_ops x1600_i2s_ecdc_ops = {
	.hw_params = x1600_i2s_hw_params,
	.hw_free = x1600_i2s_hw_free,
};

struct gpio_desc *en_spk;

static int x1600_i2s_ecdc_dai_link_init(struct snd_soc_pcm_runtime *rtd)
{
	if (IS_ERR(en_spk))
		return PTR_ERR(en_spk);
	gpiod_set_value_cansleep(en_spk, 1);

	return 0;
}

static struct snd_soc_card *panda_card;

static int snd_x1600_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct snd_soc_card *card;
	struct snd_soc_dai_link *dai_link;
	struct device_node *snd_node = pdev->dev.of_node;
	int num_links;

	num_links = of_property_count_strings(snd_node, "ingenic,dai-link");
	if (num_links < 0)
		return num_links;
	BUG_ON(!num_links);

	card = (struct snd_soc_card *)devm_kzalloc(&pdev->dev,
			sizeof(struct snd_soc_card), GFP_KERNEL);
	if (!card)
		return -ENOMEM;
	panda_card = card;

	dai_link = (struct snd_soc_dai_link *)devm_kzalloc(&pdev->dev,
			sizeof(struct snd_soc_dai_link) * num_links, GFP_KERNEL);
	if (!dai_link)
		return -ENOMEM;

	card->num_links = num_links;
	card->dai_link = dai_link;
	card->owner = THIS_MODULE;
	card->dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(card, "ingenic,model");
	if (ret)
		return ret;

	en_spk = devm_gpiod_get_optional(card->dev, "spken", GPIOD_OUT_LOW);

	for (i = 0; i < card->num_links; i++) {
		dai_link[i].cpu_of_node = of_parse_phandle(snd_node, "ingenic,cpu-dai" , i);
		dai_link[i].platform_of_node = of_parse_phandle(snd_node, "ingenic,platform", i);
		dai_link[i].codec_of_node = of_parse_phandle(snd_node, "ingenic,codec", i);

		ret = of_property_read_string_index(snd_node, "ingenic,codec-dai", i,
				&(dai_link[i].codec_dai_name));

		if (ret || !dai_link[i].cpu_of_node ||
				!dai_link[i].codec_of_node ||
				!dai_link[i].platform_of_node)
			return -ENODEV;
		ret = of_property_read_string_index(snd_node, "ingenic,dai-link", i,
				&(dai_link[i].name));
		if (ret)
			return -ENODEV;
		ret = of_property_read_string_index(snd_node, "ingenic,stream", i,
				&(dai_link[i].stream_name));
		if (ret)
			dai_link[i].stream_name = dai_link[i].name;

		dev_dbg(&pdev->dev, "dai_link %s\n", dai_link[i].name);
		dev_dbg(&pdev->dev, "stream_name %s\n", dai_link[i].stream_name);
		dev_dbg(&pdev->dev, "cpu %s(%s)\n", dai_link[i].cpu_of_node->name,
				dai_link[i].cpu_of_node->full_name);
		dev_dbg(&pdev->dev, "platform %s(%s)\n", dai_link[i].platform_of_node->name,
				dai_link[i].platform_of_node->full_name);
		dev_dbg(&pdev->dev, "codec dai %s\n", dai_link[i].codec_dai_name);
		dev_dbg(&pdev->dev, "codec %s(%s)\n", dai_link[i].codec_of_node->name,
				dai_link[i].codec_of_node->full_name);

		if (!strcmp(dai_link[i].name, "i2s-ecodec")) {
			dai_link->ops = &x1600_i2s_ecdc_ops;
			dai_link->init = x1600_i2s_ecdc_dai_link_init;
		}
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, card);

	dev_info(&pdev->dev, "Sound Card successed\n");

	return ret;
}

static int snd_x1600_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id sound_dt_match[] = {
	{ .compatible = "ingenic,x1600-sound", .data = NULL },
	{},
};
MODULE_DEVICE_TABLE(of, sound_dt_match);

static struct platform_driver snd_x1600_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ingenic-fpga-x1600",
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(sound_dt_match),
	},
	.probe = snd_x1600_probe,
	.remove = snd_x1600_remove,
};
module_platform_driver(snd_x1600_driver);

MODULE_AUTHOR("cli<chen.li@ingenic.com>");
MODULE_DESCRIPTION("ALSA SoC FPGA Snd Card");
MODULE_LICENSE("GPL");
