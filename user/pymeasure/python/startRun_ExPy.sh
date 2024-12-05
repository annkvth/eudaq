#!/usr/bin/env sh                                                                                         
BINPATH=../../../bin
$BINPATH/euRun &
sleep 5

python3 Keithley2470PyProducer.py &

