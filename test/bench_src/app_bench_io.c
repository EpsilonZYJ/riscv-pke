#include "user/user_lib.h"
#include "util/types.h"

#ifndef IO_CHUNK_SIZE
#define IO_CHUNK_SIZE 256
#endif

#ifndef IO_ROUNDS
#define IO_ROUNDS 8192
#endif

int main(void) {
    const char *path = "/io_bench.bin";
    char wbuf[IO_CHUNK_SIZE];
    char rbuf[IO_CHUNK_SIZE];
    int wsum = 0;
    int rsum = 0;

    for (int i = 0; i < IO_CHUNK_SIZE; i++) {
        wbuf[i] = 'A' + (i % 26);
        wsum += (int)wbuf[i];
    }

    int fd = open(path, O_RDWR);
    if (fd < 0)
        fd = open(path, O_CREAT | O_RDWR);
    if (fd < 0) {
        printu("io_bench open failed, fd=%d\n", fd);
        exit(-1);
        return -1;
    }

    if (lseek_u(fd, 0, LSEEK_SET) < 0) {
        printu("io_bench lseek(begin) failed\n");
        close(fd);
        exit(-1);
        return -1;
    }

    int write_bytes = 0;
    int read_bytes = 0;

    for (int i = 0; i < IO_ROUNDS; i++) {
        int n = write_u(fd, wbuf, IO_CHUNK_SIZE);
        if (n != IO_CHUNK_SIZE) {
            printu("io_bench write failed at round=%d, ret=%d\n", i, n);
            close(fd);
            exit(-1);
            return -1;
        }
        write_bytes += n;
    }

    if (lseek_u(fd, 0, LSEEK_SET) < 0) {
        printu("io_bench lseek(rewind) failed\n");
        close(fd);
        exit(-1);
        return -1;
    }

    for (int i = 0; i < IO_ROUNDS; i++) {
        for (int j = 0; j < IO_CHUNK_SIZE; j++)
            rbuf[j] = 0;

        int n = read_u(fd, rbuf, IO_CHUNK_SIZE);
        if (n != IO_CHUNK_SIZE) {
            printu("io_bench read failed at round=%d, ret=%d\n", i, n);
            close(fd);
            exit(-1);
            return -1;
        }

        read_bytes += n;

        int cur = 0;
        for (int j = 0; j < IO_CHUNK_SIZE; j++)
            cur += (int)rbuf[j];
        rsum += cur;
    }

    close(fd);

    printu("io_bench chunk=%d rounds=%d write_bytes=%d read_bytes=%d\n", IO_CHUNK_SIZE, IO_ROUNDS, write_bytes,
           read_bytes);
    printu("io_bench checksum_w=%d checksum_r=%d\n", wsum * IO_ROUNDS, rsum);

    if (rsum != wsum * IO_ROUNDS) {
        printu("io_bench checksum mismatch\n");
        exit(-1);
        return -1;
    }

    exit(0);
    return 0;
}
