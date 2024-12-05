Python producer using pymeasure to talk with instruments

(e.g. for steering a Keithley power supply)

Dependency: pymeasure and pyeudaq and pyvisa-py

Install pymeasure with `pip install pymeasure` .

Install EUDAQ with the `EUDAQ_BUILD_PYTHON` cmake flag.
        cmake .. -DEUDAQ_BUILD_PYTHON=ON
