#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "fsdata.c"

#include "pico/cyw43_arch.h"
#include "pico/cyw43_driver.h"

#include "lwip/init.h"
//#include "lwip/apps/httpd.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/sockets.h"

#include "lib/dhcpserver/dhcpserver.h"
#include "lib/dnsserver/dnsserver.h"
#include "lib/httpserver/httpserver.h"



#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"


#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"

#include "task.h"


#define PIN_RPM_METER_R 21
#define PIN_RPM_METER_L 19

#define PIN_LOAD_MOTOR_A 6
#define PIN_LOAD_MOTOR_B 7

#define PIN_ANGLE_MOTOR_A 0
#define PIN_ANGLE_MOTOR_B 1

#define PIN_LAUNCH_MOTOR_L 8

#define PIN_LAUNCH_MOTOR_R 9

#define PIN_ROTARY_A 14
#define PIN_ROTARY_B 15

#define PIN_ANGLE_SWITCH 12
#define PIN_LOAD_SWITCH 10
#define PIN_UNLOAD_SWITCH 11

#define PIN_EN_LOAD 17
#define PIN_EN_H 16


#define PIN_H_ANG_SENS_SDA 2
#define PIN_H_ANG_SENS_SCL 3
#define H_ANG_SENS_ADDR 0x36
#define H_ANG_SENS_I2C i2c1


#define PIN_H_A 27
#define PIN_H_B 28


int currentTheta = 0;
int requestedTheta = 0;
int currentPhiTicks = 0;
int requestedPhiTicks = 0;
unsigned short PWMR = 3000;
unsigned short PWML = 3000;
unsigned int ballSpeed = 0;
unsigned int ballDT = 0;
bool phiReady = false;
unsigned int rotaryDT = 0;
unsigned int rotaryState = 0;

uint32_t LTRPMRIRQ  = 0;
unsigned int RPMR = 0;
unsigned int requestedRPMR = 0;

uint32_t LTRPMLIRQ  = 0;
unsigned int RPML = 0;
unsigned int requestedRPML = 0;


TaskHandle_t webuiSetupTask,initMechsTask,measureRPMTask,updMechsTask,loopHAngleTask,setVAngleTask,setLMotorSpeed,setRMotorSpeed,loadTask,unloadTask,shootTask;


void set_pwm_frequency(uint pin, uint freq) {
    uint slice_num = pwm_gpio_to_slice_num(pin);

    uint system_clock = clock_get_hz(clk_sys); 
    printf("pwm sys clk : %d", system_clock);


    float div = ((float)system_clock/(1<<16))/((float)freq);
    pwm_set_phase_correct(slice_num,false);

    pwm_set_clkdiv_mode(slice_num,PWM_DIV_FREE_RUNNING);

    pwm_set_clkdiv(slice_num, div);

    pwm_set_enabled(slice_num, true);
}


uint32_t rotaryGetValue() 
{
    uint32_t temp = rotaryDT;
    rotaryDT = 0;
    return temp;
}

static void calibrateAngling(__unused void *params)
{

    gpio_put(PIN_ANGLE_MOTOR_A,0);
    gpio_put(PIN_ANGLE_MOTOR_B,1);
    
    printf("calibrateAngling");
    
    while(gpio_get(PIN_ANGLE_SWITCH)){
        vTaskDelay(1);
        continue;
    }
    
    printf("calibrateAngling done");
    gpio_put(PIN_ANGLE_MOTOR_A,0);
    gpio_put(PIN_ANGLE_MOTOR_B,0);
    
    currentPhiTicks = 0;
    rotaryGetValue();

    vTaskDelete(NULL);
}


void movePhiTicks(int val){

    printf("moving phi ticks begin\n");
    //int val = *((int*)params);
    gpio_put(PIN_ANGLE_MOTOR_A,0);
    gpio_put(PIN_ANGLE_MOTOR_B,0);

    //Forward => +rotate
    int movedAmount = 0;
    rotaryGetValue();//Zero rotary
    if(val>0){
        gpio_put(PIN_ANGLE_MOTOR_A,1);
        gpio_put(PIN_ANGLE_MOTOR_B,0);
        while((movedAmount < val)/* && gpio_get(PIN_ANGLE_SWITCH)*/){
            movedAmount += rotaryGetValue();   
            printf("forward : %d \n",movedAmount);
        }
    }
    else //Backward => -rotate
    {
        gpio_put(PIN_ANGLE_MOTOR_A,0);
        gpio_put(PIN_ANGLE_MOTOR_B,1);
        while((movedAmount > val) && gpio_get(PIN_ANGLE_SWITCH)){
            movedAmount += rotaryGetValue(); 
            printf("backward : %d \n",movedAmount);
        }
    }
    currentPhiTicks += movedAmount;
    gpio_put(PIN_ANGLE_MOTOR_A,0);
    gpio_put(PIN_ANGLE_MOTOR_B,0);
    printf("moving phi ticks done\n");
    vTaskDelete(NULL);
}

