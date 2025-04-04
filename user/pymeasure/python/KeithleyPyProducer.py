#! /usr/bin/env python3

import pyeudaq
from pyeudaq import EUDAQ_INFO, EUDAQ_ERROR
import time
from datetime import datetime
from pymeasure.instruments.keithley import Keithley2450
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
        self.sourcemeter=None
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
        self.sourcemeter = Keithley2450(f'TCPIP::{self.ip}::inst0::INSTR')
        keithley.source_current_range = 200
        self.sourcemeter.compliance_current = 50e-6
        self.sourcemeter.source_voltage_range = 200

    @exception_handler
    def DoConfigure(self):        
        EUDAQ_INFO('DoConfigure')
        confList = self.GetConfiguration().as_dict()
        self.bias = confList['bias']
        if bias > 0:
            print("Don't kill the LGAD!")
        else:
            self.sourcemeter.enable_source()
            print(f'Keithley set to {self.bias} V')
            #target voltage, steps, pause-in-seconds
            self.ramp_to_voltage(self.bias, 30, 0.5) 

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
            self.sourcemeter.measure_current()
            print(f'{self.sourcemeter.voltage}V {self.sourcemeter.current * 1e6}uA')
            trigger_n += 1
            time.sleep(1)
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
