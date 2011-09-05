#include <xenos/xenos.h>
#include <input/input.h>
#include <xenon_nand/xenon_sfcx.h>
#include <xenon_soc/xenon_power.h>
#include <xenon_smc/xenon_smc.h>

#include "nandflasher.h"

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