#include <stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<strings.h>
#include<dirent.h>
#include<inttypes.h>
#include<errno.h>


#define HASHSIZE (16*1024)

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
	if (hash==0) return 1;
	return hash;
}

#define NUMLEAVES (11)

int char_to_leave(const char c)
{
	if (c<'0') return 10;
	if (c>'9') return 10;
	return c-'0';
}

typedef struct tree_node_s {
	uint64_t hash;
	struct tree_node_s *leaves[NUMLEAVES];
	char *name;
} tree_node_t;

tree_node_t root; //Should be initialized with zero

tree_node_t *create_tree_node(void)
{
	tree_node_t *tn=malloc(sizeof(tree_node_t));
	if (tn==NULL) return tn;
	memset(tn, 0, sizeof(tree_node_t));
	return tn;
}

tree_node_t *find_tree_node_(const char *s, const char *fn, tree_node_t *base)
{
	if (base==NULL) return NULL;
	if ((*s=='\0') || (*s=='.')) {
		base->name=malloc(strlen(fn)+1);
		strcpy(base->name, fn);
		return base;
	}
	int l=char_to_leave(*s);
	if (base->leaves[l]==NULL) base->leaves[l]=create_tree_node();
	return find_tree_node_(&(s[1]), fn, base->leaves[l]);
}

tree_node_t *find_tree_node(const char *s, const char *path)
{
	return find_tree_node_(s, path, &root);
}

void delete_tree(tree_node_t *t)
{
	if (t==NULL) return;
	int n;
	for (n=0; n<NUMLEAVES; n++) {
		delete_tree(t->leaves[n]);
	}
	if (t->name!=NULL) free(t->name);

	memset(t, 0, sizeof(tree_node_t));
	if (t!=&root) free(t);
}


//tree_node_t *hashtable[HASHSIZE]={NULL};

int read_hashes(FILE *f, const char *dir)
{
	if (f==NULL) return 0;
	char fn[512];
	uint64_t hash=0;
	int cnt=0;

	while (fscanf(f, "%s%"SCNx64, fn, &hash)==2) {
		char fn2[1024];
		snprintf(fn2, sizeof(fn2), "%s/%s", dir, fn);
		tree_node_t *tn=find_tree_node(fn, fn2);
		if (tn!=NULL) {
			tn->hash=hash;
			cnt=cnt+1;
		}
	}

	return cnt;
}

int print_hashes_(FILE *f, tree_node_t *b)
{
	int cnt=0;
	if (b==NULL) return 0;
	if ((b->hash!=0) && (b->name!=NULL) && (b->hash!=~0)) {
		char *bn=strrchr(b->name, '/');
		if (bn!=NULL) {
			fprintf(f, "%s\t%"PRIx64"\n", &(bn[1]), b->hash);
			cnt=cnt+1;
		}
	}
	int n;
	for (n=0; n<NUMLEAVES; n++)
	{
		cnt=cnt+print_hashes_(f, b->leaves[n]);
	}
	return cnt;
}

int ht_size=(HASHSIZE);
tree_node_t *ht[HASHSIZE];

int delete_double_(tree_node_t *base)
{
	int delcnt=0;
	if (base==NULL) return 0;
	if (ht==NULL) return 0; //Should not be possible
	if ( (base->hash!=0) && (base->name!=NULL) ) { //We are at a leaf
		uint64_t entry=base->hash%ht_size;
		if ((ht[entry]!=NULL) && (ht[entry]->hash==base->hash)) { //perhaps there is a double
			int cmp=compare_pages(base->name, ht[entry]->name);
			if (cmp==0) {
				//printf("Deleting File %s\n", base->name);
				unlink(base->name);
				base->hash=0;
				delcnt=delcnt+1;
			}
		} else {
			ht[entry]=base;
		}
	}
	int n;
	for (n=0; n<NUMLEAVES; n++) {
		delcnt=delcnt+delete_double_(base->leaves[n]);
	}
	return delcnt;
}

int delete_double(const uint64_t ht_size)
{
	memset(ht, 0, sizeof(ht));
	int delcnt=delete_double_(&root);
	return delcnt;
}


void handle_directory(const char *dir)
{
	memset(&root, 0, sizeof(root));

	int rhcnt=0;

	char filename[1024];
	memset(filename, 0, sizeof(filename));
	snprintf(filename, sizeof(filename)-1, "%s/.hash_cache", dir);
	FILE *f=fopen(filename, "r");
	if (f!=NULL) {
		rhcnt=read_hashes(f, dir);
		fclose(f);
	}

	int chcnt=0; //How many hashes were calculated
	int delcnt=0; //How many files were deleted

	//printf("handle_directory %s\n", dir);
	DIR *folder=opendir(dir);
	if (folder == NULL) return;
	struct dirent *entry;
	while ( (entry=readdir(folder) )) {
		if (strstr(entry->d_name, ".t42")==NULL) continue;
		memset(filename, 0, sizeof(filename));
		snprintf(filename, sizeof(filename)-1, "%s/%s", dir, entry->d_name);
		tree_node_t *tn=find_tree_node(entry->d_name, filename);
		if (tn==NULL) return; //should not happen
		if (tn->hash!=0) {
			continue;
		}
		if (tn->hash==0) {
			tn->hash=calculate_page_hash(filename);
			chcnt=chcnt+1;
		}	
	}
	closedir(folder);
	delcnt=delete_double(1024*128);
	memset(filename, 0, sizeof(filename));
	snprintf(filename, sizeof(filename)-1, "%s/.hash_cache", dir);
	int wcnt=0;
	f=fopen(filename, "w");
	if (f!=NULL) {
		wcnt=print_hashes_(f, &root);
		fclose(f);
	}	
	printf("%s\tR:%4d    C:%4d    D:%4d    W:%4d\n", dir, rhcnt, chcnt, delcnt, wcnt);
	delete_tree(&root);
}


void handle_subdir(const char *dir, const int godown)
{
	if (godown<=0) return handle_directory(dir);
//	printf("handle_subdir %s %d\n", dir, godown);
	DIR *folder=opendir(dir);
	if (folder == NULL) return;
	struct dirent *entry;
	while ( (entry=readdir(folder) )) {
		if (entry->d_type!=DT_DIR) continue;
		if (entry->d_name[0]=='.') continue;
		char filename[1024];
		memset(filename, 0, sizeof(filename));
		snprintf(filename, sizeof(filename)-1, "%s/%s", dir, entry->d_name);
		handle_subdir(filename, godown-1);
	}
	closedir(folder);
}

void handle_deduplication(const char *dir)
{
	handle_subdir(dir,2);
}


int main(int argc, char *argv[])
{
	if (argc!=2) {
		printf("This program deletes duplicate copies of a page keeping the first one.\nUsage: %s <directory>\n", argv[0]);
		return 1;
	}
	handle_deduplication(argv[1]);
}
