#include "inconsistent_map.h"
__kernel void vector_add(__global char *Dat, __global size_t *Ctl, __global size_t *Ret) {

    // Get the index of the current element to be processed
    int i = get_global_id(0);
    __global char *b = Dat;
//    float c = 0;
    if (i == 0) {
        printf("In GPU:\nDat=%p\nCtl=%p\nRet=%p\n",Dat,Ctl,Ret);
        // printf("%p\n", aa);
        switch (Ctl[0]) {
                case RD_PAGE:
                        Ret[0] = Dat;
                        for (size_t j = 0; j < 250; j++) {
                                printf("%c ",  Dat[j]);
                        }
                        break;
                case RD_PAGE_WITH_OFFSET:
                        b += Ctl[1];
                        for (size_t j = 0; j < 250; j++) {
                                printf("%c", b[j]);
                        }
                        break;
                case WR_PAGE_WITH_OFFSET:
                        b += Ctl[1];
                        __global char *c = Ctl;
                        for (size_t j = 0; j < 250; j++) {
                                b[j] = '.';
                        }
                        break;
        }
        printf("\n");
        
    }
}