void setPhiTicks(void* params)
{
    int dest = *(int*) params;
    movePhiTicks(dest-currentPhiTicks);
}

static void unload(__unused void *params)
{
    gpio_put(PIN_LOAD_MOTOR_A,0);
    gpio_put(PIN_LOAD_MOTOR_B,1);
    
    printf("unloading");
    
    for(int i = 0;i<2000;i++){
        if(!gpio_get(PIN_UNLOAD_SWITCH))
            break;
        vTaskDelay(1);
    }
    
    printf("unloading done");
    


    gpio_put(PIN_LOAD_MOTOR_A,1);//This is to prevent overshoot
    gpio_put(PIN_LOAD_MOTOR_B,0);
    vTaskDelay(50);
    
    gpio_put(PIN_LOAD_MOTOR_A,0);
    gpio_put(PIN_LOAD_MOTOR_B,0);
    
    vTaskDelete(NULL); 
}

static void load(__unused void *params)
{


    gpio_put(PIN_LOAD_MOTOR_A,1);
    gpio_put(PIN_LOAD_MOTOR_B,0);
    
    printf("loading");
    
    for(int i = 0;i<2000;i++){
        if(gpio_get(PIN_LOAD_SWITCH))
            break;
        vTaskDelay(1);
    }
    
    printf("loading done");
    


    gpio_put(PIN_LOAD_MOTOR_A,0);//This is to prevent overshoot
    gpio_put(PIN_LOAD_MOTOR_B,1);
    vTaskDelay(100);
    
    gpio_put(PIN_LOAD_MOTOR_A,0);
    gpio_put(PIN_LOAD_MOTOR_B,0);

    vTaskDelete(NULL);
}


static void h_ang_loop(__unused void *params)
{
    int ret = 0;
    uint8_t rxdata[2] = {0};
    uint8_t txdata[] = {0x0C};
  
    gpio_put(PIN_H_A,0);
    gpio_put(PIN_H_B,0);
    while(true){
       
     
        ret = i2c_write_blocking(H_ANG_SENS_I2C,H_ANG_SENS_ADDR,txdata,1,false);
        ret = i2c_read_blocking(H_ANG_SENS_I2C, H_ANG_SENS_ADDR, rxdata, 2, false);
        currentTheta = (rxdata[0] << 8) + rxdata[1] + 1; 
        currentTheta = currentTheta + 1;
        if(ret == PICO_ERROR_GENERIC)
        {
            currentTheta = 505;
        }
        vTaskDelay(100);
    }
    vTaskDelete(NULL);
}

static void shoot(__unused void *params)
{
    
    
    

    gpio_put(PIN_LOAD_MOTOR_A,1);
    gpio_put(PIN_LOAD_MOTOR_B,0);
    
    printf("loading");
    
    for(int i = 0;i<2000;i++){
        if(!gpio_get(PIN_LOAD_SWITCH))
            break;
        vTaskDelay(1);
    }
    
    printf("loading done");
    


    gpio_put(PIN_LOAD_MOTOR_A,0);//This is to prevent overshoot
    gpio_put(PIN_LOAD_MOTOR_B,1);
    vTaskDelay(100);
    




    gpio_put(PIN_LOAD_MOTOR_A,0);
    gpio_put(PIN_LOAD_MOTOR_B,0);
    vTaskDelay(50);





    gpio_put(PIN_LOAD_MOTOR_A,0);
    gpio_put(PIN_LOAD_MOTOR_B,1);
    
    printf("unloading");
    
    for(int i = 0;i<2000;i++){
        if(gpio_get(PIN_UNLOAD_SWITCH))
            break;
        vTaskDelay(1);
    }
    
    printf("unloading done");
    

    gpio_put(PIN_LOAD_MOTOR_A,1);//This is to prevent overshoot
    gpio_put(PIN_LOAD_MOTOR_B,0);
    vTaskDelay(50);


    gpio_put(PIN_LOAD_MOTOR_A,0);
    gpio_put(PIN_LOAD_MOTOR_B,0);
    vTaskDelete(NULL);
}

