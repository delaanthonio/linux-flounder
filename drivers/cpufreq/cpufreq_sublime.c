/*
 *  drivers/cpufreq/cpufreq_sublime.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *            (C)  2009 Alexander Clouter <alex@digriz.org.uk>
 *            (C)  2015 Dela Anthonio
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "cpufreq_governor.h"

/* Sublime governor macros */
#define DEF_HIGHSPEED_FREQUENCY_UP_THRESHOLD (85)
#define DEF_FREQUENCY_UP_THRESHOLD           (80)
#define DEF_MICRO_FREQUENCY_UP_THRESHOLD     (70)
#define DEF_FREQUENCY_DOWN_THRESHOLD         (25)
#define DEF_MICRO_FREQUENCY_DOWN_THRESHOLD   (40)
#define DEF_FREQUENCY_STEP                   (8)
#define DEF_MICRO_FREQUENCY_STEP             (3)
#define DEF_SAMPLING_DOWN_FACTOR             (1)
#define MAX_SAMPLING_DOWN_FACTOR             (10)
#define DEF_HIGHSPEED_FREQUENCY              (1734000)
#define MAX(x,y)                             (x > y ? x : y)
#define MIN(x,y)                             (x < y ? x : y)

static DEFINE_PER_CPU(struct sb_cpu_dbs_info_s, sb_cpu_dbs_info);

static inline unsigned int get_freq_target(struct sb_dbs_tuners *sb_tuners,
					   struct cpufreq_policy *policy)
{
	unsigned int freq_target = (sb_tuners->freq_step * policy->max) / 100;

	/* max frequency cannot be less than 100. But who knows... */
	if (unlikely(freq_target == 0))
		freq_target = DEF_FREQUENCY_STEP;

	return freq_target;
}

static inline unsigned int get_micro_freq_target(struct sb_dbs_tuners *sb_tuners,
					   struct cpufreq_policy *policy)
{
	unsigned int freq_target = (sb_tuners->micro_freq_step * policy->max) / 100;

	/* max frequency cannot be less than 100. But who knows... */
	if (unlikely(freq_target == 0))
		freq_target = DEF_MICRO_FREQUENCY_STEP;

	return freq_target;
}

/*
 * Every sampling_rate, we check, if current idle time is less than 20%
 * (default), then we try to increase frequency. Every sampling_rate *
 * sampling_down_factor, we check, if current idle time is more than 80%
 * (default), then we try to decrease frequency
 *
 * Any frequency increase takes it to the maximum frequency. Frequency reduction
 * happens at minimum steps of 5% (default) of maximum frequency
 */
