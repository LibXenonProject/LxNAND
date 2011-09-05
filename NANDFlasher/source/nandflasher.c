/* LxNANDTool, a multi-purpose NAND tool to modify, copy, and manipulate the Xbox 360 NAND.
 * LxNANDTool is currently only tested on 16MB chips but will assumedly work on all chips.
 * 
 * LxNANDTool can so far read and write the 360 NAND using static file names and paths.
 * The tool can also be controlled by the user with the controller, displaying simple on-
 * screen instructions to read, write, clear screen and shutdown th console. 
 *
 * More features will be added soon, hopefully DMA (Direct Memory Access) for major speed enhancements.
 *
 * Written in collaboration by Tuxuser and UNIX
 * http://LibXenon.org
 */
     
#include <debug.h>
#include <xetypes.h>
#include <xenon_nand/xenon_sfcx.h>

#include "nandflasher.h"

const unsigned char elfhdr[] = {0x7f, 'E', 'L', 'F'};

struct BadBlockManagment BBMfile;
struct BadBlockManagment BBMnand;

/* Read the entire NAND to file */
void readNANDToFile(char *path, int bb_64mb_only)
{
  FILE *f;
  int i;
  int nand_sz = sfc.size_bytes;
  unsigned char *buf = (unsigned char *) malloc(sfc.block_sz_phys);
  
  console_clrscr();
  
  if (!buf)
  {
     printf("Memory error!\n");
     waitforexit();
  }
            
  f = fopen(path, "wb");
  if (!f){
     printf(" ! %s failed to open\n", path);
     waitforexit();
  }
            
  if(bb_64mb_only){ 
     printf(" ! Detected a BIG BLOCK Console with INTEGRATED Memory Unit!\n");
     printf(" ! Will ONLY read 64 MB!\n");
     nand_sz = NAND_SIZE_64MB;
  }
            
  printf("Please wait while reading NAND to %s ... \n",path);
            
  int status;
            
  for (i = 0; i < nand_sz; i += sfc.block_sz) 
  {
     status = sfcx_read_block(buf, i, 1);
              
     if (!SFCX_SUCCESS(status))
       printf(" ! Error occured while dumping the NAND! Status: %04X\n",status);

     if(!fwrite(buf, 1, sfc.block_sz_phys, f))
     {   
       printf(" ! Error dumping NAND! Please check disk space!\n");
       waitforexit();                 
     }
  }
            
  printf("\n");
  printf("Closing file, freeing memory...\n");

  fclose(f);
  free(buf);
             
             
  printf("Reading NAND finished. Do whatever you like to do now !\n");
  waitforexit();
}

/* Read file from USB, to memory buffer, to NAND */
void writeFileToFlash(char* path)
{
  FILE *f;
  int i, reserved_blocks, nand_sz;
  unsigned char *buf;
  unsigned long fileLen;
        
  // If bigblock 256/512MB console, override NANDsize with 64MB
  nand_sz = sfc.size_bytes;
  if (nand_sz == NAND_SIZE_256MB || nand_sz == NAND_SIZE_256MB)
    nand_sz = NAND_SIZE_64MB; //Only 64MB dumps supported on Big Block consoles

  console_clrscr();
        
  //Open file
  f = fopen(path, "rb");

  if(f == NULL)
  {
     printf(" ! %s not found on device! Aborting write!\n", path);
     waitforexit();
  }
	
  printf("\nFile %s opened successfully!\n", path);
        
  //Get file length
  fseek(f, 0, SEEK_END);
  fileLen=ftell(f);
  fseek(f, 0, SEEK_SET);

  //Check for correct filesize
  if ((fileLen/sfc.block_sz_phys)*sfc.block_sz != nand_sz){
     printf(" ! Filesize of %s is unsupported!\n", path);
     waitforexit();
  }
        
  //Allocate memory to *buffer
  buf=(unsigned char *)malloc(fileLen);
  if (!buf)
  {
      printf("Memory error!\n");
      fclose(f);
      waitforexit();
  }

  //Read file contents into buffer
  printf("Please wait while reading file into buffer ... \n", path);
  fread(buf, 1, fileLen, f);
  fclose(f);
        
        
   printf("Deleting reserved bad block area !\n");
   reserved_blocks = sfc.size_blocks-sfc.size_usable_fs;
        
   for (i = reserved_blocks * sfc.block_sz; i < nand_sz; i += sfc.block_sz){
       sfcx_erase_block(i);
   }
        
   printf("Please wait while writing to NAND...\n");
   //Do what ever with buffer
   for (i = 0; i < sfc.size_bytes_phys; i += sfc.block_sz_phys) 
   {
	sfcx_erase_block(sfcx_rawaddress_to_block(i)*sfc.block_sz);
	sfcx_write_block(&buf[i], sfcx_rawaddress_to_block(i)*sfc.block_sz);
   }

   printf("\n");
   printf("Freeing memory buffer...\n");

    free(buf);

    printf("Flashing NAND finished. Power cycle the Xbox and boot into your fresh Image !\n");
    printf(" ! If you flashed a new SMC you have to replug the PSU !\n");
        
    waitforexit();
}

