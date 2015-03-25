#include <stdio.h>


int main(void){
	struct blabla{
			int i;
			char data [256]
	} raz;
	
	raz.i = 15;
    int i = 0;
    for (i=0; i<256; i++){
		raz.data[i] = i;
	}
	//double x = 0.5564;
	char* mx = &raz;
	for(i = 0; i < sizeof(raz); ++i){
		printf("%d = %ld\n", i, (long)mx[i]);
}
}