void rotaryUpdate(){
    bool rotaryAValue = gpio_get(PIN_ROTARY_A);
    bool rotaryBValue = gpio_get(PIN_ROTARY_B);
    //printf("%d \n",rotaryDT);
    switch (rotaryState)
    {
        case 0:
            if(!rotaryAValue)
                rotaryState = 1;
            else if(! rotaryBValue)
                rotaryState = 4;
        break;

        case 1:
            rotaryState = (!rotaryBValue)? 2:1;
        break;

        case 2:
            rotaryState = (rotaryAValue)? 3:2;
        break;

        case 3:
            if(rotaryAValue && rotaryBValue){
                rotaryState = 0;
                rotaryDT += 1;
            }
        break;

        case 4:
            rotaryState = (!rotaryAValue)? 5:4;
        break;

        case 5:
            rotaryState = (rotaryBValue)? 6:5;
        break;

        case 6:
            if(rotaryAValue &&  rotaryBValue){
                rotaryState = 0;
                rotaryDT -= 1;
            }
        break;
    
    default:
        rotaryState = 0;
        break;
    }
}

static void init_mechanics(__unused void *params)
{

    pwm_set_gpio_level (PIN_LAUNCH_MOTOR_L, PWML);
    pwm_set_gpio_level (PIN_LAUNCH_MOTOR_R, PWMR);
    pwm_set_gpio_level (PIN_EN_LOAD, 53000);
    gpio_put(PIN_ANGLE_MOTOR_A,0);
    gpio_put(PIN_ANGLE_MOTOR_B,0);
    //pwm_set_gpio_level (PIN_EN_ANGLE, 47000);

    gpio_put(PIN_LOAD_MOTOR_A,0);
    gpio_put(PIN_LOAD_MOTOR_B,0);

    //xTaskCreate(calibrateAngling,"hAngCal",1024,NULL,tskIDLE_PRIORITY+2, &calibrateHAngleTask);

    //Beep Here


    //xTaskCreate(calibrateAngling,"hAngCal",1024,NULL,tskIDLE_PRIORITY+2, &calibrateHAngleTask);

    set_pwm_frequency(PIN_LAUNCH_MOTOR_R,2400);
    vTaskDelay(500);
    set_pwm_frequency(PIN_LAUNCH_MOTOR_R,1600);
    vTaskDelay(500);
    set_pwm_frequency(PIN_LAUNCH_MOTOR_R,800);

    vTaskDelete(NULL); //Delete these and your life will be hell, nothing wont work especially wifi
}

static void measureRPM(__unused void *params)
{
    uint rpml_slice_num = pwm_gpio_to_slice_num(PIN_RPM_METER_L);
    uint rpmr_slice_num = pwm_gpio_to_slice_num(PIN_RPM_METER_R);


    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
    pwm_config_set_clkdiv(&cfg, 1.0f);
    pwm_init(rpml_slice_num, &cfg, false);
    pwm_init(rpmr_slice_num, &cfg, false);
    gpio_set_function(PIN_RPM_METER_L, GPIO_FUNC_PWM);
    gpio_set_function(PIN_RPM_METER_R, GPIO_FUNC_PWM);
    pwm_set_enabled(rpml_slice_num, true);
    pwm_set_enabled(rpmr_slice_num, true);
    float rpm;
    uint16_t pulses;

    while(true)
    {
        pwm_set_counter(rpml_slice_num, 0);
        pwm_set_counter(rpmr_slice_num, 0);
        vTaskDelay(5000);
        //Decide on a minimum rpm to find the minmum time to wait, cant use 5secs with pid

        pulses = pwm_get_counter(rpml_slice_num);
        rpm = (pulses * 60.0f) / 5.0f;
        RPML = rpm;

        pulses = pwm_get_counter(rpmr_slice_num);
        rpm = (pulses * 60.0f) / 5.0f;
        RPMR = rpm;
    }
}


static void update_values(__unused void *params)
{
    PWML = requestedRPML;//PID
    PWMR = requestedRPMR;//PID 
    pwm_set_gpio_level (PIN_LAUNCH_MOTOR_L, (PWML));
    printf("LM : %d\n",PWML);
    pwm_set_gpio_level (PIN_LAUNCH_MOTOR_R, (PWMR));
    printf("RM : %d\n",PWMR);
    //uint system_clock = clock_get_hz(clk_sys); 
    //printf("pwm sys clk : %d\n", system_clock);
    //currentTheta = requestedTheta;
    //xTaskCreate(setPhiTicks,"hAngMove",512,&requestedPhiTicks,tskIDLE_PRIORITY+2, &moveHAngleTask);

    vTaskDelete(updMechsTask);
}

