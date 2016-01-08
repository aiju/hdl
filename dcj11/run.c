#include <u.h>
#include <libc.h>
#include <thread.h>

u32int *v;

void
outproc(void *)
{
	char c;
	int fd;
	
	fd = open("/dev/consctl", OWRITE);
	if(fd >= 0)
		fprint(fd, "rawon");

	for(;;){
		if(read(0, &c, 1) <= 0)
			threadexitsall(nil);
		while((int)v[3] >= 0)
			;
		v[3] = c;
	}
}

void
threadmain()
{
	int c;
	
	v = segattach(0, "axi", nil, 409600);
	if(v == (u32int*)-1)
		sysfatal("segattach: %r");
	v[0] = 0;
	sleep(250);
	v[0] = 1;
	
	proccreate(outproc, nil, mainstacksize);
	
	for(;;){
		while(c = v[2], c >= 0)
			;
		print("%c", c & 0x7f);
	}
}
