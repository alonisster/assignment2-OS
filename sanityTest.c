#include "types.h"
#include "stat.h"
#include "user.h"
#include "x86.h"

#define SIG_TEST 5
volatile int number =0;
int counter =0;
void dummyUserHandler(int signum){
    number=1;
}

void test_sigaction(){
  printf(1,"test_sigaction starting:\n");
  struct sigaction old_sigaction;
  struct sigaction new_sigaction;

  old_sigaction.sa_handler = (void*)1;
  old_sigaction.sigmask=1;

  new_sigaction.sa_handler = (void*)3;
  new_sigaction.sigmask=2;

  if(sigaction(20, &new_sigaction, &old_sigaction)<0)
    printf(1, "sigaction failed\n");
  else{
    struct sigaction dummy;
    sigaction(20, &old_sigaction, &dummy);
    if(dummy.sa_handler != new_sigaction.sa_handler||dummy.sigmask!= new_sigaction.sigmask)
      printf(2, "sanity failed: didnt update oldact properly.\n");
    else
      printf(1, "test_sigaction OK\n");
  }
}


void test_user_handler(){
    printf(1, "\n\ntest user signal handler starts\n");
    struct sigaction dummy ;
    dummy.sa_handler = dummyUserHandler;
    dummy.sigmask = 0;
    struct sigaction dummyTwo;
    if(sigaction(SIG_TEST, &dummy, &dummyTwo)<0){
        printf(1,"problem in sigaction\n");
    }
    kill(getpid(),SIG_TEST);
    if(number == 1){
        printf(1, "Test succeed: updated successfuly\n");
    }else{
        printf(1, "Test Failed: user handler did not executed\n");
    }
} 


void test_kill_sig(){
  printf(1,"test_kill_sig starting:\n");
    int pid = fork();
    if(pid == 0){
        while(1){}
    }else{
        kill(pid, SIGKILL);
        wait();
        //another check when handling signal without setting handler- should be treated as kill
        int pid2 = fork();
        if(pid2 == 0){
            while(1){}
        }else{
            kill(pid2, 2);
            wait();
            printf(1, "test_kill_sig OK\n");
        }
    }
    
}
void test_stop_cont(){

}

void test_sigprocmask_and_inherit_fork_mask(){
  printf(1,"test_sigprocmask_and_inherit_fork_mask starting:\n");
  
  sigprocmask(20);
  uint old_mask=0;
  int pid=fork();
  if(pid==0){
    old_mask=sigprocmask(0);
    if(old_mask!=20)
      printf(1,"inherit_fork_mask failed\n");
    exit();
  }
  else
  {
    wait();
    old_mask=sigprocmask(0);
    if(old_mask!=20){
      printf(1,"sigprocmask failed\n");
      return;
    }
  
  }
  printf(1,"test_sigprocmask_and_inherit_fork_mask OK:\n");  
}

void test_cas(){
  printf(1,"cas test starting:\n");
  int old=0;
  do{
      old = counter;
    } while (!cas(&counter,old,old+1));
  if(counter==1){
    printf(1,"cas test OK:\n");
  }
  else{
    printf(1, "cas test failed counter is:%d\n",counter );
  }
}


int main(){
    test_sigaction();
    test_kill_sig();
    test_sigprocmask_and_inherit_fork_mask();
    test_cas();
    test_user_handler();
    exit();
}