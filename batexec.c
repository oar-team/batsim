/* Copyright (c) 2007-2014. The SimGrid Team and OAR Team
 * All rights reserved.                                                     */
//#include "batexec.h"
#include <simgrid/msg.h>
#include <string.h>
#include <xbt/sysdep.h>         /* calloc, printf */
/* Create a log channel to have nice outputs. */
#include <xbt/log.h>
#include <xbt/asserts.h>

#include <math.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(batexec, "Batexec");


#include "job.h"
#include "utils.h"

int nb_nodes = 0;
msg_host_t *nodes;
int choice = 5;
int cpu_com = 5;
int num_best_coeff = 0;
int g = 0;


typedef struct definechanging
{
    int casenum;
    double coeff;
    int way;
    double changecoeff;
    double coefflow_boun;
    double coeffhigh_boun; 

}definechanging_t;


typedef struct coefficient
{
    double bestcoeff1;
    double bestcoeff2;
    double bestresult;
    double best_res_coeff1[1000];
    double best_res_coeff2[1000];

}coefficient_s;


void ori_value(definechanging_t* changeway)
{
 
  changeway->casenum = 5;
  changeway->coeff = 1;
  changeway->way = 0;
  changeway->changecoeff = 0;
  changeway->coefflow_boun  = 0;
  changeway->coeffhigh_boun= 1000000000; 

}


void ori_coefficients(coefficient_s* bestcoefficients)
{
  int i;
  bestcoefficients -> bestcoeff1 = 0;
  bestcoefficients -> bestcoeff2 = 0;
  for (i = 0; i < 1000; i++)
  {
    bestcoefficients -> best_res_coeff1[i] = 0;
    bestcoefficients -> best_res_coeff2[i] = 0;
  }
}


void define_change (definechanging_t* changeway)
{
    printf("please type the casenum: 1 for coefficient * originalvalue, 0 for coefficient + originalvalue\n");
    scanf("%d", &changeway->casenum);

    printf("please type the coefficient\n");
    scanf("%lf", &changeway->coeff);
                   
    printf("please type the way you want to change coefficient, 1 for coeffi*changecoeff, 0 for coeffi+changecoeff\n");
    scanf("%d", &changeway->way);                      
                       
    printf("please type the value to change coefficient\n");
    scanf("%lf", &changeway->changecoeff);

    printf("please type the low  boundry of coefficient1\n");
    scanf("%lf", &changeway->coefflow_boun);
    printf("please type the high boundry of coefficient1\n");
    scanf("%lf", &changeway->coeffhigh_boun); 
}


void set_change_type (definechanging_t * cpuchange,definechanging_t * comchange )
{
 
  printf("please type the the value you want to change 1 for cpu value, 2 for com value, 3 for both, 4 for keeping square root ratio\n");
  scanf("%d", &cpu_com);
  
  if(cpu_com == 1 || cpu_com == 4 || cpu_com ==3)
    define_change (cpuchange);
  if (cpu_com == 2 || cpu_com ==3)
   define_change (comchange);

}


double coeffgenerator (double coeff, int way,double changecoeff)
{
  if(way == 1)             
        coeff = coeff * changecoeff;
  else 
        coeff = coeff + changecoeff;

  return coeff;
}


void pirntcoeff(coefficient_s* bestcoefficients)
{
  for (int j = 0; j < g; j ++)
  {
    printf("good resultcoeff1 %d:  %lf \n", j, bestcoefficients->best_res_coeff1[j]); 
    printf("good resultcoeff2 %d:  %lf \n", j, bestcoefficients->best_res_coeff2[j]);
    printf("..........................................\n");
    printf("                                        \n");
  }
}


