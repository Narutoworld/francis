/* 
 * stoplight.c
 *
 * 31-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
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
 * Number of cars created.
 */

#define NCARS 20



struct semaphore *NW;
struct semaphore *SW;
struct semaphore *SE;
struct semaphore *NE;
struct semaphore *check_available;

/*
 *
 * Function Definitions
 *
 */

static const char *directions[] = { "N", "E", "S", "W" };

static const char *msgs[] = {
        "approaching:",
        "region1:    ",
        "region2:    ",
        "region3:    ",
        "leaving:    "
};

/* use these constants for the first parameter of message */
enum { APPROACHING, REGION1, REGION2, REGION3, LEAVING };

static void
message(int msg_nr, int carnumber, int cardirection, int destdirection)
{
        kprintf("%s car = %2d, direction = %s, destination = %s\n",
                msgs[msg_nr], carnumber,
                directions[cardirection], directions[destdirection]);
}
 
/*
 * gostraight()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement passing straight through the
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
gostraight(unsigned long cardirection,
           unsigned long carnumber)
{
		//firstly, we need check if there are already three cars in the intersection
		P(check_available);
        //approach the intersection
		if(cardirection == 0){
			message(0,carnumber,cardirection,2);
		}
		else if(cardirection == 1){
			message(0,carnumber,cardirection,3);
		}
		else if(cardirection == 2){
			message(0,carnumber,cardirection,0);
		}
		else if(cardirection == 3){
			message(0,carnumber,cardirection,1);
		}
		
		//entering region 1
		if(cardirection == 0){			
			P(NW);
			message(1,carnumber,cardirection,2);
		}	
		else if(cardirection == 1){
			P(NE);
			message(1,carnumber,cardirection,3);
		}
		else if(cardirection == 2){
			P(SE);
			message(1,carnumber,cardirection,0);
		}
		else if(cardirection == 3){
			P(SW);
			message(1,carnumber,cardirection,1);
		}

		//entering region 2
		if(cardirection == 0){
			P(SW);
			message(2,carnumber,cardirection,2);
			V(NW);
		}	
		else if(cardirection == 1){
			P(NW);
			message(2,carnumber,cardirection,3);
			V(NE);
		}
		else if(cardirection == 2){
			P(NE);
			message(2,carnumber,cardirection,0);
			V(SE);
		}
		else if(cardirection == 3){
			P(SE);
			message(2,carnumber,cardirection,1);
			V(SW);
		}
		
		//leaving the intersection
		if(cardirection == 0){
			message(4,carnumber,cardirection,2);
			V(SW);
			V(check_available);
		}
		else if(cardirection == 1){
			message(4,carnumber,cardirection,3);
			V(NW);
			V(check_available);
		}
		else if(cardirection == 2){
			message(4,carnumber,cardirection,0);
			V(NE);
			V(check_available);
		}
		else if(cardirection == 3){
			message(4,carnumber,cardirection,1);
			V(SE);
			V(check_available);
		}
        
		return;
}


/*
 * turnleft()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a left turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnleft(unsigned long cardirection,
         unsigned long carnumber)
{       
		//firstly, we need check if there are already three cars in the intersection
		P(check_available);
		//approach the intersection
		if(cardirection == 0){
			message(0,carnumber,cardirection,1);
		}
		else if(cardirection == 1){
			message(0,carnumber,cardirection,2);
		}
		else if(cardirection == 2){
			message(0,carnumber,cardirection,3);
		}
		else if(cardirection == 3){
			message(0,carnumber,cardirection,0);
		}
		
		//entering region 1
		if(cardirection == 0){
			P(NW);
			message(1,carnumber,cardirection,1);
		}	
		else if(cardirection == 1){
			P(NE);
			message(1,carnumber,cardirection,2);
		}
		else if(cardirection == 2){
			P(SE);
			message(1,carnumber,cardirection,3);
		}
		else if(cardirection == 3){
			P(SW);
			message(1,carnumber,cardirection,0);
		}
		//entering region 2
		if(cardirection == 0){
			P(SW);
			message(2,carnumber,cardirection,1);
			V(NW);
		}	
		else if(cardirection == 1){
			P(NW);
			message(2,carnumber,cardirection,2);
			V(NE);
		}
		else if(cardirection == 2){
			P(NE);
			message(2,carnumber,cardirection,3);
			V(SE);
		}
		else if(cardirection == 3){
			P(SE);
			message(2,carnumber,cardirection,0);
			V(SW);
		}
		//entering region 3
		if(cardirection == 0){
			P(SE);
			message(3,carnumber,cardirection,1);
			V(SW);
		}	
		else if(cardirection == 1){
			P(SW);
			V(NW);
			message(3,carnumber,cardirection,2);
		}
		else if(cardirection == 2){
			P(NW);
			V(NE);
			message(3,carnumber,cardirection,3);
		}
		else if(cardirection == 3){
			P(NE);
			message(3,carnumber,cardirection,0);
			V(SE);
		}
		//leaving the intersection
		if(cardirection == 0){
			message(4,carnumber,cardirection,1);
			V(SE);
			V(check_available);
		}
		else if(cardirection == 1){
			message(4,carnumber,cardirection,2);
			V(SW);
			V(check_available);
		}
		else if(cardirection == 2){
			message(4,carnumber,cardirection,3);
			V(NW);
			V(check_available);
		}
		else if(cardirection == 3){
			message(4,carnumber,cardirection,0);
			V(NE);
			V(check_available);
		}
        return;
}


/*
 * turnright()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a right turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnright(unsigned long cardirection,
          unsigned long carnumber)
{
	//firstly, we need check if there are already three cars in the intersection
	P(check_available);
	//approach the intersection
	if(cardirection == 0){
		message(0,carnumber,cardirection,3);
	}
	else if(cardirection == 1){
		message(0,carnumber,cardirection,0);
	}
	else if(cardirection == 2){
		message(0,carnumber,cardirection,1);
	}
	else if(cardirection == 3){
		message(0,carnumber,cardirection,2);
	}
	
	//Entering the intersection,region 1
	if(cardirection == 0){
		P(NW);
		message(1,carnumber,cardirection,3);
	}
	else if(cardirection == 1){
		P(NE);
		message(1,carnumber,cardirection,0);
	}
	else if(cardirection == 2){
		P(SE);
		message(1,carnumber,cardirection,1);
	}
	else if(cardirection == 3){
		P(SW);
		message(1,carnumber,cardirection,2);
	}

	//leaving the intersection
	if(cardirection == 0){
		message(4,carnumber,cardirection,3);
		V(NW);
		V(check_available);
	}
	else if(cardirection == 1){
		message(4,carnumber,cardirection,0);
		V(NE);
		V(check_available);
	}
	else if(cardirection == 2){
		message(4,carnumber,cardirection,1);
		V(SE);
		V(check_available);
	}
	else if(cardirection == 3){
		message(4,carnumber,cardirection,2);
		V(SW);
		V(check_available);
	}

	return;

}


/*
 * approachintersection()
 *
 * Arguments: 
 *      void * unusedpointer: currently unused.
 *      unsigned long carnumber: holds car id number.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Change this function as necessary to implement your solution. These
 *      threads are created by createcars().  Each one must choose a direction
 *      randomly, approach the intersection, choose a turn randomly, and then
 *      complete that turn.  The code to choose a direction randomly is
 *      provided, the rest is left to you to implement.  Making a turn
 *      or going straight should be done by calling one of the functions
 *      above.
 */
 
