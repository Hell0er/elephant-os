#include "stdio.h"
int main(void){
    printf("prog_no_arg from disk.\n");
    while (1);   // 因为没有exit()，用while (1); 卡住
    return 0;
}
