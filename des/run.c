#include <u.h>
#include <libc.h>
#include <libsec.h>

enum {
	RUN,
	DONE,
	N = 0x03,
	STARTH,
	STARTL,
	GOALH,
	GOALL
};

#define FEFE 0xfefefefefefefefeULL

void
main()
{
	uchar k[8] = {0x00, 0x00, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78};
	uchar da[8] = {0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41}, db[8];
	ulong sch[32];
	ulong *r;
	int i, n;
	ulong d, e;
	uvlong v;
	
	des_key_setup(k, sch);
	block_cipher(sch, da, 0);
	r = segattach(0, "axi", (void*) 0x40000000, 1048576);
	if(r == (void*)-1)
		sysfatal("segattach: %r");
	
	r[GOALH] = da[0] << 24 | da[1] << 16 | da[2] << 8 | da[3];
	r[GOALL] = da[4] << 24 | da[5] << 16 | da[6] << 8 | da[7];
	r[STARTL] = 0;
	n = r[N];
	for(i = 0; i < n; i++){
		r[STARTH] = (0x1000000U * i / n) << 8;
		sleep(0);
		r[RUN] ^= 1<<i;
		sleep(0);
	}
	for(;;){
		do{
			sleep(10);
			d = r[DONE] ^ (1<<n) - 1;
		}while(d == 0);
		while(d != 0){
			d ^= e = d & -d;
			i = (e & 0xaaaaaaaa) != 0;
			i |= ((e & 0xcccccccc) != 0) << 1;
			i |= ((e & 0xf0f0f0f0) != 0) << 2;
			i |= ((e & 0xff00ff00) != 0) << 3;
			i |= ((e & 0xffff0000) != 0) << 4;
			v = (uvlong)r[0x40 | i<<1] << 32 | r[0x41 | i<<1];
			v &= FEFE;
			for(;; v = v - 1 & FEFE){
				k[0] = v >> 56;
				k[1] = v >> 48;
				k[2] = v >> 40;
				k[3] = v >> 32;
				k[4] = v >> 24;
				k[5] = v >> 16;
				k[6] = v >> 8;
				k[7] = v;
				des_key_setup(k, sch);
				memset(db, 0x41, 8);
				block_cipher(sch, db, 0);
				if(memcmp(da, db, 7) == 0)
					break;
			}
			print("%ullx\n", v);
			v = (v | ~FEFE) + 1 & FEFE;
			r[STARTL] = v;
			r[STARTH] = v >> 32;
			r[RUN] = 1<<i;
		}
	}
}