static void http_req(http_request_t *req)
{
    if(strcmp(req->taget,"/") == 0)
    {
        const unsigned int chunkSize = 6400;
        unsigned int total = sizeof(data_ui_html);
        unsigned int sent = 0;
        unsigned int toSend = 0;
        while(sent<total)
        {
            toSend = total-sent; //min(total-sent,chunkSize);
            send(req->incoming_sock,data_ui_html+sent, toSend, 0);
            sent += toSend;
        }

        return;
    }
    else if(strcmp(req->taget,"/getStats")  == 0)
    {
            const char st[] = "HTTP/1.0 200 OK\r\nContent-type: application/json\r\n\r\n";
            send(req->incoming_sock,st ,sizeof(st)-1, 0);

            char sendBuff[HTTPSERVER_MAX_HTTP_LINE_LENGTH];
            int length = snprintf( NULL, 0, "{\"pwml\":%d,\"rpml\":%d,\"pwmr\":%d,\"rpmr\":%d,\"theta\":%d,\"phiTicks\":%d,\"ballSpeed\":%d,\"ballDT\":%d}",PWML,RPML,PWMR,RPMR,currentTheta,currentPhiTicks,ballSpeed,ballDT);
            snprintf( sendBuff, length + 1, "{\"pwml\":%d,\"rpml\":%d,\"pwmr\":%d,\"rpmr\":%d,\"theta\":%d,\"phiTicks\":%d,\"ballSpeed\":%d,\"ballDT\":%d}",PWML,RPML,PWMR,RPMR,currentTheta,currentPhiTicks,ballSpeed,ballDT);
            send(req->incoming_sock,sendBuff, length, 0);
            return;
    }
    else if(strcmp(req->taget,"/setParams")  == 0)
    {
            const char st[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\nok";
            send(req->incoming_sock,st ,sizeof(st)-1, 0);
            //rpml=3&rpmr=2&theta=1&phi=4
            sscanf(req->content,"rpml=%hu&rpmr=%hu&theta=%i&phi=%i",&requestedRPML,&requestedRPMR,&requestedTheta,&requestedPhiTicks);
            xTaskCreate(update_values,"mechsUpd",512,NULL,tskIDLE_PRIORITY+2, &updMechsTask);
            return;
    }
    else if(strcmp(req->taget,"/shoot")  == 0)
    {
            const char st[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\nok";
            send(req->incoming_sock,st ,sizeof(st)-1, 0);
            
            xTaskCreate(shoot,"shoot",512,NULL,tskIDLE_PRIORITY+2, &shootTask);
            return;
    }
    /*else if(strcmp(req->taget,"/hLeft")  == 0)
    {
            const char st[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\nok";
            send(req->incoming_sock,st ,sizeof(st)-1, 0);
            
            xTaskCreate(hLeft,"hLeft",512,NULL,tskIDLE_PRIORITY+2, &shootTask);
            return;
    }
    else if(strcmp(req->taget,"/hRight")  == 0)
    {
            const char st[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\nok";
            send(req->incoming_sock,st ,sizeof(st)-1, 0);
            
            xTaskCreate(hRight,"hRight",512,NULL,tskIDLE_PRIORITY+2, &shootTask);
            return;
    }*/
    const char st[] = "HTTP/1.0 404 Not Found\r\nContent-type: text/html\r\n\r\nNot Found!";
    send(req->incoming_sock,st ,sizeof(st)-1, 0);
    return;
}



static void setup_webui(__unused void *params)
{

    cyw43_arch_init();
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    cyw43_arch_enable_ap_mode("Tennis Robot", "deeznutz", CYW43_AUTH_WPA2_MIXED_PSK);

    ip4_addr_t addr = { .addr = 0x0104A8C0 }, mask = { .addr =  0x00FFFFFF};
    
    dhcp_server_t dhcp_server;

    dhcp_server_init(&dhcp_server, &addr,&mask);
    dns_server_t dns_server;
    dns_server_init(&dns_server, &addr);

    
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    vTaskDelete(NULL);
}

void gpio_callback(uint gpio, uint32_t events) {


    if(gpio == PIN_ROTARY_A || gpio == PIN_ROTARY_B)
    {
        rotaryUpdate();
    }

}
 http_server_t http_server;
int main()
{
    stdio_init_all();
    gpio_init(22);
    gpio_set_dir(22,GPIO_OUT);
    gpio_put(22,0);

    i2c_init(H_ANG_SENS_I2C, 100 * 1000);
    gpio_set_function(PIN_H_ANG_SENS_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_H_ANG_SENS_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_H_ANG_SENS_SDA);
    gpio_pull_up(PIN_H_ANG_SENS_SCL);
    
    //printf("wait\n");
    set_sys_clock_khz(240000, true);

    gpio_init(PIN_RPM_METER_R);
    gpio_set_dir(PIN_RPM_METER_R,GPIO_IN);
    gpio_pull_down(PIN_RPM_METER_R);

    gpio_init(PIN_RPM_METER_L);
    gpio_set_dir(PIN_RPM_METER_L,GPIO_IN);
    gpio_pull_down(PIN_RPM_METER_L);

    gpio_init(PIN_H_A);
    gpio_set_dir(PIN_H_A,GPIO_OUT);

    gpio_init(PIN_H_B);
    gpio_set_dir(PIN_H_B,GPIO_OUT);

    gpio_init(PIN_LOAD_MOTOR_A);
    gpio_set_dir(PIN_LOAD_MOTOR_A,GPIO_OUT);

    gpio_init(PIN_LOAD_MOTOR_B);
    gpio_set_dir(PIN_LOAD_MOTOR_B,GPIO_OUT);

    gpio_init(PIN_ANGLE_MOTOR_A);
    gpio_set_dir(PIN_ANGLE_MOTOR_A,GPIO_OUT);

    gpio_init(PIN_ANGLE_MOTOR_B);
    gpio_set_dir(PIN_ANGLE_MOTOR_B,GPIO_OUT);

    gpio_init(PIN_LAUNCH_MOTOR_L);
    gpio_set_dir(PIN_LAUNCH_MOTOR_L,GPIO_OUT);
    gpio_set_function(PIN_LAUNCH_MOTOR_L,GPIO_FUNC_PWM);
    set_pwm_frequency(PIN_LAUNCH_MOTOR_L,800);//maybe trye 30K someday

    gpio_init(PIN_LAUNCH_MOTOR_R);
    gpio_set_dir(PIN_LAUNCH_MOTOR_R,GPIO_OUT);
    gpio_set_function(PIN_LAUNCH_MOTOR_R,GPIO_FUNC_PWM);
    set_pwm_frequency(PIN_LAUNCH_MOTOR_R,800);

    //pwm_set_enabled (5, true);// Slice 5 is enable, 5A and 5B are the launch motors

    gpio_init(PIN_EN_LOAD);
    gpio_set_dir(PIN_EN_LOAD,GPIO_OUT);
    gpio_set_function(PIN_EN_LOAD,GPIO_FUNC_PWM);
    set_pwm_frequency(PIN_EN_LOAD,800);



    gpio_init(PIN_ROTARY_A);
    gpio_set_dir(PIN_ROTARY_A,GPIO_IN); //We might wanna pull up these, gotta check

    gpio_init(PIN_ROTARY_B);
    gpio_set_dir(PIN_ROTARY_B,GPIO_IN);//We might wanna pull up these, gotta check

    gpio_init(PIN_ANGLE_SWITCH);
    gpio_set_dir(PIN_ANGLE_SWITCH,GPIO_IN);
    gpio_pull_up(PIN_ANGLE_SWITCH);

    gpio_init(PIN_LOAD_SWITCH);
    gpio_set_dir(PIN_LOAD_SWITCH,GPIO_IN);
    gpio_pull_up(PIN_LOAD_SWITCH);

    gpio_init(PIN_UNLOAD_SWITCH);
    gpio_set_dir(PIN_UNLOAD_SWITCH,GPIO_IN);
    gpio_pull_up(PIN_UNLOAD_SWITCH);


    gpio_set_irq_enabled_with_callback(PIN_ROTARY_A,  GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true,&gpio_callback);
    gpio_set_irq_enabled(PIN_ROTARY_B,  GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    


    gpio_put(PIN_LOAD_MOTOR_A,0);
    gpio_put(PIN_LOAD_MOTOR_B,0);
    
    //xTaskCreate(init_mechanics,"mechsInit",512,NULL,tskIDLE_PRIORITY+2, &initMechsTask);
    //xTaskCreate(measureRPM,"measRPM",512,NULL,tskIDLE_PRIORITY+2, &measureRPMTask);
    xTaskCreate(h_ang_loop,"hAngLoop",1024,NULL,tskIDLE_PRIORITY+1, &loopHAngleTask);


    xTaskCreate(setup_webui,"webuiSetup",1024,NULL,tskIDLE_PRIORITY+5, &webuiSetupTask);
    //For some reasons, Creating a task here (after webui task) would result in bad things, including the whole wifi/ap/webserver/dhcp not working.
   
    http_init(&http_server,http_req, 80);


    vTaskStartScheduler();

    return 0;
}
