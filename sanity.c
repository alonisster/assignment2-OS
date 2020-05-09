
#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"


void dummyUserHandler(int signum){
  printf(1, "user signal handler active- test passed\n");
  return;
}

void check_sigaction(){
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



void sanityCheck(){
  check_sigaction();
}

// void dummyUserHandler(int signum){
//   printf(1, "user signal handler active- test passed\n");
//   return;
// }

// void check_sigaction_user_handler(){
//   struct sigaction dummy ;
//   struct sigaction old_sigaction;// = malloc(sizeof(struct sigaction));
//   old_sigaction.sa_handler = (void*)1;
//   dummy.sa_handler = dummyUserHandler;
//   dummy.sigmask = 0;
//   sigaction(3, &dummy, &old_sigaction);
//   if(old_sigaction.sa_handler == (void*)1){
//     printf(1, "sanity checks failed: didnt update oldact.\n");
//   }else{
//     printf(1, "sanity check: updated oldact successfully.\n");
//     struct sigaction dummyTwo;
//     sigaction(3, &old_sigaction, &dummyTwo);
//     if(dummyTwo.sa_handler != dummy.sa_handler){
//       printf(1, "sanity failed: didnt update oldact properly.\n");
//     }else{
//       printf(1, "passed\n");
//     }
//   }

//   sigaction(3, &dummy, 0);
//   kill(getpid(), 3);  
// }

void test_kill(){
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
            printf(1, "test kill passed\n");
        }
    }
    
}

void test_stop_cont(){

}

void test_inherit_fork_mask(){

}

// void test_cas(){
//     int old;
//     int counter=0;
//     do{
//         old = counter;
       
//     } while (cas(&counter,old,1) != 0);
//      printf(1,"changed %d\n", counter);
// }


int main(){
    printf(1, "usertests starting\n");

//   if(open("usertests.ran", 0) >= 0){
//     printf(1, "already ran user tests -- rebuild fs.img\n");
//     exit();
//   }
//   close(open("usertests.ran", O_CREATE));
    // sanityCheck();
    // check_sigaction_user_handler();
    // test_kill();

    // test_cas();
    
    exit();
}