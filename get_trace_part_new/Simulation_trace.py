import json
import math
import time

f=open('generated_trace.json')
trace_file = json.load(f)       

cpu_value = (trace_file['profiles']['1']['cpu']) 
com_value = ((trace_file['profiles']['1']['com']))

class define_value:
    def __init__(self):
        self.casenum = 0
        self.coeff1 = 0
        self.coeff2 = 0   
        self.way = 0
        self.changecoeff = 0
        self.lowboundry = 0
        self.highboundry = 0

    def set_value (self):
        self.casenum = input("please type the casenum: 1 for coefficient * originalvalue, 0 for coefficient + originalvalue ");
        print ("casenum: " + self.casenum)

        self.coeff1 = input ("please type the coefficient\n")
        print ("coefficient: " + self.coeff1)
        self.coeff1 = float (self.coeff1) 
                           
        self.way = input("please type the way you want to change coefficient, 1 for coeffi*changecoeff, 0 for coeffi+changecoeff\n");
        print("the way you choose: " + self.way)                    
                               
        self.changecoeff = float(input("please type the value to change coefficient "))
        print("the value to change coefficient: ")
        print( self.changecoeff)

        self.lowboundry = float(input("please type the low  boundry of coefficient1\n"))
        print("lowboundry: " )
        print(self.lowboundry)

        self.highboundry = float(input("please type the high boundry of coefficient1\n"))
        print("highboundry: " )
        print(self.highboundry)

    def coeffgenerator (self):
        if(self.way == '1'):  
            self.coeff1 = self.changecoeff * self.coeff1 
        elif(self.way == '0'):
            self.coeff1 = self.changecoeff + self.coeff1 
    
  
class define_profile:
    def __init__(self):
        self.cpu_vector = []
        self.com_matrix = []

    def transform_type (self,cpu_value, com_value):
        length = len (cpu_value)
        for i in range (length):
            self.cpu_vector.append (float (cpu_value[i]))
            for j in range (length):            
                self.com_matrix.append(float (com_value[i*length +j]))
            
    def changing_trace (self,user_define):
        length = len (self.cpu_vector)
        if(user_define.casenum == '1'):
            coeff2 = math.sqrt(user_define.coeff1)
        for i in range (length):
            if(user_define.casenum == '0'):
                cpu_origin_value = self.cpu_vector[i]
                self.cpu_vector[i] = user_define.coeff1 + self.cpu_vector[i]
                if(self.cpu_vector[i] <= 0):
                    print("over the range")
                    break
                coeff2 = math.sqrt(self.cpu_vector[i] / cpu_origin_value)
            else:

                self.cpu_vector[i] = user_define.coeff1 * self.cpu_vector[i]
                
            for j in range (length):
                self.com_matrix[i*length + j] = coeff2 * self.com_matrix[i*length+j]
    


      

user_define = define_value()
profiles = define_profile() 

measurements = {
    "version": 0,
    "command:": "",
    "date": str(time.asctime()),
    "description": "workload with profile file for test",
    "nb_res": 4,
    "jobs": [
        {"id":1, "subtime":10, "walltime": 100, "res": 4, "profile": "1"},
        {"id":2, "subtime":20, "walltime": 100, "res": 4, "profile": "1"},
        {"id":3, "subtime":30, "walltime": 3,   "res": 4, "profile": "1"},
        {"id":4, "subtime":32, "walltime": 100, "res": 4, "profile": "1"}
    ],
    "profiles": {
        "1": {
            "type": "msg_par",
            "cpu": None,
            "com": None
            }
        }
}

def trace_main_function(cpu_value,com_value,profiles,user_define,measurements):
    file_index = 0; 
    profiles.transform_type(cpu_value, com_value)
    user_define .set_value()
    ori_cpu = list(profiles.cpu_vector)
    ori_com = list(profiles.com_matrix)
    while (user_define.coeff1 >= user_define.lowboundry and user_define.coeff1 <= user_define.highboundry):
        user_define.coeffgenerator()
        profiles.changing_trace (user_define)
        if (user_define.changecoeff == 1 and user_define.way == '1'):
            print("please reset the parameters")
            break
        elif (user_define.changecoeff == 0 and user_define.way == '0'):
            print("please reset the parameters")
            break
        measurements["profiles"]["1"]["cpu"] = profiles.cpu_vector
        measurements["profiles"]["1"]["com"] = profiles.com_matrix
        profiles.cpu_vector = list(ori_cpu)
        profiles.com_matrix = list(ori_com)
        print("dddddddddd")
        print(ori_cpu)
        #profiles.transform_type(cpu_value, com_value)
        with open('simulated_traces/trace_'+str(file_index)+'.json', 'w') as f:
            out = json.dumps(measurements, indent=2)
            f.write (json.dumps(measurements, indent=2,separators=(',',':')))
            file_index+=1; 

trace_main_function(cpu_value,com_value,profiles,user_define,measurements)
