import re
import math as M
from pathlib import Path
from time import sleep

if __name__ == "__main__":
    while True:
        device_path = Path("/dev/ilps28qsw1")
        f = open(device_path, "r")
        string_val = f.readline()
        re_p = r"^Presure: (\d*), Temp: (\d*)$"
        x = re.search(re_p, string_val);
        if x == None:
            print("Com error");
        pressure_raw = int(x.group(1));
        temp_raw = int(x.group(2));

        pressure = pressure_raw/(4096.0*256)
        temp = temp_raw/100.0
        print(f"{pressure: < 10.4f}\t{temp: < 10.4f}\n")
        f.close()
        sleep(1)