// rewrite the profile with proper com values
 void changing_com(profile_t p, double coeff, int casenum, int nbres){
                        
        int i = 0;
        int j = 0;
        if(casenum == 0)
         { 
            printf(" coeff2 + \n");
            printf("best coeff2 %lf\n", coeff);
            for (i = 0; i < nbres; i++)
             {
                
                for(j = 0;j < nbres; j++)
                {
                    if (((s_msg_par_t *)(p->data))->com[i*nbres+j] > 0)
                    ((s_msg_par_t *)(p->data))->com[i*nbres+j] = ((s_msg_par_t *)(p->data))->com[i*nbres+j] + coeff;
                    printf("%d com: %lf\n",i,((s_msg_par_t *)(p->data))->com[i*nbres+j] );
                
                 }
             }
        }  
        else
        {
             printf(" coeff2 * \n");
             printf("best coeff2 %lf\n", coeff);
             for (i = 0; i < nbres; i++)
            {
              for(j = 0;j < nbres; j++)
                {
                    if (((s_msg_par_t *)(p->data))->com[i*nbres+j] > 0)
                        ((s_msg_par_t *)(p->data))->com[i*nbres+j] = ((s_msg_par_t *)(p->data))->com[i*nbres+j] * coeff;
                    printf("%d com: %lf\n",i,((s_msg_par_t *)(p->data))->com[i*nbres+j] );
                }
             }
         }
}


// rewrite the profile with proper cpu values
void changing_cpu(profile_t p, double coeff, int casenum, int nbres){
  int i = 0;
  int j = 0;
  double originalcpu;
  double ratio;
  double squa_ratio;
  if(casenum == 0)
  {
    printf("coefficient1 + \n");
    printf("coefficient1 %lf\n", coeff);
    for (i = 0; i < nbres; i++)
    {
       if (((s_msg_par_t *)(p->data))->cpu[i] != 0.000001)
       {
        originalcpu = ((s_msg_par_t *)(p->data))->cpu[i];
        ((s_msg_par_t *)(p->data))->cpu[i]  = ((s_msg_par_t *)(p->data))->cpu[i] + coeff;
        printf("%d cpu: %f\n",i, ((s_msg_par_t *)(p->data))->cpu[i] );
        
        }
        if(cpu_com == 4)
        {
          ratio = ((s_msg_par_t *)(p->data))->cpu[i] / originalcpu;
          squa_ratio = sqrt (ratio);
          printf("coefficient2 * \n");
          printf("coefficient2 [%d] %lf\n", i, squa_ratio);
          for(j = 0; j < nbres; j++)          
          { 
              if (((s_msg_par_t *)(p->data))->com[i*nbres+j] > 0)
                  ((s_msg_par_t *)(p->data))->com[i*nbres+j] = ((s_msg_par_t *)(p->data))->com[i*nbres+j] *squa_ratio;
                   printf("%d com: %lf\n ",i,((s_msg_par_t *)(p->data))->com[i*nbres+j]);
          }
        }
     }   

 }
  else
  {
    printf(" coefficient1 * \n");
    printf("coefficient1 %lf\n", coeff);
    for (i = 0; i < nbres; i++)
     {
        printf("..................................%d cpu: %f.................................\n",i, ((s_msg_par_t *)(p->data))->cpu[i] );
        if (((s_msg_par_t *)(p->data))->cpu[i]  != 0.000001)
         ((s_msg_par_t *)(p->data))->cpu[i]  = ((s_msg_par_t *)(p->data))->cpu[i]  * coeff;
         printf("%d cpu: %f\n",i, ((s_msg_par_t *)(p->data))->cpu[i] );
          }     
     }
}


// rewrite the profile with reset coefficient using reste operation
void redefine_profile(int nbres, profile_t p){
    
    int casenum;
    double coeff;
    printf("please type of changing of valies: 1 for coefficient * originalvalue, 0 for coefficient + originalvalue\n");
    scanf("%d", &casenum);

    printf("please type the coefficient\n");
    scanf("%lf", &coeff);

    printf("please type the the value you want to change 1 for cpu value, 2 for com value, 3 for both\n");
    scanf("%d", &cpu_com);

    if (cpu_com == 1 || cpu_com == 3 )
        changing_cpu(p,coeff, casenum, nbres);
    else if (cpu_com == 2 || cpu_com == 3)
        changing_com(p,coeff, casenum, nbres);
}