static void sb_check_cpu(int cpu, unsigned int load)
{
	struct sb_cpu_dbs_info_s *dbs_info = &per_cpu(sb_cpu_dbs_info, cpu);
	struct cpufreq_policy *policy = dbs_info->cdbs.cur_policy;
	struct dbs_data *dbs_data = policy->governor_data;
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int freq_target;

	/*
	 * break out if we 'cannot' reduce the speed as the user might
	 * want freq_step to be zero
	 */
	if (sb_tuners->freq_step == 0)
		return;

	/* Check for high-speed frequency increase */
	if (load > sb_tuners->highspeed_up_threshold) {
	    dbs_info->down_skip = 0;

	    // Stop if the current speed is already the maximum
	    if (dbs_info->requested_freq == policy->max)
	        return;

	    // Jump to the high-speed frequency if the current policy is below it
	    if (dbs_info->requested_freq < sb_tuners->highspeed_freq){
	        dbs_info->requested_freq = sb_tuners->highspeed_freq;
	    } else {
	        dbs_info->requested_freq += get_micro_freq_target(sb_tuners, policy);
	        // Make sure the requested frequency is at most the maximum frequency
                dbs_info->requested_freq = MIN(dbs_info->requested_freq, policy->max);
	    }

	    __cpufreq_driver_target(policy, dbs_info->requested_freq,
	        CPUFREQ_RELATION_H);
	    return;
	}

	/* Check for frequency increase */
	else if (load >= sb_tuners->up_threshold) {
	    dbs_info->down_skip = 0;

	    /* break out early if the high-speed freq is already set */
	    if (dbs_info->requested_freq == sb_tuners->highspeed_freq)
	        return;

	    dbs_info->requested_freq += get_freq_target(sb_tuners, policy);

            // Ensure the requested frequency is at most the high-speed frequency
	    dbs_info->requested_freq = MIN(dbs_info->requested_freq, sb_tuners->highspeed_freq);

	    __cpufreq_driver_target(policy, dbs_info->requested_freq,
	        CPUFREQ_RELATION_H);
	    return;
	}

        /* Check for micro frequency increase */
	if (load > sb_tuners->micro_up_threshold) {
	    dbs_info->down_skip = 0;

	    // Stop if the current speed is already the high-speed frequency
	    if (dbs_info->requested_freq == sb_tuners->highspeed_freq)
	        return;

	    // Jump to the high-speed frequency if the current policy is below it
            dbs_info->requested_freq += get_micro_freq_target(sb_tuners, policy);
            dbs_info->requested_freq = MIN(dbs_info->requested_freq, sb_tuners->highspeed_freq);

	    __cpufreq_driver_target(policy, dbs_info->requested_freq,
                                    CPUFREQ_RELATION_H);
	    return;
	    }


	// if sampling_down_factor is active break out early
	if (++dbs_info->down_skip < sb_tuners->sampling_down_factor)
		return;
	dbs_info->down_skip = 0;

	/* Check for frequency decrease */
	if (load < sb_tuners->down_threshold) {

		// break out early if the frequency is set to the minimum
		if (policy->cur == policy->min)
			return;

		freq_target = get_freq_target(sb_tuners, policy);
		if (dbs_info->requested_freq > freq_target) {
			dbs_info->requested_freq -= freq_target;
			dbs_info->requested_freq = MAX (dbs_info->requested_freq, policy->min);
		} else
			dbs_info->requested_freq = policy->min;

		__cpufreq_driver_target(policy, dbs_info->requested_freq,
				CPUFREQ_RELATION_L);
		return;
	}

	/* Check for micro frequency decrease */
	if (load < sb_tuners->micro_down_threshold) {

		// break out early if the frequency is set to the minimum
		if (policy->cur == policy->min)
			return;

		freq_target = get_micro_freq_target(sb_tuners, policy);
		if (dbs_info->requested_freq > freq_target) {
			dbs_info->requested_freq -= freq_target;
			dbs_info->requested_freq = MAX (dbs_info->requested_freq, policy->min);
		} else
			dbs_info->requested_freq = policy->min;

		__cpufreq_driver_target(policy, dbs_info->requested_freq,
				CPUFREQ_RELATION_L);
		return;
	}
}

static void sb_dbs_timer(struct work_struct *work)
{
	struct sb_cpu_dbs_info_s *dbs_info = container_of(work,
			struct sb_cpu_dbs_info_s, cdbs.work.work);
	unsigned int cpu = dbs_info->cdbs.cur_policy->cpu;
	struct sb_cpu_dbs_info_s *core_dbs_info = &per_cpu(sb_cpu_dbs_info,
			cpu);
	struct dbs_data *dbs_data = dbs_info->cdbs.cur_policy->governor_data;
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	int delay = delay_for_sampling_rate(sb_tuners->sampling_rate);
	bool modify_all = true;

	mutex_lock(&core_dbs_info->cdbs.timer_mutex);
	if (!need_load_eval(&core_dbs_info->cdbs, sb_tuners->sampling_rate))
		modify_all = false;
	else
		dbs_check_cpu(dbs_data, cpu);

	gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy, delay, modify_all);
	mutex_unlock(&core_dbs_info->cdbs.timer_mutex);
}

static int dbs_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
		void *data)
{
	struct cpufreq_freqs *freq = data;
	struct sb_cpu_dbs_info_s *dbs_info =
					&per_cpu(sb_cpu_dbs_info, freq->cpu);
	struct cpufreq_policy *policy;

	if (!dbs_info->enable)
		return 0;

	policy = dbs_info->cdbs.cur_policy;

	/*
	 * we only care if our internally tracked frequency moves outside the 'valid'
	 * ranges of frequency available to us otherwise we do not change it
	*/
	if (dbs_info->requested_freq > policy->max
			|| dbs_info->requested_freq < policy->min)
		dbs_info->requested_freq = freq->new;

	return 0;
}

