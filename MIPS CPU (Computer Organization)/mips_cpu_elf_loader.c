//==========================================================
// MIPS CPU binary executable file loader
//
// Main Function:
// 1. Loads binary excutable file into distributed memory
// 2. Waits MIPS CPU for finishing program execution
//
// Author:
// Yisong Chang (changyisong@ict.ac.cn)
//
// Revision History:
// 14/06/2016	v0.0.1	Add cycle counte support
//==========================================================
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <unistd.h>  
#include <sys/mman.h>  
#include <sys/types.h>  
#include <sys/stat.h>  
#include <fcntl.h>
#include <elf.h>

#include <assert.h>
#include <sys/time.h>

#define MIPS_CPU_REG_TOTAL_SIZE		(1 << 14)
#define MIPS_CPU_REG_BASE_ADDR		0x40000000

#define MIPS_CPU_MEM_SIZE		(1 << 10)
#define MIPS_CPU_FINISH_DW_OFFSET	0x00000003
#define MIPS_CPU_RESET_REG_OFFSET	0x00002000
#define MIPS_CPU_CYCLE	0x00002004
#define MIPS_CPU_INST	0x00002008
#define MIPS_CPU_BR		0x0000200C
#define MIPS_CPU_LD		0x00002010
#define MIPS_CPU_ST		0x00002014
#define MIPS_CPU_USER1	0x00002018
#define MIPS_CPU_USER2	0x0000201C
#define MIPS_CPU_USER3	0x00002020

#define MIPS_CPU_RES_DW_OFFSET		0x000000A8

void *map_base;
volatile uint32_t *map_base_word;
int	fd;

#define mips_addr(p) (map_base + (uintptr_t)(p))

void loader(char *file) {
	FILE *fp = fopen(file, "rb");
	assert(fp);
	Elf32_Ehdr *elf;
	Elf32_Phdr *ph = NULL;
	int i;
	uint8_t buf[4096];

	// the program header should be located within the first
	// 4096 byte of the ELF file
	fread(buf, 4096, 1, fp);
	elf = (void *)buf;

	// TODO: fix the magic number with the correct one
	const uint32_t elf_magic = 0x464c457f;
	uint32_t *p_magic = (void *)buf;
	// check the magic number
	assert(*p_magic == elf_magic);

	// our MIPS CPU can only reset with PC = 0
	assert(elf->e_entry == 0);

	for(i = 0, ph = (void *)buf + elf->e_phoff; i < elf->e_phnum; i ++) {
		// scan the program header table, load each segment into memory
		if(ph[i].p_type == PT_LOAD) {
			uint32_t addr = ph[i].p_vaddr;

			if(addr >= MIPS_CPU_REG_TOTAL_SIZE) {
				// Ignore segments with address out of ideal memory
				// All segments we need to load can fit in the ideal memory
				continue;
			}

			// TODO: read the content of the segment from the ELF file
			// to the memory region [VirtAddr, VirtAddr + FileSiz)
			// Use file operations
			// Use `mips_addr(addr)` to refer to address in mips CPU
			memcpy(mips_addr(ph[i].p_vaddr),buf+ph[i].p_offset,ph[i].p_filesz);//dest,source,size

			// TODO: zero the memory region
			// [VirtAddr + FileSiz, VirtAddr + MemSiz)
			memset(mips_addr(ph[i].p_vaddr+ph[i].p_filesz),0,ph[i].p_memsz-ph[i].p_filesz);
		}
	}

	fclose(fp);
}

void init_map() {
	fd = open("/dev/mem", O_RDWR|O_SYNC);  
	if (fd == -1)  {  
		perror("init_map open failed:");
		exit(1);
	} 

	//physical mapping to virtual memory 
	map_base = mmap(NULL, MIPS_CPU_REG_TOTAL_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, MIPS_CPU_REG_BASE_ADDR);
	
	if (map_base == NULL) {  
		perror("init_map mmap failed:");
		close(fd);
		exit(1);
	}  

	map_base_word = (uint32_t *)map_base;
}

void resetn(int val) {
	*(map_base_word + (MIPS_CPU_RESET_REG_OFFSET >> 2)) = val;
}

int wait_for_finish() {
	int ret;
	while((ret = *(map_base_word + MIPS_CPU_FINISH_DW_OFFSET)) == 0xFFFFFFFF);
	return ret;
}

void memdump() {
	int i;

	printf("Memory dump:\n");
	for(i = 0; i < MIPS_CPU_MEM_SIZE / sizeof(int); i++) {
		if(i % 4 == 0) {
			printf("0x%04x:", i << 2);
		}

		printf(" 0x%08x", map_base_word[i]);
		
		if(i % 4 == 3) {
			printf("\n");
		}
	}

	printf("\n");
}

void finish_map() {
	munmap(map_base, MIPS_CPU_REG_TOTAL_SIZE);
	close(fd);
}

int main(int argc, char *argv[]) {  
	int cycle_cnt,inst_cnt,br_cnt,ld_cnt,st_cnt,user1_cnt,user2_cnt,user3_cnt;
	float CPI;
	/* mapping the MIPS distributed memory into the address space of this program */
	init_map();

	/* reset MISP CPU */
	resetn(0);

	/* load MIPS binary executable file to distributed memory */
	loader(argv[1]);

	// memdump();

	/* finish reset MIPS CPU */
	resetn(1);

	/* wait for MIPS CPU finish  */
	printf("Running %s...", argv[1]);
	fflush(stdout);

	int ret = wait_for_finish();
	printf("\t\t%s\n", (ret == 0 ? "pass" : "fail!!!!!!!"));
	if (!ret)
	{
		cycle_cnt=*(map_base_word + (MIPS_CPU_CYCLE >> 2));
		inst_cnt=*(map_base_word + (MIPS_CPU_INST >> 2));
		br_cnt=*(map_base_word + (MIPS_CPU_BR >> 2));
		ld_cnt=*(map_base_word + (MIPS_CPU_LD >> 2));
		st_cnt=*(map_base_word + (MIPS_CPU_ST >> 2));
		user1_cnt=*(map_base_word + (MIPS_CPU_USER1 >> 2));
		user2_cnt=*(map_base_word + (MIPS_CPU_USER2 >> 2));
		user3_cnt=*(map_base_word + (MIPS_CPU_USER3 >> 2));
		CPI=(float)cycle_cnt/(inst_cnt+0.0);
		printf("\t\tthe cycle_cnt is:%d\n",cycle_cnt);
		printf("\t\tthe inst_cnt is:%d\n",inst_cnt);
		printf("\t\tthe br_cnt is:%d\n",br_cnt);
		printf("\t\tthe ld_cnt is:%d\n",ld_cnt);
		printf("\t\tthe st_cnt is:%d\n",st_cnt);
		printf("\t\tthe user1_cnt is:%d\n",user1_cnt);
		printf("\t\tthe user2_cnt is:%d\n",user2_cnt);
		printf("\t\tthe user3_cnt is:%d\n",user3_cnt);
		printf("the CPI is:%f\n",CPI);
	}

	/* reset MISP CPU */
	resetn(0);

	/* dump all distributed memory */
	//memdump();

	finish_map();

	return 0; 
} 
