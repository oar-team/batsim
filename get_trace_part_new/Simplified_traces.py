# This program is used to simplify split trace files
import os,os.path,re,time,json;
def Simplified_trace_function():
    d_compute = {}
    d_comm = {}

    sourceFileList=os.listdir('split_files/'); # get the list of split trace files
    targetFile='simplified_trace/reduceResult.txt';
    for eachFile in sourceFileList: #traserve the split files
        currentFile=open('split_files/'+eachFile,'r',encoding='utf8'); # open split files
        temp_dict = {}
        final_list=[]

        for line in currentFile:
            if len(line) <= 1:
                continue

            p_re=re.compile('\d\s.*?\s\d+\.\d+|\d\s(?!send)(?!allreduce).*?\s\d+\s\d+'); # use regular expression to extract useful information of each line
            match=p_re.findall(line); 
            s_re = re.compile('\s')
            if(match):  
                
                split_elements = s_re.split(match[0]) #in irecv, recv case
                if (split_elements[1] == 'irecv' or split_elements[1] == 'recv'):
                    url=split_elements[0]+' '+split_elements[1]+' '+split_elements[2]
                    val = float(split_elements[3])
            

                elif (split_elements[1] == 'allreduce'): #in allreduce case, we directly append it in the final list
                    final_list.append(match[0]);  
                    final_list.append('\n')

                else:
                    url=split_elements[0]+' '+split_elements[1] 
                    val = float(split_elements[2])

           
                if url in temp_dict: 
                    temp_dict[url]+=val;   
                else:
                    temp_dict[url]=val;  
         

        currentFile.close();  
        
        for key,value in temp_dict.items():
            final_list.append(key+' '+str(value));  
            final_list.append('\n')  


        tempFile=open(targetFile+eachFile,'a',encoding='utf8');  
        tempFile.writelines(final_list);  
        tempFile.close()  

   # print(targetFile+eachFile+'.txt  is created '+str(time.asctime()));  
      
