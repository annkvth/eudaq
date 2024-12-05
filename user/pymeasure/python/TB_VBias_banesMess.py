import socket
import time
import argparse
from typing import DefaultDict
import numpy as np

parser = argparse.ArgumentParser(description='Control for Keithley 2470')
parser.add_argument('-comp','--comp', help = 'Set current compliance [uA]', default = 1450, type = float) 
parser.add_argument('-vtar','--vtar', help = 'Set target voltage [V]', default = 0, type = int) 
parser.add_argument('-nsteps','--nsteps', help = 'Set number of steps for IV curve', default = 10, type = int) 
parser.add_argument('-tcurve','--tcurve', help = 'Set time between steps [s]', default = 2, type = float)
parser.add_argument('-taq','--taq', help = 'Time between acquisitions at stable voltage [s]', default = 20, type = float)
parser.add_argument('-file','--file', help  = 'Name of output file', default = 'Monitor_Data/IV_Monitor.txt', type = str)
#parser.add_argument('-file','--file', help  = 'Name of output file', default = 'IV_Monitor.txt', type = str)
parser.add_argument('-ip','--ip', help  = 'IP address', default = '169.254.19.233', type = str)
args = parser.parse_args()

print(f'Communicating with Keithley on IP address {args.ip}')
print(f'Please note: this script is configured to set only negative target voltages, since that is what we usually want.')

keithley = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
keithley.connect((args.ip, 5025))
keithley.sendall('*IDN?\n'.encode())
time.sleep(0.01)   
keithley.sendall(('SOUR:VOLT:RANG 1000\n').encode())
time.sleep(0.01)   
print((keithley.recv(1024)).decode())

if args.comp > 5400:
    args.comp = 5400
    print('\nCompliance too high! New compliance set to '+str(args.comp)+' uA\n')

if abs(args.vtar > 810):
    args.vtar = 1 
    print('\nVoltage too high! New voltage target set to -'+str(args.vtar)+' V\n')

keithley.sendall(':READ? "defbuffer1", DATE\n'.encode())
time.sleep(0.01)   
date = (keithley.recv(1024)).decode()
date = date[:-1]
d = date.replace('/', '')
keithley.sendall(':READ? "defbuffer1", TIME\n'.encode())
time.sleep(0.01)   
tim = (keithley.recv(1024)).decode()
tim=tim[:-1]
tim2 = tim.replace('/', '')
args.file = args.file.replace('.txt', '')
args.file = args.file+'_'+str(args.vtar)+'V_'+d+'_'+tim2+'.txt'
#args.file = args.file+'_'+str(args.vtar)+'V_'+d+'.txt'

print('Current compliance = '+str(args.comp)+' uA')
print('V target = -'+str(args.vtar)+' V')
print('Number of steps for IV curve = '+str(args.nsteps))
print('Time required for each step of the IV curve = '+str(args.tcurve)+' s')
print('Output file name = '+args.file)
print('IP Address [Keithley 2470] = '+args.ip)
print('')

