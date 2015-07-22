# This program is used to calculate the original computation vector and the communication matrix
import os,os.path,re,time,json;
d_compute = {}
d_comm = {}
matrix_index = ()
matrix = []

def createcomp_dic(val,key,d_compute):# function for calculating the original computation vector
    if key in d_compute:
        d_compute[key] += float(val)
    else:
        d_compute[key] = float(val)
    return d_compute
    

def createcomm_dic(val,keys,d_comm): # function for calculating the original communication matrix
    if keys in d_comm.keys():
        d_comm[keys] += float(val)
        
    else:
        d_comm[keys] = float(val)
    return d_comm


def bcast_calculator (file_number,key,val,d_comm):
    p_number = 0
    while p_number <= file_number-1:
        if p_number == key:
            p_number += 1
            continue;
        else:
            keys = (key, p_number)
            d_comm = createcomm_dic(val,keys,d_comm)
            p_number += 1
    return d_comm


def allreduce_calculator (file_number,key,val,d_comm):
    p_number = 0

    while p_number <= file_number-1:
        if p_number == key:
            coeff = file_number - 1
            changed_val = (coeff * val)
            keys = (key, key)
            d_comm = createcomm_dic(changed_val,keys,d_comm)
            p_number += 1
        else:
            keys = (key, p_number)
            d_comm = createcomm_dic(val,keys,d_comm)  
            p_number += 1
    return d_comm


def file_number():
    path = ('traces')
    file_number = 0
    for files in os.listdir(path):
        if files.endswith('.txt'):
            file_number += 1
    return file_number

file_number = file_number();


def matrix_index(file_number):
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


sourceFileList=os.listdir('split_files/'); #get the list of split files
for eachFile in sourceFileList: #traserve the split files
    currentFile=open('split_files/'+eachFile,'r',encoding='utf8'); #open split files

    for line in currentFile:
        if len(line) <= 1:
            continue

        words = line.split()
        key,cmd = int (words[0]), words[1]
        
        if cmd == "compute":
            val = words[2]
            d_compute = createcomp_dic(val,key,d_compute)
            
        elif cmd == "recv" or cmd == "irecv":
            key1 = int (words[2])
            val = float(words[3])
            keys = (key1,key)
            d_comm = createcomm_dic(val,keys,d_comm)


        elif cmd == "bcast":
            val = float(words[2])
            d_comm = bcast_calculator (file_number,key,val,d_comm)

        elif cmd == "allreduce":
            com_val = float(words[2])
            d_comm = allreduce_calculator(file_number,key,com_val,d_comm)
            cop_val = words[3]
            d_compute = createcomp_dic(cop_val,key,d_compute)

    currentFile.close();  

comp = []
comm = []

i = 0
for i in range (file_number):
    if i in d_compute:
        comp.append(float(d_compute[i]))
    else:
        comp.append(float(0))


for index in matrix:
    if index in d_comm.keys():
        comm.append(float(d_comm[index]))
    else:
        comm.append(float(0))





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
            "cpu": comp,
            "com": comm
    }

    }
}

with open('generated_trace.json', 'w') as f:
    out = json.dumps(measurements, indent=2)
    f.write (json.dumps(measurements, indent=2,separators=(',',':')))
