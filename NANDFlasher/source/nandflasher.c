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

BBM BBMfile;
BBM BBMnand;

/**
* Read the NAND into a file.
*
* @param path - The path to the target file e.g "uda:/flashdmp.bin"
* @param size the size of the packet buffer to allocate
* @return none
*/
void readNand(char *path, int bb_64mb_only)
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

/**
* Allocate memory for a packet buffer for a given netbuf.
*
* @param buf the netbuf for which to allocate a packet buffer
* @param size the size of the packet buffer to allocate
* @return pointer to the allocated memory
* NULL if no memory could be allocated
*/
void writeNand(char* path)
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
  if (fileLen == NAND_SIZE_16MB_RAW || fileLen == NAND_SIZE_64MB_RAW)
     printf("updflash.bin has a valid filesize.");
  else {
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
  printf("Please wait while reading file into buffer ... \n");
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

/**
* Allocate memory for a packet buffer for a given netbuf.
*
* @param buf the netbuf for which to allocate a packet buffer
* @param size the size of the packet buffer to allocate
* @return pointer to the allocated memory
* NULL if no memory could be allocated
*/
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

/**
* Allocate memory for a packet buffer for a given netbuf.
*
* @param buf the netbuf for which to allocate a packet buffer
* @param size the size of the packet buffer to allocate
* @return pointer to the allocated memory
* NULL if no memory could be allocated
*/
int analyzeFile(char *path,int firstblock,int lastblock,int mode)
{
    FILE *f;
    int foundBB, foundEDC, block, page;
    unsigned long filelength, offset;
    unsigned char* pagebuf;
    unsigned char EDC[3];
    BBMfile.BadBlocksCount = 0;
    
    pagebuf = (unsigned char *)malloc(sfc.page_sz_phys);

    console_clrscr();
    
    f = fopen(path, "rb");
    if (!f){
	printf(" ! Can't open %s\n",path);
        waitforexit();
    }
    fseek(f, 0, SEEK_END);
    filelength=ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("\nStarting Analyzing!\n\n");
    
    for (block = firstblock; block < lastblock; block++)
    {
        printf("\rCurrently @ block %04X",block);
        foundBB = 0;
        foundEDC= 0;
        
        for (page = 0; page < sfc.pages_in_block; page++)
        {
           offset = (block*sfc.block_sz_phys)+(page*sfc.page_sz_phys);
           memset(pagebuf,0xFF,sfc.page_sz_phys);
           fseek(f,offset,SEEK_SET);
           fread(pagebuf,1,sfc.page_sz_phys,f);
           
            if(!sfcx_is_pageempty(pagebuf) && (!sfcx_is_pagevalid(pagebuf)))
                foundBB = 1;
           
           if(mode == (1||2)){
               memset(EDC,0,0x3);
               memcpy(EDC,&pagebuf[sfc.page_sz_phys-0x3],0x3);
               sfcx_calcecc(pagebuf);
               
               if (memcmp(EDC,&pagebuf[sfc.page_sz_phys-0x3],0x3))
                   foundEDC = 1;
           }
        }
        
        if (foundBB){
            BBMfile.BadBlocks[BBMfile.BadBlocksCount++] = block;
            printf("\nBad Block found @ %04X\n",block);
        }

        if (foundEDC){
            BBMfile.EDCerrorBlocks[BBMfile.EDCerrorCount++] = block;
            printf("\nEDC Error found @ %04X\n",block);
        }
        
    }
    fclose(f);
    
    return 0;
}

/**
* Allocate memory for a packet buffer for a given netbuf.
*
* @param buf the netbuf for which to allocate a packet buffer
* @param size the size of the packet buffer to allocate
* @return pointer to the allocated memory
* NULL if no memory could be allocated
*/
int analyzeNand(int first,int last,int mode)
{
    int foundBB, foundEDC, block, page, status;
    unsigned long offset;
    unsigned char* pagebuf;
    unsigned char EDC[3];
    
    BBMnand.BadBlocksCount = 0;
    
    pagebuf = (unsigned char *)malloc(sfc.page_sz_phys);
    
    console_clrscr();
    
    printf("\nStarting Analyzation!\n\n");
    
    for (block=first; block < last; block++)
    {
        printf("\rCurrently @ block %04X",block);
        foundBB = 0;
        foundEDC= 0;
        for (page=0; page < sfc.pages_in_block; page++)
        {
            offset = (block*sfc.block_sz)+(page*sfc.page_sz);
            status = sfcx_read_page(pagebuf,offset,1);
            
            if(!sfcx_is_pageempty(pagebuf) && !sfcx_is_pagevalid(pagebuf))
                foundBB = 1;
            
            if(status & STATUS_BB_ER)
                foundBB = 1;
            
           if(mode){
               memset(EDC,0,0x3);
               memcpy(EDC,&pagebuf[sfc.page_sz_phys-0x3],0x3);
               sfcx_calcecc(pagebuf);
               
               if (memcmp(EDC,&pagebuf[sfc.page_sz_phys-0x3],0x3))
                   foundEDC = 1;
           }
        }
        if (foundBB){
            BBMnand.BadBlocks[BBMnand.BadBlocksCount++] = block;
            printf("\nBad Block found @ %04X\n",block);
        }

        if (foundEDC){
            BBMnand.EDCerrorBlocks[BBMnand.EDCerrorCount++] = block;
            printf("\nEDC Error found @ %04X\n",block);
        }

    }
    
    return 0;
}

void printReport(BBM bbm)
{
    int i;
    
    if (!bbm.BadBlocksCount && !bbm.EDCerrorCount){
        printf("\nNo errors found!\n");
    }
    
    if (bbm.BadBlocksCount){
      printf("\nFound %i bad block(s): ",bbm.BadBlocksCount);
        for(i = 0; i < bbm.BadBlocksCount;i ++)
            printf("0x%04X ",bbm.BadBlocks[i]);
        printf("\n\n");
    }
    
    if (bbm.EDCerrorCount){
      printf("Found %i block(s) with wrong EDC: ",bbm.EDCerrorCount);
         for(i = 0; i < bbm.EDCerrorCount;i ++)
            printf("0x%04X ",bbm.EDCerrorBlocks[i]);
        printf("\n\n");
        }
    
}

/**
* Erase NANDblocks.
*
* @param buf the netbuf for which to allocate a packet buffer
* @param size the size of the packet buffer to allocate
* @return pointer to the allocated memory
* NULL if no memory could be allocated
*/
int eraseNand(int first, int last)
{
    int i, status;

    for (i = first; i < first+(last-first); i++)
    {
        status = sfcx_erase_block(i*sfc.block_sz);
        if(!SFCX_SUCCESS(status))
            printf(" ! Error while erasing block %04X\n",i);
    }
    return 0;
}

int sfcx_DMAread_block(unsigned char *data, unsigned char *meta, int raw, unsigned long  rawlength, unsigned long length)
{
   int status;
   sfcx_writereg(SFCX_STATUS, sfcx_readreg(SFCX_STATUS));
   sfcx_writereg(SFCX_CONFIG, (sfcx_readreg(SFCX_CONFIG)|CONFIG_DMA_LEN));

   // Set flash address (logical)
   //address &= 0x3fffe00; // Align to page
   sfcx_writereg(SFCX_ADDRESS, 0);
   // Set the target address for data
   sfcx_writereg(SFCX_DPHYSADDR,data-BASE_ADDRESS);
   // Set the target address for meta
   sfcx_writereg(SFCX_MPHYSADDR,meta-BASE_ADDRESS);
   // Command the read
   // Either a logical read (0x200 bytes, no meta data)
   // or a Physical read (0x210 bytes with meta data)
   sfcx_writereg(SFCX_COMMAND, raw ? DMA_PHY_TO_RAM : DMA_LOG_TO_RAM);

   // Wait Busy
   while ((status = sfcx_readreg(SFCX_STATUS)) & STATUS_BUSY);
   if (!SFCX_SUCCESS(status))
   {
     if (status & STATUS_BB_ER)
       printf(" ! SFCX: Bad block found at %08X\n");
     else if (status & STATUS_ECC_ER)
       // printf(" ! SFCX: (Corrected) ECC error at address %08X: %08X\n", address, status);
     status = status;
     else if (!raw && (status & STATUS_ILL_LOG))
       printf(" ! SFCX: Illegal logical block at %08X (status: %08X)\n", status);
     else
       printf(" ! SFCX: Unknown error at address %08X: %08X. Please worry.\n", status);
   }

   // Set internal page buffer pointer to 0
   sfcx_writereg(SFCX_ADDRESS, 0);

   int i;
   //int page_sz = raw ? sfc.page_sz_phys : sfc.page_sz;
   for (i = 0; i < length; i += 4)
   {
     // Byteswap the data
     *(int*)(&data[i]) = __builtin_bswap32(read32(&data[i]));
   }
   for (i = 0; i < (rawlength - length); i += 4) // sfc.block_sz_phys - sfc.block_sz == META SIZE
   {
     // Byteswap the data
     *(int*)(&meta[i]) = __builtin_bswap32(read32(&meta[i]));
   }

   return status;
}

void dmatest()
{
    int i;
    unsigned long j,k;
   // unsigned char block_raw[sfc.block_sz_phys];
    //unsigned char edc[3];
    int size_blocks;
    unsigned long size_bytes, size_bytes_raw;
    FILE *f;
    char *path = "uda:/dmatest2.bin";
    
    f = fopen(path, "wb");
    if (!f){
	printf(" ! Can't open %s\n",path);
        waitforexit();
    }
    
    if (sfc.size_bytes == NAND_SIZE_256MB || sfc.size_bytes == NAND_SIZE_256MB){
        size_blocks = 0x200; //Only 64MB dumps supported on Big Block consoles
        size_bytes = 0x4000000;
        size_bytes_raw = 0x4200000;
    }
    
    unsigned char dma_user[size_bytes];
    unsigned char dma_spare[size_bytes_raw-size_bytes];
    unsigned char dma_merge[size_bytes_raw];
    
    printf("DMA Length: %10X\n",(sfcx_readreg(SFCX_CONFIG)|CONFIG_DMA_LEN));
    
        //sfcx_read_block(block_raw,i*sfc.block_sz,1);
        sfcx_DMAread_block(dma_user,dma_spare,1,size_bytes_raw,size_bytes);
        
        k = 0;
        for (j=0; j<size_bytes_raw; j += sfc.page_sz_phys)
        {
            memcpy(&dma_merge[j],            &dma_user[k*sfc.page_sz],sfc.page_sz);
            memcpy(&dma_merge[j+sfc.page_sz],&dma_spare[k*sfc.meta_sz],sfc.meta_sz);
            k++;
        }
        //printf("Comparing Block (bytes %02X %02X %02X) %04X: ",dma_merge[0],dma_merge[1],dma_merge[2], i);
        
//        if (!memcmp(&dma_merge[0],&block_raw[0],sfc.block_sz_phys))
//            printf("OK!\n");
//        else
//            printf("FAIL !\n"); 
        
/*        for (j=0; j<sfc.block_sz_phys; j += sfc.page_sz_phys)
        {
            memcpy(edc,&dma_merge[(j+sfc.page_sz_phys)-3],3);
           sfcx_calcecc(&dma_merge[j]);
            if (!memcmp(edc,&dma_merge[(j+sfc.page_sz_phys)-3],3))
                printf("EDC OK!\n");
            else
                printf("EDC FAIL!\n"); 
        } */

        if (!(fwrite(dma_merge,1,size_bytes_raw,f)));
                printf("fwrite fail!\n");
                
    fclose(f);
        
    printf("DONE!!!!\n");
}
