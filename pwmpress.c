/*
 * Copyright (c) 2021 Justin Liu
 * Author: Justin Liu <rssnsj@gmail.com>
 * https://github.com/rssnsj/
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
static unsigned operate_ms = 400;
static unsigned hold_ms = 500;

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
 *  $operate_us: 信号持续时长 (微秒)
 *  $cycle_us: 信号周期时长 (频率的倒数, 微秒)
 *  $duty_us: 占空时长 (信号周期内高电平的的时长, 微秒)
 */
static void pwm_gpio(const char *file, unsigned operate_us, unsigned cycle_us, unsigned duty_us)
{
	int fd, i;

	if ((fd = open(file, O_WRONLY)) < 0) {
		fprintf(stderr, "*** open('%s'): %s.\n", file, strerror(errno));
		return;
	}

	for (i = 0; i < operate_us / cycle_us; i++) {
		char buf[20];

		/* 输出高电平 */
		sprintf(buf, "1\n");
		write(fd, buf, strlen(buf));
		usleep(duty_us);

		/* 输出低电平 */
		sprintf(buf, "0\n");
		write(fd, buf, strlen(buf));
		usleep(cycle_us - duty_us);
	}

	close(fd);
}

static void print_help(int argc, char *argv[])
{
	printf("GPIO relay control CLI.\n");
	printf("Usage:\n");
	printf("  %s [options]\n", argv[0]);
	printf("  -d <gpio>           GPIO ID (default: %u)\n", gpio_no);
	printf("  -t <msec>           delayed time in ms (default: %u)\n", hold_ms);
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
			hold_ms = strtoul(optarg, NULL, 10);
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
	pwm_gpio(the_path, operate_ms*1000, 20000, 2500); /* 50Hz, 占空2.5ms, 持续0.5s */
	usleep(hold_ms*1000); /* 按压XXms */
	pwm_gpio(the_path, operate_ms*1000, 20000, 1500); /* 50Hz, 占空1.5ms, 持续0.5s */

	return 0;
}