def main(args):
    comp = abs(args.comp)
    keithley.sendall((':SOUR:VOLT:ILIM '+str(comp*1E-6)+'\n').encode())
    time.sleep(0.01)   
    vtar = abs(args.vtar)
    keithley.sendall('SOUR:FUNC VOLT\n'.encode())
    time.sleep(0.01)       
    keithley.sendall('SENS:FUNC "CURR"\n'.encode())
    time.sleep(0.01)
    keithley.sendall('SENS:CURR:RANG:AUTO ON\n'.encode())
    time.sleep(0.01)

    nsteps = abs(args.nsteps)
    tcurve = abs(args.tcurve)
    taq = abs(args.taq)

    OUTPUT = open(args.file, "w")
    OUTPUT.write('Voltage [V]\tCurrent [A]\tRelTime [s]\n')

    keithley.sendall(':OUTP ON\n'.encode())
    time.sleep(0.01)
    keithley.sendall('TRACe:MAKE "voltMeas", 800\n'.encode())
    time.sleep(0.01)
    keithley.sendall('TRACe:MAKE "currMeas", 800\n'.encode())
    time.sleep(0.01)


    keithley.sendall(':MEAS:VOLT? "voltMeas"\n'.encode())
    vin=int(float((keithley.recv(1024)).decode()))  

    # if target threshold = 0, turn output off after ramp
    curr = []
    volt = []
    t = []
    j = 0
    for V in range (0,nsteps+1):
        keithley.sendall(('SOUR:VOLT '+str(-abs(V*(-abs(vtar)+abs(vin))/nsteps-abs(vin)))+'\n').encode())
        time.sleep(tcurve)  
        keithley.sendall(':MEAS:VOLT? "voltMeas"\n'.encode())
        volt.append(float((keithley.recv(1024)).decode()))        
        keithley.sendall(':MEAS:CURR? "currMeas"\n'.encode())
        curr.append(float((keithley.recv(1024)).decode()))
        keithley.sendall(':MEAS? "defbuffer1", SEC\n'.encode())
        t.append(float((keithley.recv(1024)).decode()))
        if V == 0: t0 = t[0]      
        OUTPUT.write(time.strftime("%x %X")+'\t'+str(volt[j])+'\t'+str(curr[j])+'\t'+str(t[j]-t0)+'\n')
        print(str(volt[j])+' V\t'+str("%.3f" % (curr[j]*1e6))+' uA\t'+str(t[j]-t0)+' s'+'\n')
        j=j+1
    if vtar == 0:
        keithley.sendall(':MEAS:VOLT? "voltMeas"\n'.encode())
        volt.append(float((keithley.recv(1024)).decode())) 
        keithley.sendall(':MEAS:CURR? "currMeas"\n'.encode()) 
        curr.append(float((keithley.recv(1024)).decode()))
        keithley.sendall(':MEAS? "defbuffer1", SEC\n'.encode())
        t.append(float((keithley.recv(1024)).decode()))
        OUTPUT.write(str(volt[j])+'\t'+str(curr[j])+'\t'+str(t[j]-t0)+'\n')
        print(str(volt[j])+' V\t'+str("%.3f" % (curr[j]*1e6))+' uA\t'+str(t[j]-t0)+' s'+'\n')
        keithley.sendall(':OUTP OFF\n'.encode())
    else:    
        try:
            while True:
                keithley.sendall(':MEAS:VOLT? "voltMeas"\n'.encode())
                volt.append(float((keithley.recv(1024)).decode()))
                keithley.sendall(':MEAS:CURR? "currMeas"\n'.encode()) 
                curr.append(float((keithley.recv(1024)).decode()))
                keithley.sendall(':MEAS? "defbuffer1", SEC\n'.encode())
                t.append(float((keithley.recv(1024)).decode()))
                time.sleep(taq)

                keithley.sendall('TRACe:CLEar "voltMeas"\n'.encode()) # clear buffers to avoid overflow
                time.sleep(0.01)
                keithley.sendall('TRACe:CLEar "currMeas"\n'.encode()) # clear buffers to avoid overflow
                time.sleep(0.01)
                keithley.sendall('TRACe:CLEar "defbuffer1"\n'.encode())
                time.sleep(0.01)

                OUTPUT.write(time.strftime("%x %X") + '\t' + str(volt[j])+'\t'+str(curr[j])+'\t'+str(t[j]-t0)+'\n')
                print(str(volt[j])+' V\t'+str("%.3f" % (curr[j]*1e6))+' uA\t'+str(t[j]-t0)+' s'+'\n')
                j=j+1
                OUTPUT.flush()
        except KeyboardInterrupt: pass 
    OUTPUT.close()
        
    keithley.sendall('TRAC:DEL "voltMeas"\n'.encode()) # delete buffers
    time.sleep(0.01)
    keithley.sendall('TRAC:DEL "currMeas"\n'.encode()) # delete buffers
    time.sleep(0.01)    
  

    print('Buffers cleared and deleted, all good. Good job son.')

if __name__ == "__main__":
    main(args)