int updateXeLL(char *path)
{
    FILE *f;
    int i, j, k, status, startblock, current, offsetinblock, blockcnt, filelength;
    unsigned char *updxell, *user, *spare;
    
    console_clrscr();
    
    /* Check if updxell.bin is present */
    f = fopen(path, "rb");
    if (!f){
	printf(" ! Can't find / open %s\n",path);
        waitforexit();
    }
    
    if (sfc.initialized != SFCX_INITIALIZED){
        fclose(f);
        printf(" ! sfcx is not initialized! Unable to update XeLL in NAND!\n");
	waitforexit();
    }
   
    /* Check filesize of updxell.bin, only accept full 256kb binaries */
    fseek(f, 0, SEEK_END);
    filelength=ftell(f);
    fseek(f, 0, SEEK_SET);
    if (filelength != XELL_SIZE){
        fclose(f);
        printf(" ! %s does not have the correct size of 256kb. Aborting update!\n", path);
        waitforexit();
    }
    
    printf(" * Update-XeLL binary found @ %s ! Looking for XeLL binary in NAND now.\n", path);
    
    for (k = 0; k < XELL_OFFSET_COUNT; k++)
    {
      current = xelloffsets[k];
      offsetinblock = current % sfc.block_sz;
      startblock = current/sfc.block_sz;
      blockcnt = offsetinblock ? (XELL_SIZE/sfc.block_sz)+1 : (XELL_SIZE/sfc.block_sz);
      
    
      spare = (unsigned char*)malloc(blockcnt*sfc.pages_in_block*sfc.meta_sz);
      if(!spare){
        printf(" ! Error while memallocating filebuffer (spare)\n");
        waitforexit();
      }
      user = (unsigned char*)malloc(blockcnt*sfc.block_sz);
      if(!user){
        printf(" ! Error while memallocating filebuffer (user)\n");
        waitforexit();
      }
      j = 0;

      for (i = (startblock*sfc.pages_in_block); i< (startblock+blockcnt)*sfc.pages_in_block; i++)
      {
         sfcx_read_page(sfcx_page, (i*sfc.page_sz), 1);
	 //Split rawpage into user & spare
	 memcpy(&user[j*sfc.page_sz],&sfcx_page[0x0],sfc.page_sz);
	 memcpy(&spare[j*sfc.meta_sz],&sfcx_page[sfc.page_sz],sfc.meta_sz);
	 j++;
      }
      
        if (memcmp(&user[offsetinblock+(XELL_FOOTER_OFFSET)],XELL_FOOTER,XELL_FOOTER_LENGTH) == 0){
            printf(" * XeLL Binary found @ 0x%08X\n", (startblock*sfc.block_sz)+offsetinblock);
        
         updxell = (unsigned char*)malloc(XELL_SIZE);
         if(!updxell){
           printf(" ! Error while memallocating filebuffer (updxell)\n");
           waitforexit();
         }
        
         status = fread(updxell,1,XELL_SIZE,f);
         if (status != XELL_SIZE){
           fclose(f);
           printf(" ! Error reading file from %s\n", path);
           waitforexit();
         }
	 	 
	 if (!memcmp(updxell, elfhdr, 4)){
	   printf(" * really, we don't need an elf.\n");
	   waitforexit();
	 }

         if (memcmp(&updxell[XELL_FOOTER_OFFSET],XELL_FOOTER, XELL_FOOTER_LENGTH)){
	   printf(" ! XeLL does not seem to have matching footer, Aborting update!\n");
	   waitforexit();
	 }
         
         fclose(f);
         memcpy(&user[offsetinblock], updxell,XELL_SIZE); //Copy over updxell.bin
         printf(" * Writing to NAND!\n");
	 j = 0;
         for (i = startblock*sfc.pages_in_block; i < (startblock+blockcnt)*sfc.pages_in_block; i ++)
         {
	     if (!(i%sfc.pages_in_block))
		sfcx_erase_block(i*sfc.page_sz);

	     /* Copy user & spare data together in a single rawpage */
             memcpy(&sfcx_page[0x0],&user[j*sfc.page_sz],sfc.page_sz);
	     memcpy(&sfcx_page[sfc.page_sz],&spare[j*sfc.meta_sz],sfc.meta_sz);
	     j++;

	     if (!(sfcx_is_pageempty(sfcx_page))) // We dont need to write to erased pages
	     {
             memset(&sfcx_page[sfc.page_sz+0x0C],0x0, 4); //zero only EDC bytes
             sfcx_calcecc((unsigned int *)sfcx_page); 	  //recalc EDC bytes
             sfcx_write_page(sfcx_page, i*sfc.page_sz);
	     }
         }
         printf(" * XeLL flashed! Reboot the xbox to enjoy the new build\n");
	 for(;;);
	
      }
    }
    printf(" ! Couldn't locate XeLL binary in NAND. Aborting!\n");
    waitforexit();
}

