#! /usr/bin/env python3

import pyeudaq
from pyeudaq import EUDAQ_INFO, EUDAQ_ERROR
import time
from datetime import datetime
#from pymeasure.instruments.keithley import Keithley2450
import numpy as np
import socket
import threading

def exception_handler(method):
    def inner(*args, **kwargs):
        try:
            return method(*args, **kwargs)
        except Exception as e:
            EUDAQ_ERROR(str(e))
            raise e
    return inner

class KeithleyPyProducer(pyeudaq.Producer):
    def __init__(self, name, runctrl):
        pyeudaq.Producer.__init__(self, name, runctrl)
        self.name = name
        self.ip=None
        self.is_running = 0
        self.keithley=None
        self.lock=threading.Lock()
        EUDAQ_INFO('New instance of KeithleyPyProducer')

    @exception_handler
    def DoInitialise(self):        
        iniList=self.GetInitConfiguration().as_dict()
        if 'key_a' in iniList:
            initA=iniList['key_a']
            print(f'key_a(init) = {initA}')
        # let's not do an if-in-list check - it should be there!
        # else I want to crash (until I imprement proper exceptions...)
        self.ip = iniList['IPaddress']
        print(f'Keithley IPaddress = {self.ip}')

        # could also make these ini parameter in the future
        self.compliance_current = 20e-6
        self.source_voltage_range = 200
        self.update_every = 30
        
        self.keithley = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.keithley.connect((self.ip, 5025))
        self.keithley.sendall('*IDN?\n'.encode())
        time.sleep(0.01)   
        self.keithley.sendall((f'SOUR:VOLT:RANG {self.souce_voltage_rage}\n').encode())
        time.sleep(0.01)   
        print((keithley.recv(1024)).decode())

        self.keithley.sendall((f':SOUR:VOLT:ILIM {self.compliance_current}\n').encode())
        time.sleep(0.01)   
        self.keithley.sendall('SOUR:FUNC VOLT\n'.encode())
        time.sleep(0.01)       
        self.keithley.sendall('SENS:FUNC "CURR"\n'.encode())
        time.sleep(0.01)
        self.keithley.sendall('SENS:CURR:RANG:AUTO ON\n'.encode())
        time.sleep(0.01)        
        self.keithley.sendall(':OUTP ON\n'.encode())
        time.sleep(0.01)

    @exception_handler
    def DoConfigure(self):        
        EUDAQ_INFO('DoConfigure')
        confList = self.GetConfiguration().as_dict()
        self.vtarget = confList['bias']
        if bias > 0:
            print("Don't kill the LGAD!")
        else:
            print(f'Keithley set to {self.vtarget} V')
            #could also be parameters: steps, pause-in-seconds
            nsteps = 10
            steppause=2.0
            # get current voltage and ramp to there
            self.keithley.sendall(':MEAS:VOLT? "voltMeas"\n'.encode())
            vmeas =int(float((keithley.recv(1024)).decode())) 
            stepsize=(self.vtarget - vmeas)/nsteps
            thesteps = np.arange(vmeas, self.vtarget+(stepsize*0.5), stepsize)
            for step in thesteps:
                self.keithley.sendall(('SOUR:VOLT {step}\n').encode())
                time.sleep(steppause)  
                self.keithley.sendall(':MEAS:VOLT? "voltMeas"\n'.encode())
                volt=(float((keithley.recv(1024)).decode()))        
                self.keithley.sendall(':MEAS:CURR? "currMeas"\n'.encode())
                curr=(float((keithley.recv(1024)).decode()))
                self.keithley.sendall(':MEAS? "defbuffer1", SEC\n'.encode())
                t=(float((keithley.recv(1024)).decode()))
                logline=f'{volt}V {curr*1e6}uA {t}s'
                print(logline)
            EUDAQ_INFO(logline)


            
    @exception_handler
    def DoStartRun(self):
        EUDAQ_INFO('DoStartRun')
        self.is_running = 1
        
    @exception_handler
    def DoStopRun(self):        
        EUDAQ_INFO('DoStopRun')
        self.is_running = 0

    @exception_handler
    def DoReset(self):        
        EUDAQ_INFO('DoReset')
        self.is_running = 0

    @exception_handler
    def RunLoop(self):
        EUDAQ_INFO("Start of RunLoop in KeithleyPyProducer")
        trigger_n = 0;
        while(self.is_running):
            ev = pyeudaq.Event("RawEvent", "sub_name")
            ev.SetTriggerN(trigger_n)

            time.sleep(self.update_every)
            self.keithley.sendall(':MEAS:VOLT? "voltMeas"\n'.encode())
            volt=(float((keithley.recv(1024)).decode()))        
            self.keithley.sendall(':MEAS:CURR? "currMeas"\n'.encode())
            curr=(float((keithley.recv(1024)).decode()))
            self.keithley.sendall(':MEAS? "defbuffer1", SEC\n'.encode())
            t=(float((keithley.recv(1024)).decode()))
            logline=f'{volt}V {curr*1e6}uA {t}s'
            EUDAQ_INFO(logline)
            trigger_n += 1
        EUDAQ_INFO("End of RunLoop in KeithleyPyProducer")

if __name__ == "__main__":
    import argparse
    parser=argparse.ArgumentParser(description='Power Producer',formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--run-control' ,'-r',default='tcp://localhost:44000')
#    parser.add_argument('--run-control' ,'-r',default='tcp://localhost:44123')
    parser.add_argument('--name' ,'-n',default='KeithleyProducer')
    args=parser.parse_args()

    keiSMproducer = KeithleyPyProducer(args.name,args.run_control)
    print (f"connecting to runcontrol in {args.run_control}")
    keiSMproducer.Connect()
    time.sleep(2)
    while(keiSMproducer.IsConnected()):
        time.sleep(1)
