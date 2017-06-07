/*
 * Copyright (c) 2012 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/cpuquiet.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/touchboost.h>
#include <asm/cputime.h>

#define TOUCHBOOST_DURATION	(1000 * USEC_PER_MSEC)
#define DEF_UP_DELAY		(50)
#define DEF_DOWN_DELAY		(1000)
#define DEF_LOAD_SAMPLE_RATE	(20)

typedef enum {
	IDLE,
	DISABLED,
	ENABLED,
} TOUCHBOOST_STATE;

struct idle_info {
	u64 idle_last;
	u64 last_timestamp;
	u64 idle_current;
	u64 timestamp;
};

static DEFINE_PER_CPU(struct idle_info, idleinfo);
static DEFINE_PER_CPU(unsigned int, cpu_load);

static struct timer_list load_timer;
static bool load_timer_active;

/* configurable parameters */
static unsigned long up_delay;
static unsigned long down_delay;

/* governor global variables */
static unsigned int  load_sample_rate = DEF_LOAD_SAMPLE_RATE; /* msec */
static struct delayed_work touch_work;
static TOUCHBOOST_STATE touchboost_state;
static struct kobject *touch_kobject;

static void calculate_load_timer(unsigned long data)
{
	int i;
	u64 idle_time;
	u64 elapsed_time;

	if (!load_timer_active)
		return;

	for_each_online_cpu(i) {
		struct idle_info *iinfo = &per_cpu(idleinfo, i);
		unsigned int *load = &per_cpu(cpu_load, i);

		iinfo->idle_last = iinfo->idle_current;
		iinfo->last_timestamp = iinfo->timestamp;
		iinfo->idle_current =
			get_cpu_idle_time_us(i, &iinfo->timestamp);
		elapsed_time = iinfo->timestamp - iinfo->last_timestamp;

		idle_time = iinfo->idle_current - iinfo->idle_last;
		idle_time *= 100;
		do_div(idle_time, elapsed_time);
		*load = 100 - idle_time;
	}
	mod_timer(&load_timer, jiffies + msecs_to_jiffies(load_sample_rate));
}

static void start_load_timer(void)
{
	int i;

	if (!load_timer_active) {
                load_timer_active = true;
                for_each_online_cpu(i) {
                        struct idle_info *iinfo = &per_cpu(idleinfo, i);
                        iinfo->idle_current =
                                get_cpu_idle_time_us(i, &iinfo->timestamp);
                }
                mod_timer(&load_timer, jiffies + msecs_to_jiffies(100));
        }
}

static void stop_load_timer(void)
{
	if (load_timer_active) {
		load_timer_active = false;
		del_timer(&load_timer);
	}
}

static unsigned int get_slowest_cpu(void)
{
	unsigned int cpu = nr_cpu_ids;
	unsigned long min_load = ULONG_MAX;
	int i;

	for_each_online_cpu(i) {
		unsigned int *load = &per_cpu(cpu_load, i);

		if ((i > 0) && (min_load > *load)) {
			cpu = i;
			min_load = *load;
		}
	}

	return cpu;
}

static unsigned int cpu_highest_load(void)
{
	unsigned int max_load = 0;
	int i;

	for_each_online_cpu(i) {
		unsigned int *load = &per_cpu(cpu_load, i);
		max_load = max(max_load, *load);
	}

	return max_load;
}

static unsigned int count_slow_cpus(unsigned int limit)
{
	unsigned int cpu_count = 0;
	int i;

	for_each_online_cpu(i) {
		unsigned int *load = &per_cpu(cpu_load, i);

		if (*load <= limit)
			cpu_count++;
	}

	return cpu_count;
}

static bool load_is_skewed(void)
{
	unsigned long skewed_load = cpu_highest_load() / 4;

	/*  skewed: freq targets for at least 2 CPUs are below 25% threshold */
	return count_slow_cpus(skewed_load) >= 1;
}

/**
 * touch_queue_delayed_work - queue touch work on CPU 0
 * @delay: number of jiffies to wait before queuing
 *
 * Returns %false if @work was already on a queue, %true otherwise.  If
 * @delay is zero and @touch_work is idle, it will be scheduled for immediate
 * execution.
 */
static bool touch_queue_delayed_work(int delay) {
    return queue_delayed_work_on(0, system_freezable_wq, &touch_work, delay);
}

static void check_cpu_cores(struct work_struct *work)
{
	unsigned int cpu = nr_cpu_ids;
	switch (touchboost_state) {
	case IDLE:
		break;
	case DISABLED:
		if (load_is_skewed()) {
			cpu = get_slowest_cpu();
			if (cpu < nr_cpu_ids)
				cpuquiet_quiesence_cpu(cpu, false);
			stop_load_timer();
		} else
			touch_queue_delayed_work(down_delay);
		break;
	case ENABLED:
		cpu = cpumask_next_zero(0, cpu_online_mask);
		if (cpu < nr_cpu_ids)
			cpuquiet_wake_cpu(cpu, false);
		stop_load_timer();
		break;
	default:
		pr_err("%s: invalid cpuquiet touch governor state %d\n",
		       __func__, touchboost_state);
	}
}