int scanBadBlocksinFile(char *path)
{
    FILE *f;
    int found, i;
    unsigned long filelength, block, page;
    unsigned char* blockbuf;
    BBMfile.BadBlocksCount = 0;
    
    blockbuf = (unsigned char *)malloc(sfc.block_sz_phys);

    f = fopen(path, "rb");
    if (!f){
	printf(" ! Can't open %s\n",path);
        waitforexit();
    }
    fseek(f, 0, SEEK_END);
    filelength=ftell(f);
    fseek(f, 0, SEEK_SET);
    
    for (block = 0; block < filelength; block += sfc.block_sz_phys)
    {
        printf(".");
        found = 0;
        memset(blockbuf,0xFF,sfc.block_sz_phys);
        fseek(f, block, SEEK_SET);
        fread(blockbuf,1,sfc.block_sz_phys,f);
        
        for (page = 0; page < sfc.block_sz_phys; page += sfc.page_sz_phys)
        {
            if(!sfcx_is_pageempty(&blockbuf[page]) && !sfcx_is_pagevalid(&blockbuf[page]))
                found = 1;
        }
        
        if (found){
            BBMfile.BadBlocks[BBMfile.BadBlocksCount++] = sfcx_rawaddress_to_block(block);
        }
    }
    fclose(f);
    
    printf("Following Bad Blocks found: ");
    
    if(!BBMfile.BadBlocksCount)
        printf("none!\n");
    else 
    {
        for(i=0;i<BBMfile.BadBlocksCount; i++)
            printf("%04X ", BBMfile.BadBlocks[i]);
    }
    printf("\n");
    
    return 0;
}

int scanBadBlocksinNAND()
{
    int found, i;
    unsigned long nand_sz, block, page;
    unsigned char* blockbuf;
    BBMnand.BadBlocksCount = 0;
    
    blockbuf = (unsigned char *)malloc(sfc.block_sz_phys);
    
    nand_sz = sfc.size_bytes;
    if (nand_sz == NAND_SIZE_256MB || nand_sz == NAND_SIZE_256MB)
            nand_sz = NAND_SIZE_64MB; //Only 64MB dumps supported on Big Block consoles
    
    for (block=0; block < nand_sz; block += sfc.block_sz)
    {
        found = 0;
        sfcx_read_block(blockbuf,block,1);
        
        for (page=0; page < sfc.block_sz_phys; page += sfc.page_sz_phys)
        {
            if(!sfcx_is_pageempty(&blockbuf[page]) && !sfcx_is_pagevalid(&blockbuf[page]))
                found = 1;
        }
        
        if (found){
            BBMnand.BadBlocks[BBMnand.BadBlocksCount++] = sfcx_rawaddress_to_block(block);
        }
    }
    printf("\n\nFollowing Bad Blocks found: ");
    
    if(!BBMnand.BadBlocksCount)
        printf("none!\n");
    else 
    {
        for(i=0;i<BBMnand.BadBlocksCount; i++)
            printf("%04X ", BBMnand.BadBlocks[i]);
    }
    printf("\n");
    
    return 0;
}

int eraseNand(int startblock, int blocklength)
{
    int i, status, err;

    for (i = startblock; i < startblock + blocklength; i++)
    {
        status = sfcx_erase_block(i*sfc.block_sz);
        if(!SFCX_SUCCESS(status))
            printf(" ! Error while erasing block %04X\n",i);
    }
    return 0;
}