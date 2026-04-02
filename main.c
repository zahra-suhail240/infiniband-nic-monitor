#include <stdio.h>
//Test run to see if I can compile the C program so far

struct ib_metrics{
    int dummy;
};

int main(){
    struct ib_metrics metrics;

    int result = get_ib_metrics(&metrics);

    if(result == -1){
        fprintf(stderr, "Error occurred while trying to get IB metrics\n");
        return -1;
    }else{
        printf("Successfully got IB metrics\n");
    }

    return 0;
}
