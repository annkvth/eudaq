#! /usr/bin/env python3
# load binary lib/pyeudaq.so
import pyeudaq
from pyeudaq import EUDAQ_INFO, EUDAQ_ERROR
import time

def exception_handler(method):
    def inner(*args, **kwargs):
        try:
            return method(*args, **kwargs)
        except Exception as e:
            EUDAQ_ERROR(str(e))
            raise e
    return inner

class ExamplePyProducer(pyeudaq.Producer):
    def __init__(self, name, runctrl):
        pyeudaq.Producer.__init__(self, name, runctrl)
        self.is_running = 0
        EUDAQ_INFO('New instance of ExamplePyProducer')

    @exception_handler
    def DoInitialise(self):        
        EUDAQ_INFO('DoInitialise')
        iniList=self.GetInitConfiguration().as_dict()
        if 'key_a' in iniList:
            initA=iniList['key_a']
            print(f'key_a(init) = {initA}')
        if 'key_b' in iniList:
            initB=iniList['key_b']
            print(f'key_b(init) = {initB}')

    @exception_handler
    def DoConfigure(self):        
        EUDAQ_INFO('DoConfigure')
        confList = self.GetConfiguration().as_dict()
        if 'key_a' in confList:
            confA=confList['key_a']
            print(f'key_a(conf) = {confA}')
        if 'key_b' in confList:
            confB=conf['key_b']
            print(f'key_b(conf) = {confB}')

        
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
        EUDAQ_INFO("Start of RunLoop in ExamplePyProducer")
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
        EUDAQ_INFO("End of RunLoop in ExamplePyProducer")

if __name__ == "__main__":
    import argparse
    parser=argparse.ArgumentParser(description='Power Producer',formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--run-control' ,'-r',default='tcp://localhost:44000')
    parser.add_argument('--name' ,'-n',default='PyProducer')
    args=parser.parse_args()

    myproducer = ExamplePyProducer(args.name,args.run_control)
    print (f"connecting to runcontrol in {args.run_control}")
    myproducer.Connect()
    time.sleep(2)
    while(myproducer.IsConnected()):
        time.sleep(1)
