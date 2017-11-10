/*
 * Tlf - contest logging program for amateur radio operators
 * Copyright (C) 2017           Ervin Hegedus <airween@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * serialmodem.c
 * fltiny serial modem interface
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

int fd;
char serial_error[10] = "";

int open_port(void) {

    struct termios options;
    extern char fldigi_modem[20];
    int _fd;

    _fd = open(fldigi_modem, O_RDWR | O_NOCTTY, O_NDELAY);
    if (_fd == -1) {
        return -1;
    }
    else {
        tcgetattr(_fd, &options);
        cfsetispeed(&options, B9600);
        cfsetospeed(&options, B9600);
        options.c_cflag |= (CLOCAL | CREAD);
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;
        tcsetattr(_fd, TCSANOW, &options);
    }

    return _fd;
}

void serial_read() {
    int rb;
    char buffer[255];

    memset(buffer, '\0', 255);

    rb = read(fd, buffer, 255);
    if (rb == -1) {
        switch (errno) {
            case EAGAIN:	strcpy(serial_error, "EAGAIN");
                    break;
            case EBADF:		strcpy(serial_error, "EBADF");
                    break;
            case EFAULT:	strcpy(serial_error, "EFAULT");
                    break;
            case EINTR:		strcpy(serial_error, "EINTR");
                    break;
            case EINVAL:	strcpy(serial_error, "EINVAL");
                    break;
            case EIO:		strcpy(serial_error, "EIO");
                    break;
            case EISDIR:	strcpy(serial_error, "EISDIR");
                    break;
        }
    }

}

int serial_write(char c) {
    char buffer[2];
    buffer[0] = c;
    buffer[1] = '\0';
    return write(fd, buffer, 1);
}

int serial_init() {

    fd = -1;

    fd = open_port();

    if (fd < 0) {
        return -1;
    }

    serial_read();
    return 0;
}

int serial_close() {
    close(fd);
    return 0;
}