static int touch_cpufreq_transition(struct notifier_block *nb,
				    unsigned long state, void *unused)
{
	if (state == CPUFREQ_POSTCHANGE || state == CPUFREQ_RESUMECHANGE) {
		switch (touchboost_state) {
		case IDLE:
			if (touchboost_is_enabled(TOUCHBOOST_DURATION)) {
				touchboost_state = ENABLED;
                touch_queue_delayed_work(up_delay);
			} else {
				touchboost_state = DISABLED;
                touch_queue_delayed_work(down_delay);
			}
			break;
		case DISABLED:
			if (touchboost_is_enabled(TOUCHBOOST_DURATION)) {
				touchboost_state = ENABLED;
				touch_queue_delayed_work(up_delay);
			}
			break;
		case ENABLED:
			if (!touchboost_is_enabled(TOUCHBOOST_DURATION)) {
				touchboost_state = DISABLED;
				touch_queue_delayed_work(down_delay);
			}
			break;
		default:
			pr_err("%s: invalid cpuquiet touch governor state %d\n",
			       __func__, touchboost_state);
		}
		start_load_timer();
	}
	return NOTIFY_OK;
}

static struct notifier_block touch_cpufreq_nb = {
	.notifier_call = touch_cpufreq_transition,
};

static void delay_callback(struct cpuquiet_attribute *attr)
{
	unsigned long val;

	if (attr) {
		val = (*((unsigned long *) (attr->param)));
		(*((unsigned long *) (attr->param))) = msecs_to_jiffies(val);
	}
}

CPQ_BASIC_ATTRIBUTE(load_sample_rate, 0644, uint);
CPQ_ATTRIBUTE(up_delay, 0644, ulong, delay_callback);
CPQ_ATTRIBUTE(down_delay, 0644, ulong, delay_callback);

static struct attribute *touch_attributes[] = {
	&up_delay_attr.attr,
	&down_delay_attr.attr,
	&load_sample_rate_attr.attr,
	NULL,
};

static const struct sysfs_ops touch_sysfs_ops = {
	.show = cpuquiet_auto_sysfs_show,
	.store = cpuquiet_auto_sysfs_store,
};

static struct kobj_type ktype_touch = {
	.sysfs_ops = &touch_sysfs_ops,
	.default_attrs = touch_attributes,
};

static int touch_sysfs(void)
{
	int err;

	touch_kobject = kzalloc(sizeof(*touch_kobject), GFP_KERNEL);

	if (!touch_kobject)
		return -ENOMEM;

	err = cpuquiet_kobject_init(touch_kobject, &ktype_touch, "touch");

	if (err)
		kfree(touch_kobject);

	return err;
}

static void touch_stop(void)
{
	/*
	   first unregister the notifiers. This ensures the governor state
	   can't be modified by a cpufreq transition
	*/
	cpufreq_unregister_notifier(&touch_cpufreq_nb, CPUFREQ_TRANSITION_NOTIFIER);

	/* now we can force the governor to be idle */
	touchboost_state = IDLE;
	cancel_delayed_work_sync(&touch_work);
	del_timer(&load_timer);

	kobject_put(touch_kobject);
}

static int touch_start(void)
{
	int err;

	err = touch_sysfs();
	if (err)
		return err;

	INIT_DELAYED_WORK(&touch_work, check_cpu_cores);

	up_delay = msecs_to_jiffies(DEF_UP_DELAY);
	down_delay = msecs_to_jiffies(DEF_DOWN_DELAY);

	cpufreq_register_notifier(&touch_cpufreq_nb, CPUFREQ_TRANSITION_NOTIFIER);

	init_timer(&load_timer);
	load_timer.function = calculate_load_timer;

	touchboost_state = DISABLED;
	return 0;
}

struct cpuquiet_governor touch_governor = {
	.name		= "touch",
	.start		= touch_start,
	.stop		= touch_stop,
	.owner		= THIS_MODULE,
};

static int __init init_touch(void)
{
	return cpuquiet_register_governor(&touch_governor);
}

static void __exit exit_touch(void)
{
	cpuquiet_unregister_governor(&touch_governor);
}

MODULE_LICENSE("GPL");
#ifdef CONFIG_CPUQUIET_DEFAULT_GOV_TOUCH
fs_initcall(init_touch);
#else
module_init(init_touch);
#endif
module_exit(exit_touch);

