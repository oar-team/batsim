#This program is used to calculate the original computation vector and the communication matrix
import os,os.path,re,time,json,shutil;
import Split_large_files as file1
import Simplified_traces as file2
matrix_index = ()
matrix = []

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


class define_profile:
    def __init__(self):
        self.d_compute = {}
        self.d_comm = {}

    def createcomp_dic(self, val, key):# function for calculating the original computation vector
        if key in self.d_compute:
            self.d_compute[key] += float(val)
        else:
            self.d_compute[key] = float(val)

    def createcomm_dic(self, val,keys): # function for calculating the original communication matrix
        if keys in self.d_comm:
            self.d_comm[keys] += float(val)
        else:
            self.d_comm[keys] = float(val)

    def bcast_calculator (self,file_number,key,val):
        p_number = 0
        while p_number <= file_number-1:
            if p_number == key:
                p_number += 1
                continue;
            else:
                keys = (key, p_number)
                profiles.createcomm_dic(val,keys)
                p_number += 1

    def allreduce_calculator (self,file_number,key,val):
        p_number = 0
        while p_number <= file_number-1:
            if p_number == key:
                coeff = file_number - 1
                changed_val = (coeff * val)
                keys = (key, key)
                profiles.createcomm_dic(changed_val,keys)
                p_number += 1
            else:
                keys = (key, p_number)
                profiles.createcomm_dic(val,keys)  
                p_number += 1


    def trace_generate(self):
        sourceFileList=os.listdir('simplified_trace/'); #get the list of split files
        for eachFile in sourceFileList: #traserve the split files
          
            currentFile=open('simplified_trace/'+eachFile,'r',encoding='utf8'); #open split files
            for line in currentFile:
                if len(line) <= 1:
                    continue

                words = line.split()
                key,cmd = int (words[0]), words[1]

                
                if cmd == "compute":
                    val = words[2]
                    self.createcomp_dic(val,key)
                    
                elif cmd == "recv" or cmd == "irecv":
                    key1 = int (words[2])
                    val = float(words[3])
                    keys = (key1,key)
                    self.createcomm_dic(val,keys)


                elif cmd == "bcast":
                    val = float(words[2])
                    self.bcast_calculator (file_number,key,val)

                elif cmd == "allreduce":
                    com_val = float(words[2])
                    self.allreduce_calculator(file_number,key,com_val)
                    cop_val = words[3]
                    self.createcomp_dic(cop_val,key)

            currentFile.close()

profiles = define_profile()

class change_to_profile:
    def __init__(self):
        self.comp = []
        self.comm = []

    def create_comp(self,file_number,profiles):
        i = 0
        for i in range (file_number):
            if i in profiles.d_compute:
                self.comp.append(float(profiles.d_compute[i]))
            else:
                self.comp.append(float(0))

    def create_comm(self,matrix,profiles):
        for index in matrix:
            if index in profiles.d_comm.keys():
                self.comm.append(float(profiles.d_comm[index]))
            else:
                self.comm.append(float(0))




final_profiles= change_to_profile()


def file_number(): # function for counting the the number of files in the traces folder
    path = ('traces')
    file_number = 0
    for files in os.listdir(path):
        if files.endswith('.txt'):
            file_number += 1
    return file_number

file_number = file_number()

def matrix_index(file_number): # function for controling the index of elements in comm matrix
    i = 0
    for i in range (file_number):
        index1 = i
    
        j = 0
        for j in range(file_number):
            index2 = j
            matrix_index = (index2,index1)
            matrix.append((matrix_index))
    return matrix

matrix = matrix_index(file_number)

def delete_files(scr): # function for removing files  
    i = 0
    filelist=[]
    rootdir=scr
    filelist=os.listdir(rootdir)
    for f in filelist:
      filepath = os.path.join( rootdir, f )
      if os.path.isfile(filepath):
        os.remove(filepath)
        

def Generate_JSON_file_function(final_profiles,profiles,matrix,file_number,measurements):
    file1.Split_large_file_function()
    file2.Simplified_trace_function()
    profiles.trace_generate()
    print (profiles.d_compute)
    final_profiles.create_comp(file_number,profiles)
    final_profiles.create_comm(matrix,profiles)
    
    measurements["profiles"]["1"]["cpu"] = final_profiles.comp
    measurements["profiles"]["1"]["com"] = final_profiles.comm
    with open('generated_trace.json', 'w') as f:
        out = json.dumps(measurements, indent=2)
        f.write (json.dumps(measurements, indent=2,separators=(',',':')))

        delete_files('simplified_trace/')
        delete_files('split_files/')


Generate_JSON_file_function(final_profiles,profiles,matrix,file_number,measurements)