/************************** sysfs interface ************************/
static struct common_dbs_data sb_dbs_cdata;

static ssize_t store_sampling_down_factor(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;

	sb_tuners->sampling_down_factor = input;
	return count;
}

static ssize_t store_sampling_rate(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	sb_tuners->sampling_rate = max(input, dbs_data->min_sampling_rate);
	return count;
}

static ssize_t store_highspeed_up_threshold(struct dbs_data *dbs_data, const char *buf,
                                  size_t count)
{
    struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
    unsigned int input;
    int ret;
    ret = sscanf(buf, "%u", &input);

    if (ret != 1 || input > 100 || input <= sb_tuners->micro_down_threshold)
        return -EINVAL;

    sb_tuners->highspeed_up_threshold = input;
    return count;
}

static ssize_t store_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 || input <= sb_tuners->micro_down_threshold)
		return -EINVAL;

	sb_tuners->up_threshold = input;
	return count;
}

static ssize_t store_micro_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 || input <= sb_tuners->down_threshold)
		return -EINVAL;

	sb_tuners->micro_up_threshold = input;
	return count;
}

static ssize_t store_down_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than 11 otherwise frequency will not fall */
	if (ret != 1 || input < 11 || input > 100 ||
			input >= sb_tuners->micro_up_threshold)
		return -EINVAL;

	sb_tuners->down_threshold = input;
	return count;
}

static ssize_t store_micro_down_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than the down threshold frequency will not fall */
	if (ret != 1 || sb_tuners->down_threshold < 11 || input > 100 ||
			input >= sb_tuners->up_threshold)
		return -EINVAL;

	sb_tuners->micro_down_threshold = input;
	return count;
}

static ssize_t store_ignore_nice_load(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int input, j;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == sb_tuners->ignore_nice_load) /* nothing to do */
		return count;

	sb_tuners->ignore_nice_load = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct sb_cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(sb_cpu_dbs_info, j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
					&dbs_info->cdbs.prev_cpu_wall, 0);
		if (sb_tuners->ignore_nice_load)
			dbs_info->cdbs.prev_cpu_nice =
				kcpustat_cpu(j).cpustat[CPUTIME_NICE];
	}
	return count;
}

static ssize_t store_freq_step(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > 100)
		input = 100;

	/*
	 * no need to test here if freq_step is zero as the user might actually
	 * want this, they would be crazy though :)
	 */
	sb_tuners->freq_step = input;
	return count;
}

static ssize_t store_micro_freq_step(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > 10)
		input = 10;

	else if (input < 1)
		input = 1;

	sb_tuners->micro_freq_step = input;
	return count;
}

static ssize_t store_highspeed_freq(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sb_dbs_tuners *sb_tuners = dbs_data->tuners;
        struct cpufreq_policy *policy;

	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

        if (ret != 1)
                return -EINVAL;

        if (input < policy->min)
                input = policy->min;

        else if (input > policy->max)
                input  = policy->max;

        sb_tuners->highspeed_freq = input;
	return count;
}

show_store_one(sb, sampling_rate);
show_store_one(sb, sampling_down_factor);
show_store_one(sb, highspeed_up_threshold);
show_store_one(sb, up_threshold);
show_store_one(sb, micro_up_threshold);
show_store_one(sb, down_threshold);
show_store_one(sb, micro_down_threshold);
show_store_one(sb, ignore_nice_load);
show_store_one(sb, freq_step);
show_store_one(sb, micro_freq_step);
show_store_one(sb, highspeed_freq);
declare_show_sampling_rate_min(sb);

gov_sys_pol_attr_rw(sampling_rate);
gov_sys_pol_attr_rw(sampling_down_factor);
gov_sys_pol_attr_rw(highspeed_up_threshold);
gov_sys_pol_attr_rw(up_threshold);
gov_sys_pol_attr_rw(micro_up_threshold);
gov_sys_pol_attr_rw(down_threshold);
gov_sys_pol_attr_rw(micro_down_threshold);
gov_sys_pol_attr_rw(ignore_nice_load);
gov_sys_pol_attr_rw(freq_step);
gov_sys_pol_attr_rw(micro_freq_step);
gov_sys_pol_attr_ro(highspeed_freq);
gov_sys_pol_attr_ro(sampling_rate_min);

