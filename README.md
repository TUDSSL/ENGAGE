# Battery-Free Game Boy

This is the official public repository for ENGAGE: the battery-free gaming console.
    
## Project Overview

### Rationale

Battery-free devices can [make phone calls](https://dl.acm.org/doi/abs/10.1145/3090090), [facilitate machine learning](https://dl.acm.org/doi/10.1145/3411808), or even [track eyes](https://doi.org/10.1145/3241539.3241578), but immersive interactive experience without batteries has not been explored so far. In this project we focus on this ignored part of the battery-free device ecosystem. We explore how reactive and user-centric applications, _mobile gaming_ in particular, can also be battery-free.	

### The Basic Problem of Gaming without Batteries: Power Failures 

<img src="https://github.com/TUDSSL/ENGAGE/blob/master/doc-images/intermittency.png" width="800px">

Powering small embedded computer only with energy harvesting would cause frequent power failures, resulting in a constant restart of a long-running application. Let us look at the above figure which symbolically represents an execution of a [Tetris](https://en.wikipedia.org/wiki/Tetris) game on a battery-free platform powered by harvested (fluctuating) energy. Without any protection against power failures the game would play until energy is lost (i.e. at line 185) and then would restart from the loading screen. [Intermittent computing techniques](https://drops.dagstuhl.de/opus/volltexte/2017/7131/pdf/LIPIcs-SNAPL-2017-8.pdf) would protect from this unwanted behaviour: after a power failure, line 186 would be executed proceeding from the exact system state as before the power failure. This way a game state will resume at the exact frame where the play ended due to power failure (in other words, [tetromino](https://en.wikipedia.org/wiki/Tetromino) will be in the same place when the game resumes).

### System Design: Game Boy Emulation and System State Checkpointing

<img src="https://github.com/TUDSSL/ENGAGE/blob/master/doc-images/system-engage.png" width="800px">

We designed a Nintendo Game Boy emulator, which we call _Energy Aware Gaming (ENGAGE)_ platform, as a proof of concept that interactive systems powered by only by (intermittently-available) energy harvesting is possible. ENGAGE is build around four key components (see also figure above):

1. **Game emulation:** Use processor emulation to play (Game Boy) retro games to resemble classic [Game Boy](https://en.wikipedia.org/wiki/Game_Boy) platform feel;
1. **Kernel:** Efficiently checkpoint minimal game state to the dedicated non-volatile memory to protect from a restart after a power failure, enabling continuous game play;
1. **Energy:** Power battery-free gaming console from gaming actions (button presses) and from ambient solar energy and store this energy in a small capacitor;
1. **Hardware:** Exploit leading (but off-the-shelf and accessible to everyone) [low-power 32-bit ARM Cortex M4 microcontroller](https://ambiq.com/apollo3-blue/) and [external FRAM](https://www.fujitsu.com/global/documents/products/devices/semiconductor/fram/lineup/MB85RS4MT-DS501-00053-1v0-E.pdf) to support checkpointing system.

### Hardware

<img src="https://github.com/TUDSSL/ENGAGE/blob/master/doc-images/hw-overview.png" width="800px">

ENGAGE internal PCB is depicted on the above figure (left-hand side: front, right-hand side: back) with the main components (A) [Ambiq Apollo3 Blue ARM Cortex-M4 MCU](https://ambiq.com/apollo3-blue/), (B) [Fujitsu MB85RS4MT 512 KB FRAM](https://www.fujitsu.com/global/documents/products/devices/semiconductor/fram/lineup/MB85RS4MT-DS501-00053-1v0-E.pdf), (C) [ZF AFIG-0007 energy harvesting switch](https://nl.mouser.com/datasheet/2/833/Energy_Harvesting_Generator_US-1075748.pdf), (D) [Semiconductor Components Industries NSR1030QMUTWG low forward voltage diode bridge](https://www.onsemi.com/pub/Collateral/NSR1030QMU-D.PDF), (E) micro USB debugging port, (F) display connector, (G) solar panels connector, (H) cartridge interface, (I) [Texas Instruments BQ25570 harvester/power manager](https://www.ti.com/lit/ds/symlink/bq25570.pdf), and (J) [Texas Instruments TPS61099 boost converter](https://www.ti.com/lit/ds/symlink/tps61099.pdf).

The specific internals of ENGAGE are as follows.

**Microcontroller:** ENGAGE is based on [ARM MCU architecture](https://en.wikipedia.org/wiki/ARM_architecture), a departure from [Texas Instruments MSP430 FRAM series](https://www.ti.com/lit/an/slaa498b/slaa498b.pdf) microcontrollers typically used in [intermittent computing systems](https://drops.dagstuhl.de/opus/volltexte/2017/7131/pdf/LIPIcs-SNAPL-2017-8.pdf). Specifically, we have used [Ambiq Apollo3 Blue ARM Cortex-M4](https://ambiq.com/apollo3-blue/) (operating at a clock frequency of 96 MHz) chosen for its best-in-class energy efficiency. Apollo3 MCU (marked as (A) in the above figure) runs the Game Boy emulator and [checkpointing software](https://github.com/TUDSSL/ENGAGE/tree/master/software).

**Memory:** A fast, byte-addressable, non-volatile memory is crucial to enable saving and restoring ENGAGE state (in other words, game state) in spite of power failures. To enable this, we equipped ENGAGE with an external dedicated FRAM IC: [Fujitsu MB85RS4MT 512 KB FRAM](https://www.fujitsu.com/global/documents/products/devices/semiconductor/fram/lineup/MB85RS4MT-DS501-00053-1v0-E.pdf), marked as (B) in the above figure, connected through SPI to the MCU providing a fast (enough) and durable method of non-volatile storage for checkpoints.

**Energy Harvesting:** We extract energy from two sources: (i) button presses, using six mechanical off-the-shelf [ZF AFIG-0007 energy harvesting switches](https://nl.mouser.com/datasheet/2/833/Energy_Harvesting_Generator_US-1075748.pdf) - four switches at the D-pad and one switch per "A" and "B" button (see (C) in the above figure), and (ii) a set of [Panasonic AM-1417CA amorphous silicon solar cells](https://www.panasonic-electric-works.com/cps/rde/xbcr/pew_eu_en/ca_amorton_solar_cells_en.pdf) attached to the front of the Game Boy chassis. Solar panels are managed by [Texas Instruments BQ25570 harvester/power manager](https://www.ti.com/lit/ds/symlink/bq25570.pdf). Harvested energy is stored in a 3.3 mF capacitor.

**Cartridges:** To be able to read Game Boy game cartridges, a cartridge connector is placed on the back of the ENGAGE platform PCB (marked as (H) in the above figure). The cartridge interfaces with the MCU using [Semtech SX1503 I/O expanders](https://www.mouser.com/datasheet/2/761/sx150x-1283306.pdf). This extender translates the 3V system voltage to 5V logic required by the cartridge.

**Display:** It was difficult to source a large display that had reasonable power requirements, a decent refresh rate, and minimal external driving circuitry. We quickly discarded the use of electronic ink displays because of its slow refresh rate and complex driving circuitry. Instead we use a 26.02 × 27.82 mm [Japan Display LPM013M126A LCD](https://www.j-display.com/product/pdf/Datasheet/4LPM013M126A_specification_Ver02.pdf). The display is is smaller than the original Game Boy screen (47 × 43 mm) but has a better resolution of 176 × 176 pixels compared to original 160 × 144 pixel matrix.

### Checkpointing

The core software component of ENGAGE is differential checkpointing library called _MPatch_ located in [software/libs/mpatch](https://github.com/TUDSSL/ENGAGE/blob/master/software/libs/mpatch) folder of this repository. Further description of MPatch is provided in a separate [document](https://github.com/TUDSSL/ENGAGE/blob/master/software/libs/mpatch/README.md).

## Getting Started

This repository contains everything required to build your own Battery-Free Game Boy. Below we describe how to build your own ENGAGE hardware and software.

### Building the Hardware

Hardware-related sources are stored in the following folders.

 - [/enclosure](https://github.com/TUDSSL/ENGAGE/tree/master/enclosure): Complete set of [3D STL files](https://en.wikipedia.org/wiki/STL_%28file_format%29) of the complete ENGAGE enclosure (front, back, buttons, D-pad, LCD holder)
 - [/hardware/main-board](https://github.com/TUDSSL/ENGAGE/tree/master/hardware/main-board): Main ENGAGE board [KiCad](https://kicad.org/) files. Specifically, the folder contains `.gbr` Gerber output files for manufacturing, `.drl` drill file for manufacturing, schematics of the PCB in `.pdf` format, `.csv` file with bill of materials and the KiCad project files `.kicad_pcb`, `.pro` and `.sch`.
 - [/hardware/apollo-3-module](https://github.com/TUDSSL/ENGAGE/tree/master/hardware/apollo-3-module): As above, but for the de-attachable board with the MCU and FRAM (see above figure - component (A) and (B))
 
 Enclosure can be printed using your favorite 3D printer or you can send all [/enclosure](https://github.com/TUDSSL/ENGAGE/tree/master/enclosure) files to one of online-available 3D printing services. As for the PCBs, you can either assemble everything yourself or send all [/hardware](https://github.com/TUDSSL/ENGAGE/tree/master/hardware/) files to many online-available PCB assembly companies for production. 

### Building the Software

Software-related sources are stored in the following folders.

- [/software/apps](https://github.com/TUDSSL/ENGAGE/tree/master/software/apps): Applications' source code folder containing three applications:
    - [emulator](https://github.com/TUDSSL/ENGAGE/tree/master/software/apps/emulator): main application of ENGAGE emulator, which can be also used to reproduce [results of the paper](#How-to-Cite-This-Work);
    - [checkpoint](https://github.com/TUDSSL/ENGAGE/tree/master/software/apps/checkpoint): application to test the performance of checkpointing mechanism;
    - [memtracker](https://github.com/TUDSSL/ENGAGE/tree/master/software/apps/memtracker): application to test the performance of memory use tracking mechanism;
- [/software/config](https://github.com/TUDSSL/ENGAGE/tree/master/software/config): Various configuration files.
- [/software/external](https://github.com/TUDSSL/ENGAGE/tree/master/software/external): External dependencies to the project, such as MCU SDK.
- [/software/libs](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs): Set of core libraries handling I/O (that is [console buttons](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs/buttons), [game cartridge](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs/cartridge), [console display](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs/display), [Game Boy emulator](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs/emulator), [FRAM access](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs/fram)) and checkpointing (that is [just-in-time checkpoint](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs/jit), [memory state tracker](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs/memtracker), and [MPatch checkpointing](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs/mpatch)).
- [/software/platforms](https://github.com/TUDSSL/ENGAGE/tree/master/software/platforms): GPIO pin mappings.
- [/software/scripts](https://github.com/TUDSSL/ENGAGE/tree/master/software/scripts): Scripts pertaining to the build process.

#### Note on the Game Boy Emulator

We do not provide the copy of the Game Boy emulator in this repository. Our build system (using [/software/scripts](https://github.com/TUDSSL/ENGAGE/tree/master/software/scripts)) automatically downloads and patches (applying optimizations and improvements) an external, not written by us, Game Boy emulator. Specifically, we have used [Game Boy emulator](https://mikrocontroller.bplaced.net/wordpress/?page_id=1290) for [STM32F746 Discovery board](https://www.st.com/en/evaluation-tools/32f746gdiscovery.html) written by [Uwe Becker](https://mikrocontroller.bplaced.net/wordpress/?page_id=8).

#### Build Process

After cloning this repository to your local computer the following instructions can be used to generate the ENGAGE binaries. From the project directory execute the following.

```bash
$ cd software/build
$ cmake ..
$ make install

```

Then using your favorite debugger (we have used one of [SEGGER J-Link debug probes](https://www.segger.com/products/debug-probes/j-link/models/model-overview/) for this project) the ENGAGE binaries (stored in the [/software/bin](https://github.com/TUDSSL/ENGAGE/tree/master/software/bin) folder) can be uploaded to the MCU using a debugger whilst powering the platform using USB. The main binary is called `emulator` whilst the others provide a means to reproduce the results obtained in the [paper](#How-to-Cite-This-Work). After this step a [game cartridge can be loaded](#Load-Game-Cartridge).

#### Last Tested Software Versions

This software was build successfully using the following software versions.

- [CMake](https://cmake.org/download/) version 3.18.4
- [arm-none-eabi-gcc](https://gcc.gnu.org/) version 10.2.0
- [arm-none-eabi-newlib](https://www.sourceware.org/newlib/) version 3.3.0
- [arm-none-eabi-binutils](https://www.gnu.org/software/binutils/) version 2.35

## How to Operate ENGAGE

There are two important operations when using ENGAGE that are not related to normal gaming: **loading game cartridge** and **system reset**, both described below.

### Load Game Cartridge

After programming the [emulator binary](https://github.com/TUDSSL/ENGAGE/tree/master/software/bin) to ENGAGE, a Game Boy cartridge with your favorite game needs to be loaded to ENGAGE. To load a cartridge, first put the physical cartridge in the cartridge slot (at the back of ENGAGE) and then hold down **Start** and **A** buttons on ENGAGE while powering ENGAGE through the USB port. You can let go **Start** and **A** buttons after a few seconds. When loading of the cartridge has completed the game will start. Note that there is currently [no progress indication](https://github.com/TUDSSL/ENGAGE/issues/4) of this operation on the screen of ENGAGE. Remember that the process can take five minutes (or more) for large cartridges! After a cartridge has been loaded it can be removed from the device.

### System Reset

Due to the unique nature of ENGAGE's state resuming after power failure, resetting ENGAGE requires a unique key combination (unlike the original Game Boy that you could turn off and on again easily). This reset is performed by holding **Start** and **Select** buttons while powering ENGAGE through an USB port. Please note that this reset operation does *not* require you to reload the game cartridge.

## Frequently Asked Questions

- **When the energy source is changed from solar to button-mashing, how long is ENGAGE temporarily off?**

We actually harvest from both energy sources at the same time! We developed a circuit that manages this internally. However, this still means that the user will experience interruptions of play, that can vary between sub-second (in extreme well outdoor light conditions) to many seconds (on a gloomy day, indoors). While the button presses generate extra energy to sustain game play, most of the energy is still generated by solar panels. More discussion on this can be found in our [research paper](#How-to-Cite-This-Work). 

- **How do the buttons generate power?**

The buttons generate power by moving a small but powerful magnet inside a tightly wound wire coil. The change in the magnetic field generates power. When you press the button (and when you release) it moves the magnet through the coil, this energy is then siphoned into a capacitor for immediate use by the ENGAGE hardware to support all activities. Because of advances in manufacturing over the past decade the magnet and coil are so small that they can fit inside a button that is acceptable to a user.

- **Why is the screen so small?**

Yes, the screen is smaller that in the original Game Boy (which is a [source of complain](https://www.reddit.com/r/gadgets/comments/in3aco/batteryfree_game_boy_runs_off_of_solar_and/) of many internauts), but the one we [used in the initial ENGAGE design](https://www.j-display.com/product/pdf/Datasheet/4LPM013M126A_specification_Ver02.pdf) the best one we could find in terms of resolution and energy consumption. If you have a better screen to suggest, then make a new [project issue](#How-to-Contribute-to-this-Project) and send us a [pull request](#How-to-Contribute-to-this-Project).

- **Can ENGAGE play sound?**

Unfortunately not. Sound generation costs extra energy and as we operate on a very thin energy budget we had to make choices which features of original Game Boy had to be excluded from ENGAGE.

- **Can I buy one?**

Unfortunately not. But you can build one! All hardware and software of ENGAGE, together with the process of how to make it run is simply in _this repository_. Please remember that ENGAGE **was not (and is not meant to be) a product we can sell**. This is a research project and a concept demonstration.

## How to Contribute to this Project

We look forward to your contributions, improvements, additions and changes. Please follow the standard [GitHub flow](https://docs.github.com/en/free-pro-team@latest/github/collaborating-with-issues-and-pull-requests/github-flow) for code contributions. In macro terms this means the following.

1. [Fork the `master` branch](https://docs.github.com/en/free-pro-team@latest/github/getting-started-with-github/fork-a-repo#keep-your-fork-synced)  of this repository; make sure that your fork will be [up to date](https://docs.github.com/en/free-pro-team@latest/github/getting-started-with-github/fork-a-repo#keep-your-fork-synced) with the latest `master` branch.
1. Create an issue [here](https://github.com/TUDSSL/ENGAGE/issues) with a new feature or a bug report.
1. Perform changes on your local branch and [push them to your forked clone](https://docs.github.com/en/free-pro-team@latest/github/collaborating-with-issues-and-pull-requests/merging-an-upstream-repository-into-your-fork).
1. Create a [pull request](https://docs.github.com/en/free-pro-team@latest/github/collaborating-with-issues-and-pull-requests/creating-a-pull-request) referencing the issue it covers and wait for our response.

### List of Known Issues

List of all known issues is listed in the [Issues](https://github.com/TUDSSL/ENGAGE/issues) list of this project. If you found a bug or you would like to enhance ENGAGE: [please contribute](#How-to-Contribute-to-this-Project)! We look forward to your additions. Let ENGAGE grow and become a better and **more sustainable** gaming experience.

## How to Cite This Work

The results of this project have been published in a peer-reviewed academic publication (from which all technical figures in this file originate). Details of the publication are as follows.

* **Authors and the project team:** [Jasper de Winkel](https://jasperdewinkel.com/), [Vito Kortbeek](https://www.vitokortbeek.com/), [Josiah Hester](https://josiahhester.com/), [Przemysław Pawełczak](http://www.pawelczak.net/)
* **Publication title:** _Battery-Free Game Boy_
* **Pulication venue:** [Proceedings of the ACM on Interactive, Mobile, Wearable and Ubiquitous Technologies, Volume 4, Issue 3, September 2020](https://dl.acm.org/toc/imwut/2020/4/3) and [Proceedings of ACM UbiComp 2020](https://ubicomp.org/ubicomp2020/)
* **Link to publication:** https://doi.org/10.1145/3411839 (Open Access)
* **Link to ACM UbiComp 2020 conference presentation video:** https://www.youtube.com/watch?v=XxjrTHBFBSM

To cite this publication please use the following BiBTeX entry.

```
@article{dewinkel:imwut:2020:gameboy,
  title = {Battery-Free Game Boy},
  author = {Jasper {de Winkel} and Vito {Kortbeek} and Josiah {Hester} and Przemys{\l}aw {Pawe{\l}czak}},
  journal = {Proc. ACM Interact. Mob. Wearable Ubiquitous Technol.},
  volume = {4},
  number = {3},
  pages = {111:1--111:34},
  year = {2020},
  publisher = {ACM}
}
```

## Related Websites

* **Project source code:** https://github.com/tudssl/engage (this repository)
* **Project announcement website:** https://www.freethegameboy.info

## The Press

The project has been covered extensively by international media. Here is the list of example posts and articles (as of October 30, 2020).

[CNET](https://www.cnet.com/features/the-first-battery-free-game-boy-wants-to-power-a-gaming-revolution/), [The Wall Street Journal](https://www.wsj.com/articles/battery-free-energy-harvesting-perpetual-machines-the-weird-future-of-computing-11601697600?st=xyjz99iomamhwr6&reflink=article_copyURL_share), [Mashable](https://mashable.com/video/battery-free-gameboy/), [Hackaday](https://hackaday.com/2020/09/11/game-boy-plays-forever/), [The Verge](https://www.theverge.com/tldr/2020/9/4/21422605/engage-battery-free-game-boy-button-mashing-solar-panels-research-environmental-sustainability), [Gizmodo](https://earther.gizmodo.com/this-battery-free-game-boy-is-the-first-step-toward-ens-1844953062), [Engadget](https://www.engadget.com/engage-game-boy-211023869.html), [PCMag](https://www.pcmag.com/news/solar-powered-game-boy-provides-infinite-play), [The Register](https://www.theregister.com/2020/09/04/battery_free_gameboy/), [Tech Times](https://www.techtimes.com/articles/252268/20200903/game-boy-gaming-console-battery-free-play-want-without-recharging.htm), [Nintendo Life](https://www.nintendolife.com/news/2020/09/this_battery-free_game_boy_could_be_the_future_of_portable_tech), [Daily Mail](https://www.dailymail.co.uk/sciencetech/article-8695791/Battery-free-Game-Boy-charged-MILLIONS-times-sun-pressing-buttons.html), [The Independent](https://www.independent.co.uk/life-style/gadgets-and-tech/news/game-boy-battery-free-technology-renewable-power-a9705256.html), [r/gadgets](https://www.reddit.com/r/gadgets/comments/in3aco/batteryfree_game_boy_runs_off_of_solar_and/)

<a href="https://www.youtube.com/watch?v=5VzDyvwfEZA"><img src="https://github.com/TUDSSL/ENGAGE/blob/master/doc-images/youtube_thumbnail.png" width="300px"></a>

## Acknowledgments

This research project was supported by [Netherlands Organisation for Scientific Research](https://www.nwo.nl/en), partly funded by the [Dutch Ministry of Economic Affairs and Climate Policy](https://www.government.nl/ministries/ministry-of-economic-affairs-and-climate-policy), through [TTW Perspective program ZERO (P15-06)](https://www.nwo.nl/en/researchprogrammes/perspectief/previous-awards/zero-towards-energy-autonomous-systems-iot) within Project P1 and P4, and by the [National Science Foundation](https://www.nsf.org/) through grants [CNS-1850496](https://www.nsf.gov/awardsearch/showAward?AWD_ID=1850496) and [CNS-2032408](https://www.nsf.gov/awardsearch/showAward?AWD_ID=2032408). Any opinions, findings, and conclusions or recommendations expressed in this material are those of the author(s) and do not necessarily reflect the views of the National Science Foundation.

<a href="https://www.northwestern.edu"><img src="https://github.com/TUDSSL/ENGAGE/blob/master/doc-images/northwestern_logo.jpg" width="300px"></a><a href="https://www.tudelft.nl"><img src="https://github.com/TUDSSL/ENGAGE/blob/master/doc-images/tudelft_logo.png" width="300px"></a>

## Copyright

Copyright (C) 2020 TU Delft Embedded and Networked Systems Group/Sustainable Systems Laboratory.

MIT License or otherwise specified. See [license](https://github.com/TUDSSL/ENGAGE/blob/master/LICENSE) file for details.
