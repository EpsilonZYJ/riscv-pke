#include "user_lib.h"
#include "util/types.h"

// 进程测试：最多尝试创建的子进程数量（上限由内核资源决定）。
#define MAX_CHILDREN_TRY 29
// I/O 测试：单次读写块大小。
#define IO_CHUNK_SIZE 256
// I/O 测试：读写轮数，总数据量 = IO_CHUNK_SIZE * IO_ROUNDS。
#define IO_ROUNDS 1024

// 测试系统可同时启动的最大进程数（简化版）。
// 做法：不断 fork，直到失败；然后等待所有已创建子进程退出。
static int test_max_processes(void) {
    int children = 0;

    for (int i = 0; i < MAX_CHILDREN_TRY; i++) {
        int pid = fork();
        // fork 失败，说明达到当前可创建进程上限。
        if (pid < 0) {
            break;
        }
        // 子进程直接退出，避免额外逻辑影响测试。
        if (pid == 0) {
            printu("[proc] child process created, num=%d\n", i + 1);
            exit(0);
            return 0;
        }
        // 只有父进程会走到这里，统计成功创建的子进程数量。
        children++;
    }

    // 回收所有子进程，避免僵尸进程。
    for (int i = 0; i < children; i++) {
        wait(-1);
    }

    printu("[proc] max concurrent user processes (including parent): %d\n", children + 1);
    printu("[proc] max concurrent child processes: %d\n", children);

    return children;
}

// 基础 I/O 压力测试：写入固定模式数据，再读回并校验。
static int test_io_stress(void) {
    // 在根目录下使用一个固定测试文件。
    const char *path = "/io_bench.bin";
    char wbuf[IO_CHUNK_SIZE];
    char rbuf[IO_CHUNK_SIZE];
    // wsum/rsum 用于校验写入与读回数据是否一致。
    int wsum = 0;
    int rsum = 0;

    // 构造写入数据：循环字母 A-Z。
    for (int i = 0; i < IO_CHUNK_SIZE; i++) {
        wbuf[i] = 'A' + (i % 26);
        wsum += (int)wbuf[i];
    }

    // 先尝试读写方式打开，若文件不存在再创建。
    int fd = open(path, O_RDWR);
    if (fd < 0)
        fd = open(path, O_CREAT | O_RDWR);
    if (fd < 0) {
        printu("[io] open failed, fd=%d\n", fd);
        return -1;
    }

    // 定位到文件开头，保证测试从一致位置开始。
    if (lseek_u(fd, 0, SEEK_SET) < 0) {
        printu("[io] lseek(begin) failed\n");
        close(fd);
        return -1;
    }

    int write_calls = 0;
    int read_calls = 0;
    int total_written = 0;
    int total_read = 0;

    // 连续写入 IO_ROUNDS 次。
    for (int i = 0; i < IO_ROUNDS; i++) {
        int n = write_u(fd, wbuf, IO_CHUNK_SIZE);
        if (n != IO_CHUNK_SIZE) {
            printu("[io] write failed at round=%d, ret=%d\n", i, n);
            close(fd);
            return -1;
        }
        write_calls++;
        total_written += n;
    }

    // 写完后回到文件开头，准备读取验证。
    if (lseek_u(fd, 0, SEEK_SET) < 0) {
        printu("[io] lseek failed\n");
        close(fd);
        return -1;
    }

    // 连续读取 IO_ROUNDS 次，并累计校验和。
    for (int i = 0; i < IO_ROUNDS; i++) {
        for (int j = 0; j < IO_CHUNK_SIZE; j++)
            rbuf[j] = 0;
        int n = read_u(fd, rbuf, IO_CHUNK_SIZE);
        if (n != IO_CHUNK_SIZE) {
            printu("[io] read failed at round=%d, ret=%d\n", i, n);
            close(fd);
            return -1;
        }
        read_calls++;
        total_read += n;

        int cur = 0;
        for (int j = 0; j < IO_CHUNK_SIZE; j++)
            cur += (int)rbuf[j];
        rsum += cur;
    }

    close(fd);

    printu("[io] write calls=%d, read calls=%d\n", write_calls, read_calls);
    printu("[io] bytes written=%d, bytes read=%d\n", total_written, total_read);
    printu("[io] checksum write=%d, checksum read=%d\n", wsum * IO_ROUNDS, rsum);

    // 校验失败表示读写数据不一致。
    if (rsum != wsum * IO_ROUNDS) {
        printu("[io] checksum mismatch\n");
        return -1;
    }

    printu("[io] stress test finished\n");
    return 0;
}

int main(void) {
    printu("\n======== performance test start ========\n");

    // 依次执行：进程数量测试 + I/O 测试。
    int p = test_max_processes();
    int io = test_io_stress();

    // 任一测试失败则返回错误。
    if (p < 0 || io < 0) {
        printu("[result] test failed\n");
        exit(-1);
        return -1;
    }

    printu("[result] test finished successfully\n");
    printu("======== performance test end =========\n");

    exit(0);
    return 0;
}
