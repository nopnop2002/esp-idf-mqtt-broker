/* Example test application for testable component.
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "unity.h"
#include "sdkconfig.h"


static void print_banner(const char* text)
{
    printf("\n### %s ####\n\n", text);
}

extern "C" void app_main(void)
{    
    printf("--- %s Build: %s @ %s ---", __func__ ,  __DATE__, __TIME__);
    
    print_banner("Running tests with [smoke] tag"); 
    UNITY_BEGIN();
    unity_run_tests_by_tag("[smoke]", false);
    UNITY_END();
        
    print_banner("Starting interactive test menu");
    /* This function will not return, and will be busy waiting for UART input.
     * Make sure that task watchdog is disabled if you use this function.
     */
    unity_run_menu();
}
