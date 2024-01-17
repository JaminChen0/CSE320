#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    sf_errno = 0;
    // We want to allocate up to exactly four pages.
    sf_malloc(4096*4 - 48 - (sizeof(sf_header) + sizeof(sf_footer)));

    //sf_show_heap();
    return EXIT_SUCCESS;
    //sf_malloc(3);


}
