import network
from machine import Pin, Timer, PWM
import _thread
import asyncio
import utime
import os
from servo import Servo
import json
import machine
import socket
import time

machine.freq(240000000)

##Defines-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
pinRPMMeterR = Pin(14, Pin.IN, Pin.PULL_UP)
pinRPMMeterL = Pin(15, Pin.IN, Pin.PULL_UP)

pinLoadMotorA = Pin(0, mode=Pin.OUT)
pinLoadMotorB = Pin(1, mode=Pin.OUT)

pinAngleMotorA = Pin(2, mode=Pin.OUT)
pinAngleMotorB = Pin(3, mode=Pin.OUT)

#pinServo = Pin(6, mode=Pin.OUT)
#pwmServo = PWM(pinServo)
#pwmLaunchMotorL.freq (50)
servo = Servo(pin=16)

pinLaunchMotorL = Pin(10, mode=Pin.OUT)
pwmLaunchMotorL = PWM(pinLaunchMotorL)
pwmLaunchMotorL.freq (100)

pinLaunchMotorR = Pin(11, mode=Pin.OUT)
pwmLaunchMotorR = PWM(pinLaunchMotorR)
pwmLaunchMotorR.freq (100)



pinAngleSwitch = Pin(5, Pin.IN,Pin.PULL_UP)

pinLoadSwitchLoad = Pin(6, Pin.IN, Pin.PULL_UP)

pinLoadSwitchUnload = Pin(7, Pin.IN, Pin.PULL_UP)

pinRotaryA = Pin(12, Pin.IN, Pin.PULL_UP)
pinRotaryB = Pin(13, Pin.IN, Pin.PULL_UP)

#pinSpeedMeterA = Pin(17, mode=Pin.IN)
#pinSpeedMeterB = Pin(16, mode=Pin.IN)

pinENLoadMotor = Pin(9, mode=Pin.OUT)
pwmLoadMotor = PWM(pinENLoadMotor)
pwmLoadMotor.freq (100)


##Variables-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
currentTheta = 0
currentPhiTicks = 0
PWMR = 0
PWML = 0
ballSpeed = 0
ballDT = 0
phiReady = False
rotaryDT = 0

def rotaryGetValue():
    global rotaryDT
    temp = rotaryDT
    rotaryDT = 0
    return temp

##Movement-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
async def calibrateAngling():
    global currentPhiTicks,rotaryDT
    pinAngleMotorA.low()
    pinAngleMotorB.high()
    
    print("calibrateAngling")
    
    while pinAngleSwitch.value():
        continue
    
    print("calibrateAngling done")
    pinAngleMotorA.low()
    pinAngleMotorB.low()
    
    currentPhiTicks = 0
    rotaryDT = 0
    
def movePhiTicks(val):
    global currentPhiTicks,phiReady,rotaryDT
    phiReady = False
    pinAngleMotorA.low()
    pinAngleMotorB.low()
    print("val",val)
    movedAmount = 0
    if(val>0):
        while(movedAmount < val):
            pinAngleMotorA.high()
            pinAngleMotorB.low()
            movedAmount += rotaryGetValue()
            print(movedAmount)
            utime.sleep_ms(1)
            
    else:
        while(movedAmount > val):
            pinAngleMotorA.low()
            pinAngleMotorB.high()
            movedAmount += -rotaryGetValue()
            print(movedAmount)
            utime.sleep_ms(1)

    currentPhiTicks += movedAmount
    pinAngleMotorA.low()
    pinAngleMotorB.low()
    phiReady = True
    return
async def setPhiTicks(ticks): #cmloc : position in CM
    movePhiTicks(ticks-currentPhiTicks)
    return


async def setTheta(theta):
    pass

    
async def unload():
    pinLoadMotorA.low()
    pinLoadMotorB.high()
    
    print("unloading")
    
    for i in range(2000):
        if not pinLoadSwitchUnload.value():
            break
        utime.sleep_ms(1)
    
    print("unloading done")
    
    pinLoadMotorA.high()## This is to prevent overshoot
    pinLoadMotorB.low()
    utime.sleep_ms(50)
    
    pinLoadMotorA.low()
    pinLoadMotorB.low()
    
    return
    
async def load():
    pinLoadMotorA.high()
    pinLoadMotorB.low()
    print("loading")
    
    
    for i in range(2000):
        if  pinLoadSwitchLoad.value():
            break
        utime.sleep_ms(1)
    
    print("loading done")
    
    pinLoadMotorA.low()## This is to prevent overshoot
    pinLoadMotorB.high()
    utime.sleep_ms(100)
    
    pinLoadMotorA.low()
    pinLoadMotorB.low()
    
    return
    
async def initMotors():
    pwmLaunchMotorR.duty_u16(2500) ## 10%
    pwmLaunchMotorL.duty_u16(2500) ## 10%
    
    pwmLoadMotor.duty_u16(25000)
    
    pinLoadMotorA.low()
    pinLoadMotorB.low()
    
    pinAngleMotorA.low()
    pinAngleMotorB.low()
    
    #servo.goto(60)
    
    asyncio.create_task(calibrateAngling())
    
    asyncio.create_task(unload())
    

    
    
    
    

##Speed and RPM Sensors-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
LTRPMRIRQ = time.ticks_us()
RPMRIRQ = 0

LTRPMLIRQ = time.ticks_us()
RPMLIRQ = 0


