/*
 * ASoC Driver for monome-snd
 * connected to a Raspberry Pi
 *
 *  Created on: 20160610
 *      Author: murray foster <mrafoster@gmail.com>
 *              based on code from existing soc/bcm drivers
 *
 * Copyright (C) 2016 Murray Foster
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

#define CLK_RATE 12288000UL

static int snd_rpi_monome_init(struct snd_soc_pcm_runtime *rtd)
{
  return 0;
}

static int snd_rpi_monome_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)
{
  struct snd_soc_pcm_runtime *rtd = substream->private_data;
  struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
  struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
  struct snd_soc_card *card = rtd->card;

  int sysclk = 12288000;
  int ret = 0;

  /* Don't worry about clock id and direction (it's ignored in cs4270 driver) */
  ret = snd_soc_dai_set_sysclk(codec_dai, 0, sysclk, 0);

  if (ret < 0)
    {
      dev_err(card->dev, "Unable to set CS4270 system clock.");
      return ret;
    }

  /* cs4270 datasheet says SCLK must be 48x or 64x for max system performance */
  return snd_soc_dai_set_bclk_ratio(cpu_dai, 64);
}

static int snd_rpi_monome_startup(struct snd_pcm_substream *substream) {
  return 0;
}

static void snd_rpi_monome_shutdown(struct snd_pcm_substream *substream) {
}

/* machine stream operations */
static struct snd_soc_ops snd_rpi_monome_ops = {
  .hw_params = snd_rpi_monome_hw_params,
  .startup = snd_rpi_monome_startup,
  .shutdown = snd_rpi_monome_shutdown,
};

SND_SOC_DAILINK_DEFS(snd_rpi_monome,
	DAILINK_COMP_ARRAY(COMP_CPU("3f203000.i2s")),
	DAILINK_COMP_ARRAY(COMP_CODEC("cs4270.1-0048", "cs4270-hifi")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("3f203000.i2s")));

static struct snd_soc_dai_link snd_rpi_monome_dai[] = {
	{
		.name = "monome cs4270",
		.stream_name = "monome cs4270",
		.dai_fmt = SND_SOC_DAIFMT_CBM_CFM | \
		           SND_SOC_DAIFMT_I2S | \
		           SND_SOC_DAIFMT_NB_NF,
		.ops = &snd_rpi_monome_ops,
		.init = snd_rpi_monome_init,
		SND_SOC_DAILINK_REG(snd_rpi_monome),
	},
};

static struct snd_soc_card snd_rpi_monome = {
	.name = "snd_rpi_monome",
	.owner = THIS_MODULE,
	.dai_link = snd_rpi_monome_dai,
	.num_links = ARRAY_SIZE(snd_rpi_monome_dai),
};

static int snd_rpi_monome_probe(struct platform_device *pdev)
{
	int ret = 0;

	snd_rpi_monome.dev = &pdev->dev;

	if (pdev->dev.of_node) {
		struct device_node *i2s_node;
		struct snd_soc_dai_link_component *cpu_dai = &(snd_rpi_monome_dai[0].cpus[0]);
		struct snd_soc_dai_link_component *platform_dai = &(snd_rpi_monome_dai[0].platforms[0]);

		i2s_node = of_parse_phandle(pdev->dev.of_node,
					    "i2s-controller", 0);

		if (i2s_node) {
			//dev_alert(&pdev->dev, "using overlay i2s_node = %p\n", i2s_node);
			cpu_dai->name = NULL;
			cpu_dai->of_node = i2s_node;
			platform_dai->name = NULL;
			platform_dai->of_node = i2s_node;
		} else {
			dev_err(&pdev->dev, "monome-snd-overly: i2s_node = NULL\n");
			dev_err(&pdev->dev, "falling back to cpu_dai: %s, platform_dai: %s\n", cpu_dai->name, platform_dai->name);
		}

	}

	ret = snd_soc_register_card(&snd_rpi_monome);
	if (ret == -EPROBE_DEFER) {
		dev_alert_once(&pdev->dev, "probe; waiting for codec.\n");
	} else if (ret) {
	    dev_err(&pdev->dev, "snd_soc_register_card() failed (%d)\n", ret);
	} else {
		dev_info(&pdev->dev, "probe; card registered.\n");
	}
	return ret;
}

static int snd_rpi_monome_remove(struct platform_device *pdev)
{
	return snd_soc_unregister_card(&snd_rpi_monome);
}

static const struct of_device_id snd_rpi_monome_of_match[] = {
	{ .compatible = "monome", },
	{},
};
MODULE_DEVICE_TABLE(of, snd_rpi_monome_of_match);

static struct platform_driver snd_rpi_monome_driver = {
    .driver = {
		.name = "snd-rpi-monome",
		.owner = THIS_MODULE,
		.of_match_table = snd_rpi_monome_of_match,
	},
	.probe = snd_rpi_monome_probe,
	.remove = snd_rpi_monome_remove,
};
module_platform_driver(snd_rpi_monome_driver);

MODULE_AUTHOR("Murray Foster <mrafoster@gmail.com>");
MODULE_DESCRIPTION("ASoC Driver for monome-snd connected to a Raspberry Pi");
MODULE_LICENSE("GPL v2");
