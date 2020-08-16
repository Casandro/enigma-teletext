#include <stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<strings.h>
#include<dirent.h>


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
	fclose(f);
	return hash;
}


char *hashtable[HASHSIZE]={NULL};

void handle_file(const char *fn)
{
	//printf("handle file %s\n", fn);
	uint64_t h=calculate_page_hash(fn);
	uint64_t entry=h%HASHSIZE;
	if (hashtable[entry]==NULL) { //No matching entry found
		size_t s=strlen(fn)+1;
		char *p=malloc(s);
		if (p==NULL) {
			printf("malloc failed\n");
			return;
		}
		strcpy(p, fn);
		hashtable[entry]=p;
	} else {
		int cres=compare_pages(fn, hashtable[entry]);
		if (cres==0) {
	//		printf("deleting %s\n",fn);
			unlink(fn);
		}
	}
}


void handle_directory(const char *dir)
{
	//printf("handle_directory %s\n", dir);
	DIR *folder=opendir(dir);
	if (folder == NULL) return;
	struct dirent *entry;
	while ( (entry=readdir(folder) )) {
		if (strstr(entry->d_name, ".t42")==NULL) continue;
		char filename[1024];
		memset(filename, 0, sizeof(filename));
		snprintf(filename, sizeof(filename)-1, "%s/%s", dir, entry->d_name);
		handle_file(filename);
	}
	closedir(folder);
}


void handle_subdir(const char *dir, const int godown)
{
	if (godown<=0) return handle_directory(dir);
	printf("handle_subdir %s %d\n", dir, godown);
	DIR *folder=opendir(dir);
	if (folder == NULL) return;
	struct dirent *entry;
	while ( (entry=readdir(folder) )) {
		if (entry->d_type!=DT_DIR) continue;
		char filename[1024];
		memset(filename, 0, sizeof(filename));
		snprintf(filename, sizeof(filename)-1, "%s/%s", dir, entry->d_name);
		handle_subdir(filename, godown-1);
	}
	closedir(folder);
}

void handle_deduplication(const char *dir)
{
	memset(hashtable, 0, sizeof(hashtable));
	handle_subdir(dir,2);
	int n;
	for (n=0; n<HASHSIZE; n++) {
		if (hashtable[n]!=NULL) {
			free(hashtable[n]);
			hashtable[n]=NULL;
		}
	}
}


int main(int argc, char *argv[])
{
	if (argc!=2) {
		printf("This program deletes duplicate copies of a page keeping the first one.\nUsage: %s <directory>\n", argv[0]);
		return 1;
	}
	handle_deduplication(argv[1]);
}
