#ifndef BARRIER_H_
#define BARRIER_H_

#define barrier \
    __asm volatile("    dsb\n" \
                   "    isb\n" \
                   ::: "memory")

#endif /* BARRIER_H_ */
