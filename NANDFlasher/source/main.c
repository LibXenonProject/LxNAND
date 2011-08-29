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
     
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
     
#include <debug.h>
#include <xenos/xenos.h>
#include <input/input.h>
#include <console/console.h>
#include <time/time.h>
#include <usb/usbmain.h>
#include <ppc/register.h>
#include <xenon_nand/xenon_sfcx.h>
#include <xenon_nand/xenon_config.h>
#include <xenon_soc/xenon_secotp.h>
#include <xenon_soc/xenon_power.h>
#include <xenon_smc/xenon_smc.h>

#define version "0.1"


#define NAND_SIZE_16MB  0x1000000
#define NAND_SIZE_64MB  0x4000000
#define NAND_SIZE_256MB 0x10000000
#define NAND_SIZE_512MB 0x20000000

#define BASE_ADDRESS 0x80000000

#define MAIN_MENU 1
#define DUMP_SUBMENU 2

#define BB64MB_ONLY 1
#define FULL_DUMP 0

#define XELL_SIZE (256*1024)
#define XELL_FOOTER_OFFSET (256*1024-16)
#define XELL_FOOTER_LENGTH 16
#define XELL_FOOTER "xxxxxxxxxxxxxxxx"

#define XELL_OFFSET_COUNT         2
static const unsigned int xelloffsets[XELL_OFFSET_COUNT] = {0x95060, // FreeBOOT Single-NAND main xell-2f
                                                            0x100000}; // XeLL-Only Image
unsigned int flashconfig;
const unsigned char elfhdr[] = {0x7f, 'E', 'L', 'F'};

void waitforexit(void)
{
        printf("Press A to get back to main menu.\n");
        struct controller_data_s controller;
        while(1)
        { 		
         struct controller_data_s button;
          if (get_controller_data(&button, 0))
          {
           if((button.a)&&(!controller.a))
             prompt(MAIN_MENU);
       										  
	   controller=button;
          }
 	  usb_do_poll();
 	}
}


void prompt(int menu)
{
    	console_clrscr();
        
        switch (menu)
        {
            case MAIN_MENU:{            
                printf("\n  LibXenon NANDFlasher v%s  \n",version);
                printf("  **************************\n\n");
                printf("Flashconfig 0x%08X\n",flashconfig);
                printf("NAND-Size %i MB\n\n",sfc.size_mb);
                printf("Press A to save NAND to file on USB.\n");
                printf("Press X to write file from USB to NAND.\n");
                printf("Press B to shutdown the console.\n");
                printf("Press Y to update XeLL.\n");
                printf("Press GUIDE to return to XeLL.\n");
        
                struct controller_data_s controller;
                while(1)
                { 		
                struct controller_data_s button;
                
                if (get_controller_data(&button, 0))
                  {
                    if((button.a)&&(!controller.a))
                        prompt(DUMP_SUBMENU);

                    if((button.x)&&(!controller.x))
                        writeFileToFlash("uda:/updflash.bin");

                    if((button.b)&&(!controller.b))
                        xenon_smc_power_shutdown();
                               
                    if((button.y)&&(!controller.y))
                        updateXeLL("uda:/updxell.bin");
                                        
                    if((button.logo)&&(!controller.logo))
                        return 0;
                                
                    controller=button;
		   }
 		usb_do_poll();
                }
            }
            case DUMP_SUBMENU:{
                int nand_sz = sfc.size_bytes;
                
                if (nand_sz != NAND_SIZE_256MB && nand_sz != NAND_SIZE_512MB)
                    readNANDToFile("uda:/flashdmp.bin", FULL_DUMP);
                    
                printf("Press A to dump whole NAND.\n");
                printf("Press B to dump only flash-partition.\n");
                printf("Press X to go back to main menu.\n");
                
                struct controller_data_s controller;
                while(1)
                { 		
                struct controller_data_s button;
                
                if (get_controller_data(&button, 0))
                  {
                    if((button.a)&&(!controller.a))
                        readNANDToFile("uda:/flashdmp.bin", FULL_DUMP);

                    if((button.b)&&(!controller.b))
                        readNANDToFile("uda:/flashdmp.bin", BB64MB_ONLY);
                               
                    if((button.x)&&(!controller.x))
                        prompt(MAIN_MENU);
                                
                    controller=button;
		   }
 		usb_do_poll();
                }
            }
        }
}

