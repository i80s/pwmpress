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
#include <time.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#ifndef DEFAULT_GPIO
	#define DEFAULT_GPIO 27
#endif

static unsigned gpio_no = DEFAULT_GPIO;

static const unsigned pwm_cycle_us = 20 * 1000; /* 信号周期20ms (频率50Hz的倒数) */

static const unsigned pwm_duty1_us =  700;
static const unsigned pwm_duty2_us = 2000;

static const unsigned pwm_move_us  = 200 * 1000; /* 信号渐变 */
static const unsigned pwm_keep_us  = 100 * 1000; /* 信号保持 */

static unsigned key_hold_ms = 200; /* 按键保持时间 (ms) */

/**
 * Functions delayMicrosecondsHard, delayMicroseconds are copyied from:
 *  https://github.com/WiringPi/WiringPi/blob/master/wiringPi/wiringPi.c
 */
void delayMicrosecondsHard(unsigned int howLong)
{
	struct timeval tNow, tLong, tEnd ;

	gettimeofday (&tNow, NULL) ;
	tLong.tv_sec  = howLong / 1000000 ;
	tLong.tv_usec = howLong % 1000000 ;
	timeradd(&tNow, &tLong, &tEnd) ;

	while (timercmp (&tNow, &tEnd, <))
		gettimeofday (&tNow, NULL) ;
}
void delayMicroseconds(unsigned int howLong)
{
	struct timespec sleeper ;
	unsigned int uSecs = howLong % 1000000 ;
	unsigned int wSecs = howLong / 1000000 ;

	if (howLong == 0) {
		return ;
	} else if (howLong < 100) {
		delayMicrosecondsHard (howLong) ;
	} else {
		sleeper.tv_sec  = wSecs ;
		sleeper.tv_nsec = (long)(uSecs * 1000L) ;
		nanosleep(&sleeper, NULL) ;
	}
}

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
 *  $duty_us_1: 起始时的占空时长 (信号周期内高电平的的时长, 微秒)
 *  $duty_us_2: 结束时的占空时长
 *  $signal_us: 信号作用时长 (微秒)
 */
static void pwm_gpio(const char *file, unsigned duty_us_1, unsigned duty_us_2, unsigned signal_us)
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
		delayMicrosecondsHard(duty_us);

		/* 输出低电平 */
		sprintf(buf, "0\n");
		write(fd, buf, strlen(buf));
		delayMicrosecondsHard(pwm_cycle_us - duty_us);
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
	pwm_gpio(the_path, pwm_duty1_us, pwm_duty2_us, pwm_move_us); /* 信号渐变(1->2) */
	pwm_gpio(the_path, pwm_duty2_us, pwm_duty2_us, pwm_keep_us + key_hold_ms*1000); /* 信号保持(2) + 按压时间 */
	pwm_gpio(the_path, pwm_duty2_us, pwm_duty1_us, pwm_move_us); /* 信号渐变(2->1) */
	pwm_gpio(the_path, pwm_duty1_us, pwm_duty1_us, pwm_keep_us); /* 信号保持(1) */

	return 0;
}

