//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
	
int main(int argc, char **argv)
{
//	__asm__ __volatile__("xchg %ax, %dx");
	__asm__ __volatile__(".byte 0x66,0xbb,0xfc,0xfe,0x30,0x00");
//	__asm__ __volatile__(".byte 90");
//	__asm__ __volatile__(".byte 66");
	__asm__ __volatile__("mov %ebx,0x0003fefc");
	__asm__ __volatile__(".byte 0xea,0xb8,0xa1,0x00,0x00,0x20,0x00");
	__asm__ __volatile__("in %dx,%eax");
	__asm__ __volatile__("rdrand %edx");
	__asm__ __volatile__("rdrand %edi");
	__asm__ __volatile__("rdrand %esi");
	int a = 0;
	int i = 0;
	int b = 0;
	for(i=0;i<10;i++){

		a = a + 1;

	}

//	printf("a = %d",a);
	if(a<=5){
		b = 1;
}
	else{
		b = 2;
}
}
