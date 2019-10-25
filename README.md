This repo contains sample projects I have developed on the NXP K65F MCU.
The K65F uses an ARM Cortex M4 core and all code is developed in baremetal C.

In these projects I use some external modules which I do not have included in
this remote repository.  One of these modules is BasicIO written by Professor Todd Morton
at Western Washington University.  If there are questions about this module, or any others 
used in these projects, please send me an email or raise an issue.

In several cases, I created a project where I implemented some functionality that was later 
converted into its own module.  To distinguish between these two cases I create two seperate 
directories.  One called *Main, the other called *Mod.  As an example, my first project implementing
a simple check-sum on a user inputted block of memory has two directories associated to it.
The "CheckSumMain" directory contains the code I wrote to initially implement the check-sum
functionality.  I later converted the code in "CheckSumMain" into its own module to be included
and used in future projects.  The checksum module code is contained in "CheckSumMod."