/* Read the entire NAND to file */
void readNANDToFile(char *path, int bb_64mb_only) 
{   
            FILE *f;
	    int i;
            int nand_sz = sfc.size_bytes;
            unsigned char *buf = (unsigned char *) malloc(sfc.block_sz_phys);
            if (!buf)
            {
		printf("Memory error!\n");
		waitforexit();
            }
            
            console_clrscr();
            
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
              
              if (!SFCX_SUCCESS(status)){
                  printf(" ! Error occured while dumping the NAND! Status: %i\n",status);
                  waitforexit();
              }

              if(!fwrite(buf, 1, sfc.block_sz_phys, f)){   
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

int main(void)
{
    
        xenon_make_it_faster(XENON_SPEED_FULL);
	xenos_init(VIDEO_MODE_AUTO);
	console_init();

	usb_init();
	usb_do_poll();     
   
	printf("SFCX Init.\n");
        flashconfig = sfcx_readreg(SFCX_CONFIG);
	sfcx_init();
	if (sfc.initialized != SFCX_INITIALIZED)
        {
                printf(" ! Flashconfig: 0x%08X\n", flashconfig);
		printf(" ! sfcx initialization failure\n");
		printf(" ! nand related features will not be available\n");
		waitforexit();
        }

	printf("Xenon_config_init.\n");
	xenon_config_init();

        prompt(MAIN_MENU);
        
	return 0;
}

int updateXeLL(char *path)
{
    FILE *f;
    int i, j, k, status, startblock, current, offsetinblock, blockcnt, filelength;
    unsigned char *updxell, *user, *spare;
    
    /* Check if updxell.bin is present */
    f = fopen(path, "rb");
    if (!f){
	printf(" ! Can't find / open %s\n",path);
        waitforexit(); //Can't find/open updxell.bin from USB
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

/*
int sfcx_DMAread_nand(unsigned char *data, unsigned char *meta, int address, int raw)
{
	int status;
        
	sfcx_writereg(SFCX_STATUS, sfcx_readreg(SFCX_STATUS));
        sfcx_writereg(SFCX_CONFIG, (sfcx_readreg(SFCX_CONFIG)|CONFIG_DMA_LEN));

	// Set flash address (logical)
	//address &= 0x3fffe00; // Align to page
	sfcx_writereg(SFCX_ADDRESS, address);
        
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
			printf(" ! SFCX: Bad block found at %08X\n", sfcx_address_to_block(address));
		else if (status & STATUS_ECC_ER)
		//	printf(" ! SFCX: (Corrected) ECC error at address %08X: %08X\n", address, status);
			status = status;
		else if (!raw && (status & STATUS_ILL_LOG))
			printf(" ! SFCX: Illegal logical block at %08X (status: %08X)\n", sfcx_address_to_block(address), status);
		else
			printf(" ! SFCX: Unknown error at address %08X: %08X. Please worry.\n", address, status);
	}

	// Set internal page buffer pointer to 0
	sfcx_writereg(SFCX_ADDRESS, 0);

	int i;
	//int page_sz = raw ? sfc.page_sz_phys : sfc.page_sz;
	for (i = 0; i < sfc.block_sz; i += 4)
	{
                // Byteswap the data
		*(int*)(&data[i]) = __builtin_bswap32(read32(&data[i]));
	}
        for (i = 0; i < sfc.block_sz_phys - sfc.block_sz; i += 4) //  sfc.block_sz_phys - sfc.block_sz == META SIZE
	{
		// Byteswap the data
		*(int*)(&meta[i]) = __builtin_bswap32(read32(&meta[i]));
	}

	return status;
}

void DMAreadNANDToFile ()
{
            FILE *fMeta; FILE *fData;
            char *pathData = "uda:/data.bin";
            char *pathMeta = "uda:/meta.bin";
	    int i = 0;
            int nand_sz = sfc.size_bytes;
            int block_sz = sfc.block_sz;
            int meta_sz_total = sfc.block_sz_phys - sfc.block_sz;
            int rawblock_sz = sfc.block_sz_phys;
            const unsigned char data[block_sz];
            const unsigned char meta[meta_sz_total];
            
            console_clrscr();
            
            fMeta = fopen(pathMeta, "wb");
            if (!fMeta){
                printf(" ! %s failed to open", pathMeta);
                waitforexit();
            }
            fData = fopen(pathData, "wb");
            if (!fData){
                printf(" ! %s failed to open", pathData);
                waitforexit();
            }
            
            if (nand_sz == NAND_SIZE_256MB || nand_sz == NAND_SIZE_512MB){
                printf(" ! Detected a BIG BLOCK Console with INTEGRATED Memory Unit!\n");
                printf(" ! Will ONLY read 64 MB!\n");
                nand_sz = NAND_SIZE_64MB;
            }
            
            printf("Reading DATA to %s ... \n",pathData);
            printf("Reading META to %s ... \n",pathMeta);
            
            int status;
            
              status = sfcx_DMAread_nand(data, meta, i, 0);
              
              if (!SFCX_SUCCESS(status)){
                  printf(" ! Error occured while dumping the NAND! Status: %i\n",status);
                  waitforexit();
              }
              
            if(!fwrite(meta, 1, meta_sz_total, fMeta)){   
                 printf(" ! Error dumping NAND! Please check disk space!\n");
                 waitforexit();
              }
            fclose(fMeta);

            if(!fwrite(data, 1, block_sz, fData)){   
                 printf(" ! Error dumping NAND! Please check disk space!\n");
                 waitforexit();
              } 
            
	     printf("\n");
	     printf("Closing file, freeing memory...\n");

             fclose(fData);
//             free(data);
//             free(meta);
             
             
	     printf("Done!\n");
             waitforexit();
}
*/
