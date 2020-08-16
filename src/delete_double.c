#include <stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<strings.h>


#define HASHSIZE (32*1024*1024)

#define BIT(x, y) ((x>>y)&0x1)
int de_hamm(uint8_t x)
{
	return BIT(x,1) | (BIT(x,3)<<1) | (BIT(x,5)<<2) | (BIT(x,7)<<3);
}


int compare_pages(const char *fn1, const char *fn2)
{
	FILE *f1=fopen(fn1, "r");
	if (f1==NULL) return -1;
	FILE *f2=fopen(fn2, "r");
	if (f2==NULL) return -1;

	uint8_t packet1[42];
	uint8_t packet2[42];

	while( (fread(packet1, 1, sizeof(packet1), f1)>0) &&
		(fread(packet2, 1, sizeof(packet2), f2)>0) ) {

		int mpag1=de_hamm(packet2[1])<<4 | de_hamm(packet2[0]);
		int magazine1=mpag1&0x7;
		int row1=mpag1>>3;
		int mpag2=de_hamm(packet2[1])<<4 | de_hamm(packet2[0]);
		int magazine2=mpag2&0x7;
		int row2=mpag2>>3;


		if ( (row1==0) && (row2==0) ) continue; //Ignore header row as time can change
		int n;
		for (n=0; n<sizeof(packet1); n++) {
			if (packet1[n]!=packet2[n]) {
				fclose(f1);
				fclose(f2);
				return 1; //We found a difference
			}
		}
	}
	fclose(f1);
	fclose(f2);
	return 0; // We found no difference

}

uint64_t calculate_page_hash(const char *fn)
{
	FILE *f=fopen(fn, "r");
	if (f==NULL) return -1;
	uint8_t buf[4096]; //No teletext page will ever be that large
	fread(buf, 1, 42, f); //Skip first 42 octets;
	memset(buf, 0, sizeof(buf));
	size_t r=fread(buf, 1, sizeof(buf), f);
	uint64_t hash=0;
	int n;
	for (n=0; n<r; n++) {
		uint64_t s=buf[n];
		s=s<<(n%(64-7));
		hash=hash^(s);
	}
	return hash;
}


char *hashtable[HASHSIZE]={NULL};


int main(int argc, char *argv[])
{
	if (argc<2) {
		printf("This program deletes duplicate copies of a page keeping the first one.\nUsage: %s <file1> <file2> ... <filen>\n", argv[0]);
		return 1;
	}
	if (argc<4) return 1;
	memset(hashtable, 0, sizeof(hashtable));
	int n;
	for (n=1; n<argc-1; n++) {
		char *fn=argv[n];
		uint64_t h=calculate_page_hash(fn);
		uint64_t entry=h%HASHSIZE;
		if (hashtable[entry]==NULL) { //No matching entry found
			size_t s=strlen(fn)+1;
			char *p=malloc(s);
			if (p==NULL) {
				printf("malloc failed\n");
				return 1;
			}
			strcpy(p, fn);
			hashtable[entry]=p;
		} else {
			int cres=compare_pages(fn, hashtable[entry]);
			if (cres==0) {
				printf("deleting %s\n",fn);
				unlink(fn);
			}
		}
	}
}
