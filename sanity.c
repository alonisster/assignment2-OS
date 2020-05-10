// #include "types.h"
// #include "stat.h"
// #include "user.h"


// void dummyUserHandler(int signum){
// //   printf(1, "user signal handler active- test passed\n");
//   return;
// }

// void check_sigaction(){
//   struct sigaction dummy ;
//   struct sigaction old_sigaction;// = malloc(sizeof(struct sigaction));
//   old_sigaction.sa_handler = (void*)1;
//   dummy.sa_handler = dummyUserHandler;
//   dummy.sigmask = 0;
//   printf(1,"process id %d\n", (int)getpid());
//   sigaction(3, &dummy, &old_sigaction);
//   if(old_sigaction.sa_handler == (void*)1){
//     printf(2, "sanity checks failed: didnt update oldact.\n");
//   }else{
//     printf(2, "sanity check: updated oldact successfully.\n");
//     // sleep(50);
//     struct sigaction dummyTwo;
//     sigaction(3, &old_sigaction, &dummyTwo);
//     if(dummyTwo.sa_handler != dummy.sa_handler){
//       printf(2, "sanity failed: didnt update oldact properly.\n");
//         // sleep(50);
//     }else{
//     //   printf(2, "passed\n");
//         // sleep(50);

//     }
//   }

//   sigaction(3, &dummy, 0);
//   kill(getpid(), 3);  
// }



// void sanityCheck(){
//   check_sigaction();
// }

// // void dummyUserHandler(int signum){
// //   printf(1, "user signal handler active- test passed\n");
// //   return;
// // }

// // void check_sigaction_user_handler(){
// //   struct sigaction dummy ;
// //   struct sigaction old_sigaction;// = malloc(sizeof(struct sigaction));
// //   old_sigaction.sa_handler = (void*)1;
// //   dummy.sa_handler = dummyUserHandler;
// //   dummy.sigmask = 0;
// //   sigaction(3, &dummy, &old_sigaction);
// //   if(old_sigaction.sa_handler == (void*)1){
// //     printf(1, "sanity checks failed: didnt update oldact.\n");
// //   }else{
// //     printf(1, "sanity check: updated oldact successfully.\n");
// //     struct sigaction dummyTwo;
// //     sigaction(3, &old_sigaction, &dummyTwo);
// //     if(dummyTwo.sa_handler != dummy.sa_handler){
// //       printf(1, "sanity failed: didnt update oldact properly.\n");
// //     }else{
// //       printf(1, "passed\n");
// //     }
// //   }

// //   sigaction(3, &dummy, 0);
// //   kill(getpid(), 3);  
// // } 

// void test_kill(){
//     int pid = fork();
//     if(pid == 0){
//         while(1){}
//     }else{
//         kill(pid, SIGKILL);
//         wait();

//         //another check when handling signal without setting handler- should be treated as kill
//         int pid2 = fork();
//         if(pid2 == 0){
//             while(1){}
//         }else{
//             kill(pid2, 2);
//             wait();
//             printf(1, "test kill passed\n");
//         }
//     }
    
// }

// void test_stop_cont(){

// }

// void test_inherit_fork_mask(){

// }

// // void test_cas(){
// //     int old;
// //     int counter=0;
// //     do{
// //         old = counter;
       
// //     } while (cas(&counter,old,1) != 0);
// //      printf(1,"changed %d\n", counter);
// // }


// int main(){
//     // printf(1, "usertests starting\n");
//     // sleep(50);


//     // if(open("usertests.ran", 0) >= 0){
//     // printf(1, "already ran user tests -- rebuild fs.img\n");
//     // exit();
//     // }
//     // close(open("usertests.ran", O_CREATE));
//     sanityCheck();
//     // check_sigaction_user_handler();
//     // test_kill();

//     // test_cas();
    
//     exit();
// }



#include "types.h"
#include "stat.h"
#include "user.h"
#define SIG_TEST 7
volatile int keep_running = 1;

void
callback_for_SIG_TEST(int signum){
  char c = '@'+signum;
  c-=SIG_TEST;
  // char c = '@';
  write(1, &c, 1);
  keep_running = 0;
}

int
main(int argc, char *argv[])
{
  struct sigaction sigaction_for_SIG_TEST = {.sa_handler = callback_for_SIG_TEST, .sigmask = 0};
  struct sigaction old_sigaction;
  int wait_ret_value;
  if( sigaction( SIG_TEST, &sigaction_for_SIG_TEST, &old_sigaction ) < 0){
    printf(1, "error in sigaction!\n");
  }
  int number_of_iterations = 100, iter_num = 0;
  int pid = fork();
  if (pid < 0){
    printf(1, "error in fork!\n");
    exit();
    exit();
  } else if (pid == 0){ //child
    while(keep_running && iter_num < number_of_iterations){
      printf(1, "childID %d is running, waiting for SIG_TEST.\n", pid);
      iter_num++;
    }
    if (iter_num < number_of_iterations){
      printf(1, "childID %d got signal SIG_TEST!!\n", pid);
    } else {
      printf(1, "child exists without getting signal SIG_TEST :(  :(  \n");
    }
    exit();
  } else { //parent
    for(int i = 0; i < number_of_iterations/10; i++){
      printf(1, "pid:%d, parent iteration no. : %d\n", getpid(), i);
    }
    printf(1, "parentID send signal SIG_TEST to child\n", getpid());
    if (kill(pid, SIG_TEST) < 0 ){
      printf(1, "error in kill syscall\n");
    }
    printf(1, "parent waits to the child to exit\n");
    wait_ret_value = wait();

    printf(1, "parent's pid is : %d, child's pid is: %d, wait returned value is pid no. : %d\n", getpid(), pid, wait_ret_value );
    printf(1, "parent exits\n");
  }
  exit();
}