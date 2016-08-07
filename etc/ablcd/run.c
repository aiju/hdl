#include <u.h>
#include <libc.h>

int *reg;

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
	char c;
	
	reg = segattach(0, "axi", nil, 4096);
	if(reg == (int*)-1)
		sysfatal("segattach: %r");
	*reg = 0;
	cmd("CLTT");
	for(;;){
		if(read(0, &c, 1) <= 0)
			exits(nil);
		if(c == '\f'){
			*reg = 0;
			cmd("CLTT");
			continue;
		}
		if(c == '\n')
			*reg = 13;
		*reg = c;
	}
}
