#include<stdio.h>

int main(){
    int n=5000000;
    int counter = 0;
    int i;
    for(;counter<n;){
        for(i=0;i<10;i++){
            printf("%010d ", counter);
            counter++;
        }
        printf("\n");
    }
    return 0;
}
