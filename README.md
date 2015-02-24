scope
------
This is a modified ns-3 version which supports energy efficient ethernet (EEE) links 
(802.3az). It uses real data traces as input in order to give the power save
which can be achieved. Of course the code can be further modified and use other
types of traffic such as Poisson, CBR, etc.
The main testing scenario is implemented in "scenario_File.cc" file. It sets up
the basic EEE parameters and then it uses as input "current_trace_0" and 
"current_trace_1" for the two directions of EEE links. The traffic in each
directions flows independently of the other direction forming a full duplex
link. It is important to give the full path for the 2 trace files otherwise
ns-3 will compile but do nothing since it will not use the files. If the correct
path has been specified a few lines will appear showing the power save during
the excution.(in my case the path is "/media/3Tdisk/...")

How to compile
--------------
I give a few instuctions on what I did to compile the code and set it up.
# sudo ./waf --build-profile=optimized --enable-examples --enable-tests configure
# sudo ./waf 

More information can be found on ns-3 tutorials.

How to run the scenario
-----------------------
You go into /scratch/ folder and you type

#sudo ./waf --run scenario_File


Enjoy!