void change_profile(profile_t p, coefficient_s* bestcoefficients, int casenum1, int casenum2, int nbres)
{
  int change;

  printf("Whether reset the value, 2: change to the value you want, 1: change to the closest one, 0 no change \n");
  scanf("%d", &change);
  
    if (cpu_com == 1 || cpu_com == 3 || cpu_com == 4){  
      if(bestcoefficients->best_res_coeff1[g] > 0.000001){
        printf("We got the final cpu result\n");
        printf("best coefficient1 %lf\n", bestcoefficients->best_res_coeff1[g]);
        changing_cpu(p, bestcoefficients->best_res_coeff1[g], casenum1, nbres);
      }
      if (change == 1){
        changing_cpu(p, bestcoefficients->bestcoeff1, casenum1, nbres);
      }
    }
  if ( (cpu_com == 4 && casenum1 == 1) || cpu_com == 2 || cpu_com == 3){                       
       if(bestcoefficients->best_res_coeff2[g] > 0.000001){
         printf("We got the final com result\n");
         printf("best coefficient2 %lf\n", bestcoefficients->best_res_coeff2[g]);
         changing_com(p, bestcoefficients->best_res_coeff2[g], casenum2, nbres);
       }
       if (change == 1){
          changing_com(p, bestcoefficients->bestcoeff2, casenum2, nbres);
       }
    }
 if (change == 2)
        redefine_profile(nbres, p);
}


void printvalue(int m)
{
   int i, j =0;
   profile_t p = xbt_dict_get(profiles, "1");
   for (i = 0; i < m; i++)
      {
          printf("%d cpu: %f\n",i, ((s_msg_par_t *)(p->data))->cpu[i]);  
      }
    for (i = 0; i < m; i++)
        {
            for(j = 0;j < m; j++)
          {
            printf("%d com: %lf\n",i,((s_msg_par_t *)(p->data))->com[i*m+j]);
          }
      }
}


//choose whether continue coordinating the coefficient or not
void whether_continue(definechanging_t * cpuchange, definechanging_t * comchange)
{
  printf("please type the your decision about continuing 1 or not 0\n");
  scanf("%d", &choice);                     
   if(choice == 1)
  {
     ori_value(cpuchange);
     ori_value(comchange);        
     cpu_com = 5;
   }
}


int search_whether_repeated(char *sequence, char ** stored_seq, int length){
  int j = 0; 
  int i = 0;
  for (i = 0; i <= length-1; i++)
  { 
    if (strcmp(stored_seq[i], sequence) == 0)
    {
        j = 1;
        break;
    }
  }
  return j;
}


void change_different_profile(profile_t p, coefficient_s * bestcoefficients, int casenum1, int casenum2, int nb_res){
      if (strcmp(p->type, "msg_par") == 0)
        change_profile(p, bestcoefficients, casenum1, casenum2, nb_res); 
    
      else if (strcmp(p->type, "composed") == 0){
        int length = 0;
        char * stored_seq[1000];
        s_composed_prof_t * data = (s_composed_prof_t *) p -> data;
      
        int lg_seq = data -> lg_seq;
        char **seq = data -> seq;

        for (int j = 0; j < lg_seq; j++){
         profile_t p1 = xbt_dict_get(profiles, seq[j]);

            if (strcmp(p1->type, "delay") == 0)
              continue;

            else if(strcmp(p1->type, "msg_par") == 0)
            {  
               int num_judge = search_whether_repeated(seq[j], stored_seq, length);
               if (num_judge == 0){
                
                    stored_seq[length] = seq[j];
                    length = length +1 ;
                    change_profile(p1, bestcoefficients, casenum1, casenum2, nb_res); 
                 
                  }
            }
        }
      }
}


