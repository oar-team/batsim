#!/usr/bin/python2

import argparse

def generate_flat_platform(nb_hosts, output_file):

    hosts_id = range(0, nb_hosts)


    #on Taurus with TimeNasEP_C
    freqs = [
    {"freq":2301000,    "maxWatt":203.12,   "moyWatt":190.738,  "time":28.2},
    {"freq":2300000,    "maxWatt":180.5,    "moyWatt":171.02,   "time":31.7},
    {"freq":2200000,    "maxWatt":174.0,    "moyWatt":165.62,   "time":33.7},
    {"freq":2100000,    "maxWatt":169.25,   "moyWatt":160.47,   "time":35.0},
    {"freq":2000000,    "maxWatt":163.5,    "moyWatt":155.729,  "time":37.0},
    {"freq":1900000,    "maxWatt":158.12,   "moyWatt":151.30,   "time":38.9},
    {"freq":1800000,    "maxWatt":153.88,   "moyWatt":146.92,   "time":41.0},
    {"freq":1700000,    "maxWatt":149.25,   "moyWatt":142.95,   "time":43.6},
    {"freq":1600000,    "maxWatt":145.25,   "moyWatt":138.928,  "time":46.4},
    {"freq":1500000,    "maxWatt":141.0,    "moyWatt":135.368,  "time":48.1},
    {"freq":1400000,    "maxWatt":137.5,    "moyWatt":132.519,  "time":56.3},
    {"freq":1300000,    "maxWatt":133.5,    "moyWatt":128.87,   "time":57.3},
    {"freq":1200000,    "maxWatt":130.25,   "moyWatt":125.88,   "time":62.7}

    ]
    maxx = freqs[0]["time"]
    for f in freqs:
        f["speed"] = 100.0*maxx/f["time"]

    #ll = []
    #for f in freqs:
        #ll.append(f["maxWatt"])
    #print ll

    idle_watt = 95.0

    watt_off = 9.75
    time_on_to_off = 6.1
    watt_on_to_off = 616.08/time_on_to_off
    time_off_to_on = 151.52
    watt_off_to_on = 18966.4228/time_off_to_on


    speed_str = "Mf, ".join([str(f["speed"]) for f in freqs])+"Mf"
    speed_str += ", 1e-9Mf, "+str(1.0/time_on_to_off)+"f, "+str(1.0/time_off_to_on)+"f"

    sleep_pstates = str(len(freqs))+":"+str(len(freqs)+1)+":"+str(len(freqs)+2)

    ll = []
    for f in freqs:
        ll.append(str(idle_watt)+":"+str(f["moyWatt"]))
    ll.append(str(watt_off)+":"+str(watt_off))
    ll.append(str(watt_on_to_off)+":"+str(watt_on_to_off))
    ll.append(str(watt_off_to_on)+":"+str(watt_off_to_on))

    watt_per_state = ", ".join(ll)



    header ="""
<?xml version='1.0'?>
<!DOCTYPE platform SYSTEM "http://simgrid.gforge.inria.fr/simgrid/simgrid.dtd">
<platform version="4">
<config id="General">
    <prop id="network/coordinates" value="yes"></prop>
    <!-- Reduce the size of the stack_size. On huge machine, if the stack is too big (8Mb by default), Simgrid fails to initiate.
    See http://lists.gforge.inria.fr/pipermail/simgrid-user/2015-June/003745.html-->
        <prop id="contexts/stack_size" value="16"></prop>
        <prop id="contexts/guard_size" value="0"></prop>

</config>

<AS id="AS0"  routing="Vivaldi">
    <host id="master_host" coordinates="0 0 0" speed="100Mf">
        <prop id="watt_per_state" value="100:200" />
        <prop id="watt_off" value="10" />
    </host>

    <!-- The state 3 of Mercury is a sleep state.
    When switching from a computing state to the state 3, passing by the virtual pstate 4 is mandatory to simulate the time and energy consumed by the switch off.
    When switching from the state 3 to a computing state, passing by the virtual pstate 5 is mandatory to simulate the time and energy consumed by the switch on.
     -->
"""


    hosts = "".join(["""
        <host id="host"""+str(i)+"""" coordinates="0 0 0" speed=\""""+speed_str+"""" pstate="0" >
            <prop id="watt_per_state" value=\""""+watt_per_state+"""" />
            <prop id="watt_off" value="10" />
            <prop id="sleep_pstates" value=\""""+sleep_pstates+"""" />
        </host>""" for i in hosts_id])



    footer = """
</AS>
</platform>
"""

    output_file.write(header)
    output_file.write(hosts)
    output_file.write(footer)
    output_file.close()


def main():
    parser = argparse.ArgumentParser(description = 'One-point-topology SimGrid platforms generator')

    parser.add_argument('--nb', '-n',
                        type=int,
                        help='The number of computing entities to create',
                        required=True)

    parser.add_argument('--output', '-o',
                        type=argparse.FileType('w'),
                        help="The output file to generate",
                        required=True)

    args = parser.parse_args()

    generate_flat_platform(nb_hosts = args.nb, output_file = args.output)


if __name__ == "__main__":
    main()
