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
getpa(char *seg, u32int *pa, u32int *len)
{
	int fd;
	static char buf[512];
	char *f[10];
	
	snprint(buf, sizeof(buf), "#g/%s/ctl", seg);
	fd = open(buf, OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	read(fd, buf, 512);
	close(fd);
	tokenize(buf, f, nelem(f));
	*pa = strtol(f[4], 0, 0);
	*len = strtol(f[2], 0, 0);
}

void
threadmain()
{
	int c;
	
	v = segattach(0, "axi", nil, 409600);
	if(v == (u32int*)-1)
		sysfatal("segattach: %r");
	getpa("mem", &v[4], &v[5]);
	getpa("disk", &v[6], &v[7]);
	v[0] = 0;
	sleep(250);
	v[0] = 1;
	
	proccreate(outproc, nil, mainstacksize);
	
	for(;;){
		while(c = v[2], c >= 0)
			;
		c &= 0x7f;
		if(c == '\r' || c == 127) continue;
		print("%c", c);
	}
}
