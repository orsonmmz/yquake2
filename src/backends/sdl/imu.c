#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <assert.h>

/*#define IMU_STANDALONE*/
static const char* tty = "/dev/ttyACM0";
static const speed_t baud_rate = B9600;

#define BUF_SIZE 1024
static int tty_fd, valid = 0;

#ifdef IMU_STANDALONE
#define MSG(args...) fprintf(stderr, args)
#else
extern void Com_Printf(char *fmt, ...);
/*#define MSG(args...) Com_Printf(args)*/
#define MSG(args...)
#endif

void imu_init(void)
{
    struct termios tio;

    memset(&tio, 0, sizeof(tio));
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_cflag = CS8|CREAD|CLOCAL;           // 8n1, see termios.h for more information
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 5;

    tty_fd = open(tty, O_RDONLY | O_NONBLOCK);
    if(tty_fd < 0) {
        MSG("imu: could not open %s serial port\n", tty);
        return;
    }

    cfsetospeed(&tio, baud_rate);
    cfsetispeed(&tio, baud_rate);

    tcsetattr(tty_fd, TCSANOW, &tio);

    valid = 1;
    MSG("imu: good to go, reading %s\n", tty);
}

void imu_close(void)
{
    close(tty_fd);
}

#define END_FRAME 0xa

static int buf_ptr = 0;

int imu_update(float* yaw, float* pitch, float* roll)
{
    static char buf[BUF_SIZE];
    int start, end, read_bytes;
    float y, p, r;

    if(!valid)
        return 0;

    assert(buf_ptr < BUF_SIZE);
    read_bytes = read(tty_fd, &buf[buf_ptr], BUF_SIZE - buf_ptr);
    if(read_bytes > 0) {
        /*start = 0;*/
        /*end = 0;*/

        // look for beginning and end characters
        /*while(start < BUF_SIZE && buf[start] != END_FRAME) ++start;*/
        /*++start; // move to the new frame*/
        /*if(start < BUF_SIZE) {*/
            /*end = start;*/
            /*while(end < BUF_SIZE && buf[end] != END_FRAME) ++end;*/
        /*}*/
        buf_ptr += read_bytes;

        start = -1; // sentinel
        end = buf_ptr - 1;

        if(buf_ptr >= BUF_SIZE) {
            buf_ptr = 0;
            /*MSG("restart buf\n");*/
        }

        while(end > 0 && buf[end] != END_FRAME) --end;
        if(end > 0) {
            start = end - 1;
            while(start > 0 && buf[start] != END_FRAME) --start;
            ++start;
        }

        if(start > 1) {
            if(sscanf(&buf[start], "%f,%f,%f", &y, &p, &r) == 3) {
                *yaw = -y;
                *pitch = p;
                *roll = r / 4;
                MSG("imu: yaw=%f pitch=%f roll=%f\n", y, p, r);
                return 1;
            } else {
                MSG("imu: invalid frame received!\n");
                MSG("%d %d %s\n", start, end, buf);
                return 0;
            }
        } else {
            MSG("imu: frame not found in the buffer\n");
        }
    }

    return 0;
}

#ifdef IMU_STANDALONE
int main(int argc,char** argv)
{
    float yaw = 0, pitch = 0, roll = 0;

    imu_init();

    while (1)
        imu_update(&yaw, &pitch, &roll);

    imu_close();

    return EXIT_SUCCESS;
}
#endif
