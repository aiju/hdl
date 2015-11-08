#include <u.h>
#include <libc.h>

int *reg;

u16int face[] = {
#include "/lib/face/48x48x1/p/pjw.1"
};

void
cmd(char *fmt, ...)
{
	va_list va;
	char *s, *p;
	
	va = va_start(va, fmt);
	s = vsmprint(fmt, va);
	va_end(va);
	for(p = s; *p != 0; p++)
		*reg = *p;
	free(s);
}

void
main()
{
	int c;
	int i, j;
	u16int *p;
	
	reg = segattach(0, "axi", nil, 4096);
	if(reg == (int*)-1)
		sysfatal("segattach: %r");
	*reg = 0;
	cmd("CL");
	cmd("DIM");
	*reg = 80-24;
	*reg = 60-24;
	*reg = 48;
	*reg = 48;
	p = face;
	for(i = 0; i < 48; i++){
		for(j = 0; j < 3; j++){
			*reg = ~*p >> 8;
			*reg = ~*p;
			p++;
		}
	}
}
