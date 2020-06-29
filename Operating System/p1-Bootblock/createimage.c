#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEG_SIZE 512
#define MAXNUM 10

FILE *fp1;
FILE *fp2;
FILE *fp3;
Elf32_Ehdr bb_ehdr;
Elf32_Ehdr kn_ehdr;
int count_of_bb;

void print_ident(unsigned char *ch)
{  
    int i;  
    for (i = 0; i < 16; i++)
	{  
        printf("%02X ", ch[i]);  
    }  
    printf("\n");
}  

void read_ehdr(Elf32_Ehdr *ehdr, FILE* fp)
{  
    fseek(fp, 0, SEEK_SET);  
    fread(ehdr, sizeof(Elf32_Ehdr), 1, fp);  
}

void print_ehdr(Elf32_Ehdr *ehdr)
{
	printf("ELF Header\n");
	printf("e_ident: "); 
	print_ident(ehdr->e_ident);  
	printf("e_type: %02X\n", ehdr->e_type);  
	printf("e_machine: %02X\n", ehdr->e_machine);  
	printf("e_version: %04X\n", ehdr->e_version);  
	printf("e_entry: %04X\n", ehdr->e_entry);  
	printf("e_phoff: %04X\n", ehdr->e_phoff);  
	printf("e_shoff: %04X\n", ehdr->e_shoff);  
	printf("e_flags: %04X\n", ehdr->e_flags);  
	printf("e_ehsize: %02X\n", ehdr->e_ehsize);  	
	printf("e_phentsize: %02X\n", ehdr->e_phentsize);  
	printf("e_phnum: %02X\n", ehdr->e_phnum);  
    	printf("e_shentsize: %02X\n", ehdr->e_shentsize);  
    	printf("e_shnum: %02X\n", ehdr->e_shnum);  
    	printf("e_shstrndx: %02X\n", ehdr->e_shstrndx);  
}

void write_into_image(FILE *source_file, FILE *image_file, int source)
{	
	uint8_t seg_buf[SEG_SIZE];
	uint8_t elf_buf[SEG_SIZE*MAXNUM];
	Elf32_Phdr *ph = NULL;
	Elf32_Ehdr *eh;
	fseek(source_file, 0, SEEK_SET); 
	fread(elf_buf, SEG_SIZE*MAXNUM, 1, source_file);
	eh = (void *)elf_buf;
	if (source == 1) printf("\nBootblock Information:\n");
	if (source == 2) printf("\nKernel Information:\n");
	int i, count=0, still_write=0;
	for(i = 0, ph = (void *)elf_buf + eh->e_phoff; i< eh->e_phnum; i++)
	{
		if(ph[i].p_type == PT_LOAD)
		{	
			int seg_num = 0;
			if(ph[i].p_filesz > SEG_SIZE) still_write = ph[i].p_filesz;
			printf("p_offset[%d] = %d\n", count, ph[i].p_offset);
			printf("p_size[%d] = %d\n", count, ph[i].p_filesz);
			memset(seg_buf, 0, SEG_SIZE);
			if(still_write == 0)
			{
				count++;
				fseek(source_file, ph[i].p_offset, SEEK_SET);
				fread(seg_buf, 1, ph[i].p_filesz, source_file);
				fwrite(seg_buf, 1, SEG_SIZE, image_file);
			}
			else
			{
				while(still_write > 0)
				{
					memset(seg_buf, 0, SEG_SIZE);
					count++;
					fseek(source_file, ph[i].p_offset + seg_num * SEG_SIZE, SEEK_SET);
					seg_num++;
					fread(seg_buf, 1, (still_write > SEG_SIZE) ? SEG_SIZE : still_write, source_file);
					fwrite(seg_buf, 1, SEG_SIZE, image_file);
					if(still_write > SEG_SIZE) still_write -= SEG_SIZE;
					else still_write = 0;
				}
			}
		}
	}
	printf("Sectors Number = %d\n", count);
	if(source == 1) count_of_bb = count;
	count *= SEG_SIZE;
	if(source == 1)
	{
		fseek(image_file, 0, SEEK_SET);
		fwrite(&count, 1, sizeof(int), image_file);
		count += 2692743168;
		fwrite(&count, 1, sizeof(int), image_file);
		count -= 2692743168; 
		fseek(image_file,count,SEEK_SET);
	}
	if(source == 2)
	{
		fseek(image_file, 2*sizeof(int), SEEK_SET);
		fwrite(&(eh->e_entry), 1, sizeof(int), image_file);
		fwrite(&count, 1, sizeof(int), image_file);
		count += count_of_bb * SEG_SIZE;
		fseek(image_file,count,SEEK_SET);
	}
}

int main(int argc, char *argv[])
{	
	char *name1 = "./bootblock";
	char *name2 = "./kernel";
	char *name3 = "./image";
	
	fp1 = fopen(name1,"rb");
	fp2 = fopen(name2,"rb");
	fp3 = fopen(name3,"wb");

	read_ehdr(&bb_ehdr, fp1);  
	print_ehdr(&bb_ehdr);
	read_ehdr(&kn_ehdr, fp2);  
	print_ehdr(&kn_ehdr);
	
	write_into_image(fp1,fp3,1);
	write_into_image(fp2,fp3,2);

	fclose(fp1);
	fclose(fp2);
	fclose(fp3);

	return 0;
}
