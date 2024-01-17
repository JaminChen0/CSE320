#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    /*double* ptr = sf_malloc(sizeof(double));

    *ptr = 320320320e-320;

    printf("%f\n", *ptr);

    sf_free(ptr);

    return EXIT_SUCCESS;*/
    /*double* ptr = sf_malloc(sizeof(double));
    //sf_show_heap();

    *ptr = 3;

    printf("%f\n", *ptr);

    sf_free(ptr);*/


    sf_errno = 0;
    // We want to allocate up to exactly four pages.
    sf_malloc(4096*4 - 48 - (sizeof(sf_header) + sizeof(sf_footer)));

    //sf_show_heap();
    return EXIT_SUCCESS;
    //sf_malloc(3);


}
