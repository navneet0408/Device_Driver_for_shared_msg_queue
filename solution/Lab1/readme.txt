1. We will need to reach the working directory named “Lab1”.
2. Typing “make” while in the working directory builds the application for the Galileo board. We can copy the kernel module “Squeue.ko” and the application “main” to the Galileo board and run it there.
3. Typing “make local” while in the working directory will build the kernel module and application for the local machine for testing.  
4. To insert the kernel module, we need to type “sudo insmod Squeue.ko”. 