def rightRPMMeterIRQ(pin):
    global RPMRIRQ,LTRPMRIRQ
    curr = time.ticks_us()
    dt = time.ticks_diff(curr,LTRPMRIRQ)
    LTRPMRIRQ = curr
    if dt < 100:
        return
    RPMRIRQ = 60/(4*dt/1000000)
    LTRPMRIRQ = curr
    
def leftRPMMeterIRQ(pin):
    global RPMLIRQ,LTRPMLIRQ
    curr = time.ticks_us()
    dt = time.ticks_diff(curr,LTRPMLIRQ)
    LTRPMLIRQ = curr
    if dt < 100:
        return
    RPMLIRQ = 60/(4*dt/1000000)
    LTRPMLIRQ = curr
    

def rotaryUpdate(pin):
    global rotaryDT
    if(not pinRotaryA.value()) and (not pinRotaryB.value()):
        rotaryDT += 1
    
pinRPMMeterR.irq(trigger=Pin.IRQ_FALLING, handler=rightRPMMeterIRQ)

pinRPMMeterL.irq(trigger=Pin.IRQ_FALLING, handler=leftRPMMeterIRQ)

pinRotaryA.irq(trigger=Pin.IRQ_FALLING, handler=rotaryUpdate)

pinRotaryB.irq(trigger=Pin.IRQ_FALLING, handler=rotaryUpdate)



##Web GUI-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
extToMime = {"ico":"image/x-icon",
             "css":"text/css",
             "html":"text/html",
             "js" : "text/javascript"}
async def send_file(writer,fileName):
    try:  
        with open(fileName, "rb") as file:
            size = 6400
            
            writer.write("HTTP/1.0 200 OK\r\nContent-type: ")
            writer.write(extToMime[fileName.split('.')[-1]])
            writer.write("\r\n\r\n")
            await writer.drain()

            
            print(os.stat(fileName)[6])
            rem = os.stat(fileName)[6]
            print("wow")
            while rem > 0:
                if rem >= size:
                    writer.write(file.read(size))
                    rem -= size
                else:
                    writer.write(file.read(rem))
                    rem = 0
                await writer.drain()
            file.close()

    except OSError as e:
        writer.write("HTTP/1.0 404 Not found\r\nContent-type: text/html")
        writer.write("\r\n\r\n")
        writer.write("404 Not Found, hehe")
        await writer.drain()
        print("Not Found!",e)
    writer.close()
    await writer.wait_closed()
    return

async def serve_client(reader, writer):
    global PWML,RPML,PWMR,RPMR,currentPhiTicks
    contentLength = 0
    request = await reader.readline()
    print(request)
    
    head = await reader.readline()
    while head != b"\r\n":
        print(head)
        head = head.decode()
        if(head.find("Content-Length")!=-1):
            contentLength = int(head.split()[-1])
        head = await reader.readline()

    try:
        request = request.decode().split()[1].split('?')
        print(request)
        incomingData = request[-1].split('=')[-1]
        requestAddress = request[0]
        print('Request:', request)
    except IndexError:
        pass
    

    
    if (requestAddress == '/'):
        await send_file(writer,"home.html")
        return
            
            
    
    elif (requestAddress == '/getStats'):
        writer.write('HTTP/1.0 200 OK\r\nContent-type: application/json\r\n\r\n')
        await writer.drain()
        
        j = json.dumps({"pwml":PWML,"rpml":RPMLIRQ,"pwmr":PWMR,"rpmr":RPMRIRQ,"theta":servo.current_angle,"phiTicks":currentPhiTicks,"ballSpeed":ballSpeed,"ballDT":ballDT})
        writer.write(j)
        await writer.drain()
        
        writer.close()
        await writer.wait_closed()
        return
    
    elif (requestAddress == '/setParams'):
        
        j = await reader.read(contentLength)
        print("j",j)
        j = json.loads(j.decode())
        
        PWML = int(j["pwml"])
        PWMR = int(j["pwmr"])
        asyncio.run(setTheta(int(j["theta"])))
        asyncio.run(setPhiTicks(int(j["phiTicks"])))

        pwmLaunchMotorR.duty_u16(PWMR)
        pwmLaunchMotorL.duty_u16(PWML)
        
        writer.write('HTTP/1.0 200 OK\r\nContent-type: text/plain\r\n\r\nok')
        await writer.drain()
        writer.close()
        await writer.wait_closed()
        
        return
    
    elif (requestAddress == '/shoot'):
        
        asyncio.run(load())
        asyncio.run(unload())

        writer.close()
        await writer.wait_closed()
        return
    
    
        
    else:
        fileName = requestAddress[1:]
        await send_file(writer,fileName)  
        return
        
    
    return

##Init-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
wap = network.WLAN(network.AP_IF)
wap.config(essid='Tennis Robot', password='deeznutz')
wap.active(True)
print(wap.ifconfig())
print('Setting up webserver...')

async def net_and_gui():
    asyncio.create_task(asyncio.start_server(serve_client, "0.0.0.0", 80))
    while True:
        await asyncio.sleep(1)

# Create an Event Loop
loop = asyncio.get_event_loop()
# Create a task to run the main function
loop.create_task(net_and_gui())
asyncio.create_task(initMotors())

try:
    # Run the event loop indefinitely
    loop.run_forever()
except Exception as e:
    print('Error occured: ', e)
except KeyboardInterrupt:
    print('Program Interrupted by the user')