static struct attribute *dbs_attributes_gov_sys[] = {
	&sampling_rate_min_gov_sys.attr,
	&sampling_rate_gov_sys.attr,
	&sampling_down_factor_gov_sys.attr,
	&highspeed_up_threshold_gov_sys.attr,
	&up_threshold_gov_sys.attr,
	&micro_up_threshold_gov_sys.attr,
	&down_threshold_gov_sys.attr,
	&micro_down_threshold_gov_sys.attr,
	&ignore_nice_load_gov_sys.attr,
	&highspeed_freq_gov_sys.attr,
	&freq_step_gov_sys.attr,
	&micro_freq_step_gov_sys.attr,
	NULL
};

static struct attribute_group sb_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "sublime",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&sampling_rate_min_gov_pol.attr,
	&sampling_rate_gov_pol.attr,
	&sampling_down_factor_gov_pol.attr,
	&highspeed_up_threshold_gov_pol.attr,
	&up_threshold_gov_pol.attr,
	&micro_up_threshold_gov_pol.attr,
	&down_threshold_gov_pol.attr,
	&micro_down_threshold_gov_pol.attr,
	&ignore_nice_load_gov_pol.attr,
	&highspeed_freq_gov_pol.attr,
	&freq_step_gov_pol.attr,
	&micro_freq_step_gov_pol.attr,
	NULL
};

static struct attribute_group sb_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "sublime",
};

/************************** sysfs end ************************/

static int sb_init(struct dbs_data *dbs_data)
{
	struct sb_dbs_tuners *tuners;

	tuners = kzalloc(sizeof(struct sb_dbs_tuners), GFP_KERNEL);
	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}
        tuners->highspeed_up_threshold = DEF_HIGHSPEED_FREQUENCY_UP_THRESHOLD;
	tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;
	tuners->micro_up_threshold = DEF_MICRO_FREQUENCY_UP_THRESHOLD;
	tuners->down_threshold = DEF_FREQUENCY_DOWN_THRESHOLD;
	tuners->micro_down_threshold = DEF_MICRO_FREQUENCY_DOWN_THRESHOLD;
	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice_load = 0;
	tuners->freq_step = DEF_FREQUENCY_STEP;
	tuners->micro_freq_step = DEF_MICRO_FREQUENCY_STEP;
	tuners->highspeed_freq = DEF_HIGHSPEED_FREQUENCY;

	dbs_data->tuners = tuners;
	dbs_data->min_sampling_rate = MIN_SAMPLING_RATE_RATIO *
		jiffies_to_usecs(10);
	mutex_init(&dbs_data->mutex);
	return 0;
}

static void sb_exit(struct dbs_data *dbs_data)
{
	kfree(dbs_data->tuners);
}

define_get_cpu_dbs_routines(sb_cpu_dbs_info);

static struct notifier_block sb_cpufreq_notifier_block = {
	.notifier_call = dbs_cpufreq_notifier,
};

static struct sb_ops sb_ops = {
	.notifier_block = &sb_cpufreq_notifier_block,
};

static struct common_dbs_data sb_dbs_cdata = {
	.governor = GOV_SUBLIME,
	.attr_group_gov_sys = &sb_attr_group_gov_sys,
	.attr_group_gov_pol = &sb_attr_group_gov_pol,
	.get_cpu_cdbs = get_cpu_cdbs,
	.get_cpu_dbs_info_s = get_cpu_dbs_info_s,
	.gov_dbs_timer = sb_dbs_timer,
	.gov_check_cpu = sb_check_cpu,
	.gov_ops = &sb_ops,
	.init = sb_init,
	.exit = sb_exit,
};

static int sb_cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	return cpufreq_governor_dbs(policy, &sb_dbs_cdata, event);
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_SUBLIME
static
#endif
struct cpufreq_governor cpufreq_gov_sublime = {
	.name			= "sublime",
	.governor		= sb_cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_gov_dbs_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_sublime);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_sublime);
}

MODULE_AUTHOR("Dela Anthonio");
MODULE_DESCRIPTION("'cpufreq_sublime' - A dynamic cpufreq governor for "
		"Low Latency Frequency Transition capable processors "
		"optimized for devices with the Tegra K1 processor"
		"and have limited battry life");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_SUBLIME
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
