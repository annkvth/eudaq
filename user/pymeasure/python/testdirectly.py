from pymeasure.instruments.keithley import Keithley2450
sourcemeter = Keithley2450("TCPIP::192.168.21.254::inst0::INSTR")
keithley.source_current_range = 200
sourcemeter.compliance_current = 50e-6
sourcemeter.source_voltage_range = 200
sourcemeter.enable_source()
sourcemeter.ramp_to_voltage(-20,5,1)
sourcemeter.measure_current()
print(f'{sourcemeter.voltage}V {sourcemeter.current * 1e6}uA')
sourcemeter.ramp_to_voltage(0,5,1)
sourcemeter.disable_source()
sourcemeter.shutdown()
