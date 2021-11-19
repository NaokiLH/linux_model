#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char *argv[])
{
    int n;
    char c, *buf = "12345";
    int fd;
    int cnt = 3;
    while (1) {
        fd = open("/dev/char_dev", O_RDWR | O_APPEND);
        n = 5;
        int t = write(fd, buf, n);

        printf("%d:%s\n", t, buf);
        sleep(2);
        close(fd);
    }

    return 0;
}
