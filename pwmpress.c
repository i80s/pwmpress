/**
 * Copyright (c) 2021 i80s
 * Author: i80s
 * https://github.com/i80s/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#ifndef DEFAULT_GPIO
	#define DEFAULT_GPIO 7
#endif

static unsigned gpio_no = DEFAULT_GPIO;
static const unsigned pwm_cycle_us = 20*1000; /* 信号周期20ms (频率50Hz的倒数) */
static const unsigned pwm_move_us = 200*1000; /* 信号渐变200ms */
static const unsigned pwm_keep_us = 300*1000; /* 信号保持300ms */
static unsigned key_hold_ms = 500; /* 按键保持时间 (ms) */

static int write_to_file(const char *file, const char *str)
{
	int fd;

	if ((fd = open(file, O_WRONLY)) < 0) {
		fprintf(stderr, "*** open('%s'): %s.\n", file, strerror(errno));
		return -1;
	}
	if (write(fd, str, strlen(str)) < 0) {
		fprintf(stderr, "*** write('%s', '%s'): %s.\n", file, str, strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}

/**
 * 参数含义:
 *  $signal_us: 信号作用时长 (微秒)
 *  $duty_us_1: 起始时的占空时长 (信号周期内高电平的的时长, 微秒)
 *  $duty_us_2: 起始时的占空时长
 */
static void pwm_gpio(const char *file, unsigned signal_us, unsigned duty_us_1, unsigned duty_us_2)
{
	int fd, i;

	if ((fd = open(file, O_WRONLY)) < 0) {
		fprintf(stderr, "*** open('%s'): %s.\n", file, strerror(errno));
		return;
	}

	for (i = 0; i < signal_us / pwm_cycle_us; i++) {
		int duty_us = (int)duty_us_1 + (int)(duty_us_2 - duty_us_1) * i / (int)(signal_us / pwm_cycle_us);
		char buf[20];

		/* 输出高电平 */
		sprintf(buf, "1\n");
		write(fd, buf, strlen(buf));
		usleep(duty_us);

		/* 输出低电平 */
		sprintf(buf, "0\n");
		write(fd, buf, strlen(buf));
		usleep(pwm_cycle_us - duty_us);
	}

	close(fd);
}

static void print_help(int argc, char *argv[])
{
	printf("PWM key press CLI.\n");
	printf("Usage:\n");
	printf("  %s [options]\n", argv[0]);
	printf("  -d <gpio>           GPIO ID (default: %u)\n", gpio_no);
	printf("  -t <msec>           delayed time in ms (default: %u)\n", key_hold_ms);
	printf("  -h                  print this help\n");
}

int main(int argc, char *argv[])
{
	char the_path[64], the_data[64];
	struct stat sb;
	int rc;

	while ((rc = getopt(argc, argv, "d:t:h")) != -1) {
		switch (rc) {
		case 'd':
			gpio_no = strtoul(optarg, NULL, 10);
			break;
		case 't':
			key_hold_ms = strtoul(optarg, NULL, 10);
			break;
		case 'h':
			print_help(argc, argv);
			exit(0);
			break;
		case '?':
			print_help(argc, argv);
			exit(1);
		}
	}

	/* Initialize the GPIO port */
	sprintf(the_path, "/sys/class/gpio/gpio%u/direction", gpio_no);
	if (stat(the_path, &sb) != 0) {
		sprintf(the_data, "%u\n", gpio_no);
		rc = write_to_file("/sys/class/gpio/export", the_data);
		if (rc < 0)
			return 1;
	}
	rc = write_to_file(the_path, "low\n");

	/* Operate the values */
	sprintf(the_path, "/sys/class/gpio/gpio%u/value", gpio_no);
	pwm_gpio(the_path, pwm_move_us, 1000, 2000); /* 信号渐变, 占空1~2ms */
	pwm_gpio(the_path, pwm_keep_us, 2000, 2000); /* 信号保持 */
	usleep(key_hold_ms*1000); /* 按压XXms */
	pwm_gpio(the_path, pwm_move_us, 2000, 1000); /* 信号渐变, 占空2~1ms */
	pwm_gpio(the_path, pwm_keep_us, 1000, 1000); /* 信号保持 */

	return 0;
}

