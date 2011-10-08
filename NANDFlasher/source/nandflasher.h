#ifndef NANDFLASHER_H
#define	NANDFLASHER_H

#ifdef	__cplusplus
extern "C" {
#endif

#define version "0.1"

unsigned int flashconfig;

#define NAND_SIZE_16MB           0x1000000
#define     NAND_SIZE_16MB_RAW   0x1080000
#define NAND_SIZE_64MB           0x4000000
#define     NAND_SIZE_64MB_RAW   0x4200000
#define NAND_SIZE_128MB          0x8000000
#define NAND_SIZE_256MB          0x10000000
#define NAND_SIZE_512MB          0x20000000

#define BASE_ADDRESS 0x80000000

#define MAIN_MENU 1
#define DUMP_SUBMENU 2
#define ANALYZE_SUBMENU 3
#define ANALYZE_PHYS_SUBMENU 4
#define ANALYZE_FILE_SUBMENU 5

#define BB64MB_ONLY 1
#define FULL_DUMP 0

#define XELL_SIZE (256*1024)
#define XELL_FOOTER_OFFSET (256*1024-16)
#define XELL_FOOTER_LENGTH 16
#define XELL_FOOTER "xxxxxxxxxxxxxxxx"

#define XELL_OFFSET_COUNT         2
    
static const unsigned int xelloffsets[XELL_OFFSET_COUNT] = {0x95060, // FreeBOOT Single-NAND main xell-2f
                                                            0x100000}; // XeLL-Only Image

typedef struct _BBM{
  int BadBlocksCount;
  int BadBlocks[0x20];
  int EDCerrorCount;
  int EDCerrorBlocks[0x400];
}BBM,*PBBM;

void waitforexit(void);
void prompt(int menu);
int main(void);
void readNand(char *path, int bb_64mb_only);
void writeNand(char* path);
int updateXeLL(char *path);
int analyzeFile(char *path,int firstblock,int lastblock,int edc);
int analyzeNand(int first,int last,int edc);



#ifdef	__cplusplus
}
#endif

#endif	/* NANDFLASHER_H */

