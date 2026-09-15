# Orca-Flashforge
Orca-Flashforge is an open source slicer for FDM printers.

# How to compile
- Windows 64-bit  
  - Tools needed: Visual Studio 2022, CMake, Git, Strawberry Perl.
  - Run `build_release.bat` in `x64 Native Tools Command Prompt for VS 2022`

- Mac 64-bit  
  - Tools needed: Xcode, CMake, Git, gettext, Automake, Perl
  - run `build_release_macos.sh`

- Linux (checked on Ubuntu Desktop 22.04 6.8.0-57-generic x86_64)  
  - Tools needed: [docker](https://docs.docker.com/engine/install/ubuntu/)
  - run `sh ./DockerBuild.sh`
  - after successful build run it from docker via `sh DockerRun.sh` or export AppImage from `orcaslicer` container to your local file system

<!-- ai-template:narrative:start -->
## The Totally True and Not At All Embellished History of Orca-Flashforge

### The continuing adventures of Jordan Hubbard and Sir Reginald von Fluffington III

> *Part 20 of an ongoing chronicle. [← Part 19: c26](https://github.com/jordanhubbard/c26#the-totally-true-and-not-at-all-embellished-history-of-c26) | [Part 21: OrcaSlicer →](https://github.com/jordanhubbard/OrcaSlicer#the-totally-true-and-not-at-all-embellished-history-of-orcaslicer)*
> *[Chronicle index](https://github.com/jordanhubbard/ai-template/blob/main/CHRONICLE.md) · Ordered by first recorded AI-assisted commit.*

The programmer wished to print an object. Before this could happen, he needed to build the slicer.

Sir Reginald von Fluffington III considered the printer a box that became warm and made unnecessarily complicated noises. He was willing to inspect its output, provided the output fitted under a sofa.

This chapter concerns the programmer's fork of Orca-Flashforge. The slicer already had a substantial ancestry, carefully acknowledged elsewhere in this README. The programmer had not invented it. He had encountered the sort of build failure that can make a person feel personally responsible for every dependency in the family tree.

Newer macOS tools and CMake met older build assumptions. Fixing the top-level invocation was not enough if a dependency configured another dependency with its own ideas about policy. The programmer followed the settings through the build, explaining each layer to a cat who had developed a much shorter dependency chain: programmer, tin opener, bowl.

There were practical improvements too. Dark-mode text needed to remain readable. Individual filaments needed ironing overrides. A Klipper-modified Adventurer 5M Pro needed appropriate profiles and a working route to its printer services. These were the details that separated a slicer that existed from a slicer useful at the programmer's desk.

Then an AI-assisted shape-generation interface found its way into the fork. The ambition was now to describe geometry, obtain a model, and put it on the plate. Sir Reginald had described geometry for years by knocking objects over until their orientation satisfied him.

Even branding had consequences. Changing an application name or configuration key could strand settings that had worked before. Compatibility required more care than selecting a pleasant title for the window.

“At last,” the programmer said, “we can get back to printing.”

Sir Reginald watched the empty build plate. The original object had not yet become interesting enough to chase. He withheld endorsement, citing insufficient tuna, dependency policy, and a manufacturing process that offered him neither warmth nor a box until everyone stopped compiling.

<!-- ai-template:narrative:end -->

# License
Orca-Flashforge is licensed under the GNU Affero General Public License, version 3. Orca-Flashforge is based on Orca Slicer by SoftFever.

Orca Slicer is licensed under the GNU Affero General Public License, version 3. Orca Slicer is based on Bambu Studio by BambuLab.

Bambu Studio is licensed under the GNU Affero General Public License, version 3. Bambu Studio is based on PrusaSlicer by PrusaResearch.

PrusaSlicer is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.

Slic3r is licensed under the GNU Affero General Public License, version 3. Slic3r was created by Alessandro Ranellucci with the help of many other contributors.

The GNU Affero General Public License, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.

Orca-Flashforge includes a pressure advance calibration pattern test adapted from Andrew Ellis' generator, which is licensed under GNU General Public License, version 3. Ellis' generator is itself adapted from a generator developed by Sineos for Marlin, which is licensed under GNU General Public License, version 3.

The flashforge networking plugin is based on non-free libraries from FlashForge. It is optional to the Orca-Flashforge and provides extended functionalities for FlashForge printer users.
