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
        conf = self.GetInitConfiguration().as_dict()
        self.ip = conf['IPaddress']
        print(f'IPaddress = {self.ip}')

    @exception_handler
    def DoConfigure(self):        
        EUDAQ_INFO('DoConfigure')
        #print 'key_b(conf) = ', self.GetConfigItem("key_b")

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
            #block = bytes(r'raw_data_string')
            #ev.AddBlock(0, block)
            #print ev
            # Mengqing:
            datastr = 'raw_data_string'
            block = bytes(datastr, 'utf-8')
            ev.AddBlock(0, block)
            print(ev)
            
            self.SendEvent(ev)
            trigger_n += 1
            time.sleep(1)
        EUDAQ_INFO("End of RunLoop in KeithleyPyProducer")

if __name__ == "__main__":
    import argparse
    parser=argparse.ArgumentParser(description='Power Producer',formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--run-control' ,'-r',default='tcp://localhost:44123')
    parser.add_argument('--name' ,'-n',default='KeithleyProducer')
    args=parser.parse_args()

    keiSMproducer = KeithleyPyProducer(args.name,args.run_control)
    print (f"connecting to runcontrol in {args.run_control}")
    keiSMproducer.Connect()
    time.sleep(2)
    while(keiSMproducer.IsConnected()):
        time.sleep(1)