void adjust_coefficient(profile_t p, coefficient_s * bestcoefficients, definechanging_t * cpuchange, definechanging_t * comchange, int nb_res){
     double coeff1_original = 1;

     if(cpuchange->coeff == 1 && comchange->coeff == 1 )
            { 
                set_change_type(cpuchange, comchange);
                coeff1_original = cpuchange->coeff;
            }
            // only change cpu or com value
            if(cpu_com == 1 || cpu_com == 2){
                 if (cpu_com == 1){
                    cpuchange->coeff = coeffgenerator (cpuchange->coeff, cpuchange->way,cpuchange->changecoeff);
                  }
                 if (cpu_com == 2){
                    comchange ->coeff = coeffgenerator (comchange ->coeff, comchange ->way,comchange ->changecoeff);
                 }
                 if(cpuchange->coeff < cpuchange->coefflow_boun ||
                   cpuchange->coeff > cpuchange->coeffhigh_boun ||
                   comchange ->coeff < comchange->coefflow_boun ||
                   comchange ->coeff > comchange->coeffhigh_boun)
                   {
                      change_different_profile(p, bestcoefficients, cpuchange->casenum, comchange->casenum, nb_res);
                      ori_coefficients(bestcoefficients);
                      whether_continue(cpuchange,comchange);                     
                     }
                 }
               else if(cpu_com == 3){
                   cpuchange->coeff  = coeffgenerator (cpuchange->coeff, cpuchange->way,cpuchange->changecoeff);
                   if (cpuchange->coeff < cpuchange->coefflow_boun || cpuchange->coeff >= cpuchange->coeffhigh_boun)
                    {
                         cpuchange->coeff = coeff1_original;
                         comchange->coeff= coeffgenerator (comchange ->coeff, comchange ->way,comchange ->changecoeff);
                    }
                    if (comchange ->coeff < comchange->coefflow_boun || comchange->coeff > comchange->coeffhigh_boun)
                    {
                          
                          change_different_profile( p, bestcoefficients, cpuchange->casenum, comchange->casenum, nb_res); 
                          ori_coefficients(bestcoefficients);                   
                          whether_continue(cpuchange,comchange);                                         
                       }
               }
              else if(cpu_com == 4){
                  cpuchange->coeff = coeffgenerator (cpuchange->coeff, cpuchange->way,cpuchange->changecoeff);
                  comchange->casenum = 1;
                  if(cpuchange->coeff >= 0 && cpuchange->casenum == 1)
                        comchange->coeff = sqrt(cpuchange->coeff);
                  if (cpuchange->coeff < cpuchange->coefflow_boun || 
                      cpuchange->coeff > cpuchange->coeffhigh_boun)
                    
                      {
                        
                         change_different_profile( p, bestcoefficients, cpuchange->casenum, comchange->casenum, nb_res);              
                        
                          
                          ori_coefficients(bestcoefficients);
                          whether_continue(cpuchange,comchange);            
                      }
              }

}


void record_bestcoefficient(double result, double presenttime, coefficient_s * bestcoefficients, double cpu_coeff, double com_coeff)
{
        double a = fabs(presenttime - result);
        double b = fabs(bestcoefficients->bestresult - result);
        if(a <= b)
        {
            printf("updating best results\n");
            bestcoefficients->bestcoeff1 = cpu_coeff;
            bestcoefficients->bestcoeff2 = com_coeff;
            bestcoefficients->bestresult = presenttime;
            if (b < 0.000001 && g<1000)
            {
                bestcoefficients->best_res_coeff1[g] =  bestcoefficients->bestcoeff1;
                bestcoefficients->best_res_coeff2[g] =  bestcoefficients->bestcoeff2;
                g++;                
            }
        }
       printf("bestcoeff1:%lf, bestcoeff2:%lf,bestresult: %lf target-result %lf\n",bestcoefficients->bestcoeff1,bestcoefficients->bestcoeff2,bestcoefficients->bestresult,result);  
}


void find_coefficient(double result, s_job_t * job, int * res_idxs, coefficient_s * bestcoefficients, definechanging_t * cpuchange, definechanging_t * comchange)
{       
        
        double presenttime = 0;
        profile_t p = xbt_dict_get(profiles, job->profile);
        do
        {      
            printf("enter changing\n");
            printvalue(job->nb_res);
            // if there is no change, we set the original value 
            adjust_coefficient(p, bestcoefficients, cpuchange, comchange, job->nb_res);
            if(choice == 1)
            {
               choice = 5;
               continue;             
            }
            else if(choice == 0)
                break;
            double preresult = MSG_get_clock(); 
            job_exec1(job->id, job->nb_res, res_idxs, nodes, job->walltime,cpuchange->coeff, cpuchange->casenum, comchange->coeff, comchange->casenum, cpu_com);
            presenttime = MSG_get_clock() - preresult;
            XBT_INFO("Job id %d, job simulation time: %f", job->id,  presenttime); 
            record_bestcoefficient(result, presenttime, bestcoefficients, cpuchange->coeff, comchange->coeff);
        }while(presenttime != result);
        record_bestcoefficient(result, presenttime, bestcoefficients, cpuchange->coeff, comchange->coeff); 
}


