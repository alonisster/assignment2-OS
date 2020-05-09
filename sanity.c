
#include "types.h"
#include "stat.h"
#include "user.h"

void check_sigaction();


void dummyUserHandler(int signum){
  printf(1, "user signal handler active- test passed\n");
  return;
}

void check_sigaction_user_handler(){
  struct sigaction dummy ;
  struct sigaction old_sigaction;// = malloc(sizeof(struct sigaction));
  old_sigaction.sa_handler = (void*)1;
  dummy.sa_handler = dummyUserHandler;
  dummy.sigmask = 0;
  sigaction(3, &dummy, &old_sigaction);
  if(old_sigaction.sa_handler == (void*)1){
    printf(1, "sanity checks failed: didnt update oldact.\n");
  }else{
    printf(1, "sanity check: updated oldact successfully.\n");
    struct sigaction dummyTwo;
    sigaction(3, &old_sigaction, &dummyTwo);
    if(dummyTwo.sa_handler != dummy.sa_handler){
      printf(1, "sanity failed: didnt update oldact properly.\n");
    }else{
      printf(1, "passed\n");
    }
  }

  sigaction(3, &dummy, 0);
  kill(getpid(), 3);  
}



int main(){
    check_sigaction_user_handler();
    exit();
}