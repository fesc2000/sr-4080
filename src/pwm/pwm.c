/* pwm.c - A simple tool to control a fan via a PWM GPIO
 *
 * Copyright 2020 Felix Schmidt
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <string.h>

int cpid = 0;

int main(int argc, char **argv)
{
	char *gpio = argv[1] ? argv[1] : "";
	char *duty = argv[2];
	int fd = open (gpio, O_WRONLY);
	int dtc = duty ? atoi(duty) : 100;
	int pulse = argv[3] ? atoi(argv[3]) : 10000;
	int on, off;
	int ppid = getpid();

    if (!strcmp (gpio, "-h"))
    {
        printf ("usage: pwm sysfs-gpio-value-file [duty-precentage [cycle-time]]\n"
                "       duty-precentage: 0 .. 100 (default 100)\n"
                "       cycle-time:      total cycle time in uSec\n"
                "Example: pwm /sys/class/gpio/gpio80/value 20 10000\n"
                "         For a 20% duty cycle using GPIO80\n"
                );
        exit (0);
    }


	if (fd == -1)
	{
		perror(gpio);
		exit(1);
	}

	if ((dtc <= 0) > (dtc > 100))
	{
		exit(1);
	}

	on = (pulse * dtc) / 100;
	off = pulse - on;

	if (-1 == write(fd, "1\n", 3))
	{
		perror ("write");
		exit (1);
	}

	cpid = fork();

	if (cpid == -1)
	{
		perror ("fork");
		exit (1);
	}

	if (cpid == 0)
	{
		while (1)
		{
			if (-1 == write(fd, "1\n", 3))
			{
				perror ("write");
				exit (1);
			}

			if (getppid() != ppid)
			{
				printf ("worker: parent has died? Leave fan blowing.\n");
				exit(1);
			}

			if (off)
			{
				usleep(on);

				if (-1 == write(fd, "0\n", 3))
				{
					perror ("write");
					exit (1);
				}
				usleep(off);
			}
			else
			{
				sleep(1);
			}
		}
	}

	signal(SIGINT, SIG_IGN);
	printf ("worker: %d\n", cpid);

	while (1)
	{
		int rc;
		int wstatus;

		rc = waitpid(-1, &wstatus, WUNTRACED|WCONTINUED);

		printf("restoring fan\n");
		if (-1 == write(fd, "1\n", 3))
		{
			perror ("write");
			exit (1);
		}

		if (rc == -1)
		{
			printf("killing worker\n");
			if (-1 == kill(cpid, SIGINT))
			{
				perror ("waitpid");
				exit(1);
			}
			continue;
		}

		if (WIFSTOPPED(wstatus) || WIFCONTINUED(wstatus))
		{
			printf("child was stopped/resumed\n");
			sleep(1);
			continue;
		}

		if (WIFEXITED(wstatus))
		{
			rc = WEXITSTATUS(wstatus);
			printf("child exit: %d\n", rc);
		}
		else
		{
			rc = 1;
			printf("child aborted\n");
		}

		printf("rc=%d\n", rc);
		return rc;
	}
	return 1;
}
