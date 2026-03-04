#ifndef _PMM_H_
#define _PMM_H_

// Initialize phisical memeory manager
void pmm_init();
// Allocate a free phisical page
void *alloc_page();
// Free an allocated page
void free_page(void *pa);

void inc_page_ref(void *pa);
void dec_page_ref(void *pa);
int get_page_ref(void *pa);

#endif