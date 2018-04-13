/*
 * catlock.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use LOCKS/CV'S to solve the cat syncronization problem in 
 * this file.
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>

/*
 * 
 * Constants
 *
 */

/*
 * Number of food bowls.
 */

#define NFOODBOWLS 2

/*
 * Number of cats.
 */

#define NCATS 6

/*
 * Number of mice.
 */

#define NMICE 2


/*
 * 
 * Function Definitions
 * 
 */

/* who should be "cat" or "mouse" */
int eating_animals = 0;
struct lock *lock_bowl[2];
struct lock *lock_animal;
struct lock *lock_bowl_check;
int bowl[2];
char animal;
static void
lock_eat(const char *who, int num, int bowl, int iteration)
{
        kprintf("%s: %d starts eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
        clocksleep(1);
        kprintf("%s: %d ends eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
}

/*
 * catlock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS -
 *      1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
catlock(void * unusedpointer, 
        unsigned long catnumber)
{
	int i;
	for (i = 0; i < 4;){
//4 eating iterations
		lock_acquire(lock_animal);
//avoid not see other animals are eating
		if(animal != 'm'){
//if mouse is not eating
			if (animal == 'c')
				lock_release(lock_animal);
			else if (animal == 'n'){
				animal = 'c';
				lock_release(lock_animal);
				}
			int j;
			for (j = 0; j < NFOODBOWLS; j++){
//2 bowls in total
				lock_acquire(lock_bowl_check);
//avoid others find out the bowl is not being used at the same time
				if (bowl[j] == 0){
//if bowl is not being used
					lock_acquire(lock_bowl[j]);
//eating
					bowl[j] = 1;
					lock_release(lock_bowl_check);
					lock_acquire(lock_animal);
					eating_animals++;
					lock_release(lock_animal);
					lock_eat("cat", catnumber, j+1, i);
					lock_acquire(lock_animal);
					lock_release(lock_bowl[j]);
					bowl[j]=0;
					eating_animals--;
					if (eating_animals == 0)
						animal ='n';
					lock_release(lock_animal);
					i++;
//ready for next eating iteration
					break;
//leave after eating
				} else 
					lock_release(lock_bowl_check);
			}
		} else
			lock_release(lock_animal);
	}

        /*
         * Avoid unused variable warnings.
         */

        (void) unusedpointer;
}
	

/*
 * mouselock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds the mouse identifier from 0 to 
 *              NMICE - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
mouselock(void * unusedpointer,
          unsigned long mousenumber)
{	
	int i;
	for (i = 0; i < 4;){
//4 eating iterations
		lock_acquire(lock_animal);
//avoid not see other animals are eating
		if(animal != 'c'){
//if cat is not eating
			if (animal == 'm')
				lock_release(lock_animal);
			if (animal == 'n'){
				animal = 'm';
				lock_release(lock_animal);
			}
			int j;
			for (j = 0; j < NFOODBOWLS; j++){
//2 bowls in total
				lock_acquire(lock_bowl_check);
//avoid others find out the bowl is not being used at the same time
				if (bowl[j] == 0){
//if bowl is not being used
					lock_acquire(lock_bowl[j]);
//eating
					bowl[j] = 1;
					lock_release(lock_bowl_check);
					lock_acquire(lock_animal);
					eating_animals++;
					lock_release(lock_animal);
					lock_eat("mouse", mousenumber, j+1, i);
					lock_acquire(lock_animal);
					lock_release(lock_bowl[j]);
					bowl[j] = 0;
					eating_animals--;
					if (eating_animals == 0)
						animal = 'n';
					lock_release(lock_animal);
					i++;
//ready for next eating iteration
					break;
//leave after eating
				} else 
					lock_release(lock_bowl_check);
			}
		} else
			lock_release(lock_animal);
	}
        /*
         * Avoid unused variable warnings.
         */
        
        (void) unusedpointer;
}


/*
 * catmouselock()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catlock() and mouselock() threads.  Change
 *      this code as necessary for your solution.
 */

int
catmouselock(int nargs,
             char ** args)
{
        int index, error;
	lock_animal = lock_create("none");
        lock_bowl[0] = lock_create("first");
        lock_bowl[1] = lock_create("second");
	lock_bowl_check = lock_create("bowl_check");
	bowl[1]=0;
	bowl[0]=0;
        /*
         * Avoid unused variable warnings.
         */
	animal = 'n';
        (void) nargs;
        (void) args;
   
        /*
         * Start NCATS catlock() threads.
         */

        for (index = 0; index < NCATS; index++) {
                error = thread_fork("catlock thread", 
                                    NULL, 
                                    index, 
                                    catlock, 
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
                 
                        panic("catlock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }

        /*
         * Start NMICE mouselock() threads.
         */

        for (index = 0; index < NMICE; index++) {
   
                error = thread_fork("mouselock thread", 
                                    NULL, 
                                    index, 
                                    mouselock, 
                                    NULL
                                    );
      
                /*
                 * panic() on error.
                 */

                if (error) {
         
                        panic("mouselock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }

	lock_destroy(lock_animal);
        lock_destroy(lock_bowl[0]);
        lock_destroy(lock_bowl[1]);
lock_destroy(lock_bowl_check);
        return 0;
}

/*
 * End of catlock.c
 */