static
void
approachintersection(void * unusedpointer,
                     unsigned long carnumber)
{
        int cardirection;
		int turndirection;

        /*
         * Avoid unused variable and function warnings.
         */

        (void) unusedpointer;
        (void) carnumber;
		(void) gostraight;
		(void) turnleft;
		(void) turnright;
	
        /*
         * cardirection is set randomly.
         */

        cardirection = random() % 4;
		turndirection = random() % 3;
		if(turndirection == 0){gostraight(cardirection,carnumber);}
		else if(turndirection == 1){turnleft(cardirection,carnumber);}
		else if(turndirection == 2){turnright(cardirection,carnumber);}
		//gostraight(cardirection,carnumber);
		//turnright(cardirection,carnumber);
}


/*
 * createcars()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up the approachintersection() threads.  You are
 *      free to modiy this code as necessary for your solution.
 */

int
createcars(int nargs,
           char ** args)
{

        int index, error;
		NW = sem_create("none",1);
		NE = sem_create("none",1);
		SW = sem_create("none",1);
		SE = sem_create("none",1);
		check_available = sem_create("none",3);
        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;

        /*
         * Start NCARS approachintersection() threads.
         */

        for (index = 0; index < NCARS; index++) {

                error = thread_fork("approachintersection thread",
                                    NULL,
                                    index,
                                    approachintersection,
                                    NULL
                                    );

                /*
                 * panic() on error.
                 */

                if (error) {
                        
                        panic("approachintersection: thread_fork failed: %s\n",
                              strerror(error)
                              );
                }
        }

        return 0;
}
