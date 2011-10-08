#include <xenos/xenos.h>
#include <input/input.h>
#include <xenon_nand/xenon_sfcx.h>
#include <xenon_soc/xenon_power.h>
#include <xenon_smc/xenon_smc.h>

#include "nandflasher.h"

extern BBM BBMfile;
extern BBM BBMnand;

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
                printf("Press B to analyze NAND.\n");
                printf("Press Y to update XeLL.\n");
                printf("Press START for DMA test.\n");
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
                        writeNand("uda:/updflash.bin");

                    if((button.b)&&(!controller.b))
                        prompt(ANALYZE_SUBMENU);
                               
                    if((button.y)&&(!controller.y))
                        updateXeLL("uda:/updxell.bin");
                    
                    if((button.start)&&(!controller.start))                    
                        dmatest();
                    
                    if((button.logo)&&(!controller.logo))
                        break;
                                
                    controller=button;
		   }
 		usb_do_poll();
                }
            }
            case DUMP_SUBMENU:{
                
                int nand_sz = sfc.size_bytes;
                
                if (nand_sz != NAND_SIZE_256MB && nand_sz != NAND_SIZE_512MB)
                    readNand("uda:/flashdmp.bin", FULL_DUMP);
                
                printf("\n         DUMP MENU          \n");
                printf("  **************************\n\n");
                printf("Press A to dump whole NAND.\n");
                printf("Press B to dump only flash-partition.\n");
                printf("Press BACK to go back to main menu.\n");
                
                struct controller_data_s controller;
                while(1)
                { 		
                struct controller_data_s button;
                
                if (get_controller_data(&button, 0))
                  {
                    if((button.a)&&(!controller.a))
                        readNand("uda:/flashdmp.bin", FULL_DUMP);

                    if((button.b)&&(!controller.b))
                        readNand("uda:/flashdmp.bin", BB64MB_ONLY);
                               
                    if((button.select)&&(!controller.select))
                        prompt(MAIN_MENU);
                                
                    controller=button;
		   }
 		usb_do_poll();
                }
            }
            case ANALYZE_SUBMENU:{
                
                printf("\n       ANALYZE MENU        \n");
                printf("  **************************\n\n");
                printf("Press A to analyze physical NAND.\n");
                printf("Press B to analyze file updflash.bin.\n");
                printf("Press BACK to go back to main menu.\n");
                
                struct controller_data_s controller;
                while(1)
                { 		
                struct controller_data_s button;
                
                if (get_controller_data(&button, 0))
                  {
                    if((button.a)&&(!controller.a))
                        prompt(ANALYZE_PHYS_SUBMENU);

                    if((button.b)&&(!controller.b))
                        prompt(ANALYZE_FILE_SUBMENU);
                               
                    if((button.select)&&(!controller.select))
                        prompt(MAIN_MENU);
                                
                    controller=button;
		   }
 		usb_do_poll();
                }
            }
            case ANALYZE_PHYS_SUBMENU:{
                
                int nand_sz = sfc.size_bytes;
                int blocks = sfc.size_blocks;
                if (nand_sz == NAND_SIZE_256MB || nand_sz == NAND_SIZE_512MB)
                    blocks = 0x200;
                
                printf("\n       ANALYZE NAND       \n");
                printf("  **************************\n\n");
                printf("Press A to scan for Bad Blocks.\n");
                printf("Press B to scan for Bad Blocks and EDC errors.\n");
                printf("Press BACK to go back to main menu.\n");
                
                struct controller_data_s controller;
                while(1)
                { 		
                struct controller_data_s button;
                
                if (get_controller_data(&button, 0))
                  {
                    if((button.a)&&(!controller.a)){
                        analyzeNand(0,blocks,0);
                        printReport(BBMnand);
                        waitforexit();
                    }

                    if((button.b)&&(!controller.b)){
                        analyzeNand(0,blocks,1);
                        printReport(BBMnand);
                        waitforexit();
                    }
                               
                    if((button.select)&&(!controller.select))
                        prompt(MAIN_MENU);
                                
                    controller=button;
		   }
 		usb_do_poll();
                }
            }
            case ANALYZE_FILE_SUBMENU:{
                
                int nand_sz = sfc.size_bytes;
                int blocks = sfc.size_blocks;
                if (nand_sz == NAND_SIZE_256MB || nand_sz == NAND_SIZE_512MB)
                    blocks = 0x200;
                
                printf("\n       ANALYZE FILE        \n");
                printf("  **************************\n\n");
                printf("Press A to scan for Bad Blocks.\n");
                printf("Press B to scan for Bad Blocks and EDC errors.\n");
                printf("Press BACK to go back to main menu.\n");
                
                struct controller_data_s controller;
                while(1)
                { 		
                struct controller_data_s button;
                
                if (get_controller_data(&button, 0))
                  {
                    if((button.a)&&(!controller.a)){
                        analyzeFile("uda:/updflash.bin",0,blocks, 0);
                        printReport(BBMfile);
                        waitforexit();
                    }

                    if((button.b)&&(!controller.b)){
                        analyzeFile("uda:/updflash.bin",0,blocks, 1);
                        printReport(BBMfile);
                        waitforexit();
                    }
                               
                    if((button.select)&&(!controller.select))
                        prompt(MAIN_MENU);
                                
                    controller=button;
		   }
 		usb_do_poll();
                }
            }
        }
        return 0;
}

int main(void)
{
	xenos_init(VIDEO_MODE_AUTO);
        console_set_colors(0xD8444E00,0xFF96A300);
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