/**
 * \brief Execute jobs alone
 *
 * Execute jobs sequentially without server and scheduler
 *
 */


static int job_launcher(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    coefficient_s * bestcoefficients = (coefficient_s *) malloc(sizeof(coefficient_s));
    definechanging_t * cpuchange = (definechanging_t *)malloc(sizeof(definechanging_t)); 
    definechanging_t * comchange = (definechanging_t *)malloc(sizeof(definechanging_t)); 
    ori_value(cpuchange);
    ori_value(comchange);
    unsigned int job_index;
    s_job_t * job;
    
    xbt_dynar_foreach(jobs_dynar, job_index, job)
    {   
        double result = 0.0000;

        printf("please type the result\n");
        scanf("%lf", &result);
        
        double t = MSG_get_clock();
        int * res_idxs = (int *) malloc((job->nb_res) * sizeof(int));
        for (int i = 0; i < job->nb_res; i++)
            res_idxs[i] = i;
        job_exec(job->id, job->nb_res, res_idxs, nodes, job->walltime);
        XBT_INFO("Job id %d, job simulation time: %lf", job->id,  MSG_get_clock() - t);
        
        bestcoefficients->bestresult = MSG_get_clock() - t;
        
        if ((MSG_get_clock() - t) != result){
           find_coefficient(result, job, res_idxs, bestcoefficients, cpuchange, comchange);
    
        }
        free(res_idxs);
        free (cpuchange);
        free (comchange);  
        free(bestcoefficients);
    }
    return 0;
}


msg_error_t deploy_all(const char *platform_file)
{
    msg_error_t res = MSG_OK;
    xbt_dynar_t all_hosts;
    msg_host_t first_host;

    MSG_config("workstation/model", "ptask_L07");
    MSG_create_environment(platform_file);

    all_hosts = MSG_hosts_as_dynar();
    xbt_dynar_get_cpy(all_hosts, 0, &first_host);
    //first_host = xbt_dynar_getfirst_as(all_hosts,msg_host_t);
    xbt_dynar_remove_at(all_hosts, 0, NULL);
    //todo: remove the good (master) host, not the first one 

    nb_nodes = xbt_dynar_length(all_hosts);
    nodes = xbt_dynar_to_array(all_hosts);
    //xbt_dynar_free(&all_hosts);

    XBT_INFO("Nb nodes: %d", nb_nodes);

    MSG_process_create("job_launcher", job_launcher, NULL, first_host);

    res = MSG_main();

    XBT_INFO("Simulation time %g", MSG_get_clock());

    return res;
}


int main(int argc, char *argv[])
{
    msg_error_t res = MSG_OK;
    json_t *json_workload_profile;
    //Comment to remove debug message
    xbt_log_control_set("batexec.threshold:debug");

    if (argc < 2)
    {
        printf("Batexec: execute a list of jobs in FIFO.\n");
        printf("Resources are assigned from 0, only one job is running at a time\n");
        printf("\n");
        printf("Usage: %s platform_file workload_file\n", argv[0]);
        printf("example: %s ../platforms/small_platform.xml ../workload_profiles/test_workload_profile.json\n", argv[0]);
        exit(1);
    }

    json_workload_profile = load_json_workload_profile(argv[2]);
    retrieve_jobs(json_workload_profile);
    retrieve_profiles(json_workload_profile);
    MSG_init(&argc, argv);
    res = deploy_all(argv[1]);
    if (res == MSG_OK)
        return 0;
    else
        return 1;

}
