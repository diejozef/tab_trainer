# tab_trainer
A very basic trainer for the game [They Are Billions](https://store.steampowered.com/app/644930/They_Are_Billions/).

Currently supported version: **v0.9.1 (64bit)**
## Functionality
* [F1] - cap resources(max out resources)
* [F2] - veteran training(make every freshly trained unit veteran)
* [F3] - fast building(train/repair/build/upgrade units in 1 second)
* [F4] - fast research(research technologies in 1 second)
## Usage and compiling
The game uses JIT compilation which means some functions might not be compiled at game start. 
Active trainer features only after making sure the needed function is compiled(e.g. train a unit before activating veteran training or start researching a technology before activating fast research).

Compile for x64.
