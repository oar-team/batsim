# this program is used to split the large trace files into small files which are stored in the folder smallfiles

import os,os.path,time; 

source_folder=os.listdir('traces'); #get the folder of trace files
targetDir='files/split_files'; #set the output folder
file_index=1;
smallFileSize=2000; #set the split number in each small files
tempList=[]; #temporary list, which is uesd to store the content of the small files

for eachFile in source_folder: #traserve the trace files 
    currentFile=open('traces/'+eachFile,'r',encoding='utf8'); #open trace files 
    current_line=currentFile.readline();



    while(current_line): 
        lineNum=1  
        while(lineNum<=smallFileSize):   
            tempList.append(current_line); #store the result into the temporary list 
            current_line=currentFile.readline(); #read the next line
            lineNum+=1;# increase the line number
            if not current_line:break;# if finishing reading the whole file, we break the loop

        tempFile=open('split_files/access_'+str(file_index)+'.txt','w',encoding='utf8');#store into the target files 
        tempFile.writelines(tempList);  
        tempFile.close();  
        tempList=[];#make the list empty  
        print('smallFiles/access_'+str(file_index)+'.txt  is created at '+str(time.asctime()));  
        file_index+=1; #increase the file index
    currentFile.close();        