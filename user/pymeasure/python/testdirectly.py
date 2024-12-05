from pymeasure.instruments.keithley import Keithley2450
sourcemeter = Keithley2450("TCPIP::192.168.21.254::inst0::INSTR")
keithley.source_current_range = 200
sourcemeter.compliance_current = 50e-6
sourcemeter.source_voltage_range = 200
sourcemeter.source_voltage = 20
sourcemeter.enable_source()
sourcemeter.measure_current()
print(f'{sourcemeter.voltage}V {sourcemeter.current * 1e6}uA')
sourcemeter.disable_source()
sourcemeter.shutdown()
