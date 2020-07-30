#include <stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<strings.h>


#define BIT(x, y) ((x>>y)&0x1)
int de_hamm(uint8_t x)
{
	return BIT(x,1) | (BIT(x,3)<<1) | (BIT(x,5)<<2) | (BIT(x,7)<<3);
}

typedef struct packet_s {
	struct packet_s *next;
	int row;
	uint8_t packet[42];
} packet_t;


void delete_packet(packet_t *p)
{
	if (p==NULL) return;
	packet_t *next=p->next;
	free(p);
	return delete_packet(next);
}

packet_t *append_packet(packet_t *cur, const int row, const uint8_t *packet)
{
	if (packet==NULL) return NULL;
	if ((cur!=NULL) && (cur->next!=NULL)) return append_packet(cur->next, row, packet);
	packet_t *p=malloc(sizeof(packet_t));
	if (p==NULL) {
		fprintf(stderr, "append_packet: malloc failed\n");
		return NULL;
	}
	memset(p, 0, sizeof(packet_t));
	p->next=NULL;
	memcpy(p->packet, packet, 42);
	p->row=row;
	if (cur!=NULL)cur->next=p;
	return p;
}

void print_packet(const packet_t *a)
{
	int n;
	for (n=0; n<42; n++) {
		int c=a->packet[n]&0x7f;
		if (c<' ') c='.';
		if (c>0x7e) c='.';
		printf("%c", c);
	}
	printf(" ");
}

int compare_packets(const packet_t *a, const packet_t *b)
{
	if (a==b) return 0; //if the pointers are the same, it has to be equal
	if (a==NULL) return 1; //If one pointer is NULL, it's obviously different
	if (b==NULL) return 0; //However if this is a subset of a previous page, ignore it
	if (a->row!=b->row) return 1; //Row number different
	//Do not compare row 0
	if (a->row!=0) if (memcmp(a->packet, b->packet, 42)!=0) return 1;
	return compare_packets(a->next, b->next);
}


typedef struct page_s {
	struct page_s *next;
	struct page_s *prev;
	uint16_t pageno;
	uint16_t subpage;
	char time[9];
	packet_t *first_packet;
} page_t;

page_t *create_page(const uint16_t pageno, const uint16_t subpage, char *time)
{
	if (pageno>0x799) return NULL;
	page_t *p=malloc(sizeof(page_t));
	if (p==NULL) {
		fprintf(stderr, "create_page: malloc failed\n");
		return NULL;
	}
	memset(p, 0, sizeof(page_t));
	p->pageno=pageno;
	p->subpage=subpage;
	memcpy(p->time, time, 8);
	p->time[8]=0;
	return p;
}

void delete_page(page_t  *p) {
	if (p==NULL) return;
	delete_packet(p->first_packet);
	free(p);
}

void append_packet_to_page(page_t *page, const int row, const uint8_t *packet)
{
	if (page==NULL) return;
	if (packet==NULL) return;
	packet_t *p=append_packet(page->first_packet, row, packet);
	if (page->first_packet==NULL) page->first_packet=p;
	return;
}

int compare_page(const page_t *a, const page_t *b)
{
	if (a==NULL) return 1;
	if (b==NULL) return 1;
	if (a->pageno!=b->pageno) return 1;
	if (a->subpage!=b->subpage) return 1;
	return compare_packets(a->first_packet, b->first_packet);
}

page_t *add_page_if_not_equal(page_t *before, page_t *new)
{
	if (compare_page(before, new) == 0) { //If there is already an identical page, delete the new one
		delete_page(new);
		return NULL;
	}
	if (before==NULL) return new;
	if (before->next!=NULL) return add_page_if_not_equal(before->next, new);
	before->next=new;
	new->prev=before;
	return new;
}


void write_line_to_file(FILE *f, packet_t *p)
{
	if (f==NULL) return;
	if (p==NULL) return;
	fwrite(p->packet, 42, 1, f);
	return write_line_to_file(f, p->next);
}

void write_page(const char *path, const page_t *p)
{
	if (p==NULL) return;
	int magazine=p->pageno>>8;
	if (magazine==0) magazine=8;
	char fn[100];
	snprintf(fn, sizeof(fn), "%03x-%04x-%s", (p->pageno&0xff)|(magazine<<8), p->subpage, p->time);
	char ofn[strlen(fn)+strlen(path)+2];
	snprintf(ofn, sizeof(ofn), path, fn);
	printf("File: %s\n", ofn);
	FILE *f=fopen(ofn, "w");
	write_line_to_file(f, p->first_packet);
	fclose(f);
}


int main(int argc, char *argv[])
{
	if (argc!=2) {
		printf("Usage: %s <output-directory>\n", argv[0]);
		return 1;
	}
	char *output_directory=argv[1];

	page_t *all_pages[0x800];
	memset(all_pages, 0 ,sizeof(all_pages));

	//We have (up to) 8 multiplexed magazines, in order to handle this we have multiple pages
	page_t *pages[8]={NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

	uint8_t packet[42];
	while (fread(packet, 1,sizeof(packet), stdin)>0) {
		//Read the packet and get the magazine and row number
		int mpag=de_hamm(packet[1])<<4 | de_hamm(packet[0]);
		int magazine=mpag&0x7;
		int row=mpag>>3;
		if (row==0) { //If row==0 => new page
			int page=de_hamm(packet[3])<<4 | de_hamm(packet[2]);
			int sub=(de_hamm(packet[4])) | (de_hamm(packet[6])<<4) | (de_hamm(packet[6])<<8) | (de_hamm(packet[7])<<12);
			int subpage=sub&0x3f7f;
			char time[9];
			memset(time, 0, sizeof(time));
			int n;
			for (n=0; n<8; n++) {
				char c=packet[34+n]&0x7f;
				if (c<'0') c='-'; else
				if (c>'9') c='-';
				time[n]=c;
			}
			if (pages[magazine]!=NULL) {
				uint16_t pn=pages[magazine]->pageno;
				page_t *p=add_page_if_not_equal(all_pages[pn], pages[magazine]);
				if (all_pages[pn]==NULL) all_pages[pn]=p;
				if (p!=NULL) write_page(output_directory, p);
				pages[magazine]=NULL;
			}

			if (page!=0xff) {
				pages[magazine]=create_page(magazine<<8|page,subpage, time);
			}
		}
		append_packet_to_page(pages[magazine], row, packet);
	}
}
