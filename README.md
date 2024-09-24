Strange Attractors by
David Gee
========================================
**Strange Attractors is a program which visualizes Ordinary Differential Equations (ODE's), specifically the Lorentz Attractor, using the VRUI 12.0-001 toolkit.**

**Overview**
This program creates a particle system that can visualize any ODE with the known step functions. The program randomly seeds particles in the Vertex Buffer and decays particles over time, fading them to nothing and soon freeing the particles from the vector. New particles can be seeded after runtime and is added to the original vector through the Ring Buffer. The Triple Buffer allows fluid motion of updating and displaying the Vertex Buffer. 

**What I learned**
I have completed this research project in Summer 2024 under Oliver Kreylos in the DataLabs at UC Davis. I have learned how to communicate between foreground and background thread, how triple buffers work, how to troubleshoot an extensive look to pointers within C++, how to implement ring buffers, how to add birth/death rate to particles, how to add particles after initialization, and so much more. 

StrangeAttractors.cpp is the most up-to-date file. The other files show progression of the Strange Attractors program, starting with the earliest stage StrangeAttractors1, then StrangeAttractorsArray, StrangeAttractorsInputP1, StrangeAttractorsInputP2, and lastly the most up-to-date program StrangeAttractors. 

I could not have done this work without my mentor Oliver Kreylos along with his VRUI-12.0-001 toolkit. 

**Installation**
 - Please download the VRUI-12.0-001 toolkit by typing this in the terminal: wget -O - http://vroom.library.ucdavis.edu/Software/Vrui/Vrui-12.0-001.tar.gz | tar xfz
 - Download the StrangeAttractors.cpp file and the makefile.
This function in the terminal should download all the files needed for the files within *insert folder name here*
make PACKAGES=VRUIALL *insert folder name here*
 - Lastly, this command should compile and run the StrangeAttractors file:
  make StrangeAttractors && ./bin/StrangeAttractors

