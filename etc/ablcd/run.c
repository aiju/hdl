#include <u.h>
#include <libc.h>

int
main()
{
	int c;
	int *v;
	
	v = segattach(0, "axi", nil, 4096);
	if(v == (int*)-1)
		sysfatal("segattach: %r");
	for(;;){
		if(read(0, &c, 1) <= 0)
			return 0;
		*v = c;
	}
}