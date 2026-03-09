#ifndef _SYNC_UTILS_H_
#define _SYNC_UTILS_H_

/**
 * 同步屏障函数
 * 
 * 用于多个处理器核心之间的同步，确保所有核心都到达某个点后才能继续执行
 * 
 * @param counter 指向共享计数器的指针，用于记录已到达屏障点的核心数量
 * @param all 总共需要等待的核心数量
 */
static inline void sync_barrier(volatile int *counter, int all) {

  int local;

  // 使用原子加法指令将计数器加1，并获取加法之前的值
  asm volatile("amoadd.w %0, %2, (%1)\n"
               : "=r"(local)
               : "r"(counter), "r"(1)
               : "memory");

  // 如果当前累计到达的核心数还未达到总数，则等待其他核心到达
  if (local + 1 < all) {
    do {
      // 循环读取计数器的值，直到所有核心都到达屏障点
      asm volatile("lw %0, (%1)\n" : "=r"(local) : "r"(counter) : "memory");
    } while (local < all);
  }
}

#endif